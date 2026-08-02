#include <clipcoach/storage/clip-library-service.hpp>
#include <clipcoach/storage/clip-repository.hpp>
#include <clipcoach/storage/database-recovery.hpp>
#include <clipcoach/storage/export-job-repository.hpp>
#include <clipcoach/storage/session-repository.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <exception>
#include <future>
#include <mutex>
#include <set>
#include <thread>
#include <utility>

namespace clipcoach::storage {
namespace {

std::string exportedVariantId(const ExportJob &job)
{
	return job.request.clipId + "-variant-vertical-" + ExportManager::cleanBaseName(job.outputPath.stem().string());
}

int exportedDurationSeconds(const ExportJob &job)
{
	if (!job.request.keepSegments.empty()) {
		long long durationMilliseconds = 0;
		for (const auto &segment : job.request.keepSegments)
			durationMilliseconds += segment.durationMilliseconds;
		return std::max(1, static_cast<int>((durationMilliseconds + 999) / 1000));
	}
	if (job.request.trimDurationMilliseconds > 0)
		return std::max(1, static_cast<int>((job.request.trimDurationMilliseconds + 999) / 1000));
	return std::max(job.request.durationSeconds, 1);
}

StorageStatus removeLocalFile(const std::filesystem::path &path)
{
	if (path.empty()) {
		return StorageStatus::ok();
	}
	std::error_code error;
	const auto status = std::filesystem::symlink_status(path, error);
	if (error) {
		if (error == std::errc::no_such_file_or_directory)
			return StorageStatus::ok();
		return StorageStatus::fail("could not inspect local file " + path.u8string() + ": " + error.message());
	}
	if (!std::filesystem::exists(status)) {
		return StorageStatus::ok();
	}
	if (!std::filesystem::is_regular_file(status) && !std::filesystem::is_symlink(status)) {
		return StorageStatus::fail("refusing to delete a non-file path: " + path.u8string());
	}
	if (!std::filesystem::remove(path, error) || error) {
		return StorageStatus::fail("could not delete local file " + path.u8string() + ": " + error.message());
	}
	return StorageStatus::ok();
}

} // namespace

class ClipLibraryService::Impl final {
public:
	using Task = std::function<void(SqliteDatabase &)>;

	Impl(std::filesystem::path databasePath, LogCallback logCallback)
		: databasePath_(std::move(databasePath)),
		  logCallback_(std::move(logCallback)),
		  worker_([this] { run(); })
	{
	}

	~Impl()
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			stopping_ = true;
		}
		condition_.notify_one();
		if (worker_.joinable()) {
			worker_.join();
		}
	}

	bool enqueue(Task task)
	{
		{
			std::lock_guard<std::mutex> lock(mutex_);
			if (stopping_) {
				return false;
			}
			tasks_.push_back(std::move(task));
		}
		condition_.notify_one();
		return true;
	}

	void log(bool error, const std::string &message) const
	{
		if (logCallback_) {
			logCallback_(error, message);
		}
	}

	const std::filesystem::path &databasePath() const noexcept { return databasePath_; }

private:
	void run()
	{
		SqliteDatabase database(databasePath_);
		const auto opened = openDatabaseWithRecovery(database);
		if (!opened.status.success) {
			log(true, "SQLite initialization failed: " + opened.status.error);
		} else {
			if (opened.recovered) {
				log(true, "Corrupt SQLite library preserved at " + opened.backupPath.string() +
						  "; using a clean fallback database");
			}
			log(false,
			    "SQLite clip library ready at schema version " + std::to_string(database.schemaVersion()));
		}

		while (true) {
			Task task;
			{
				std::unique_lock<std::mutex> lock(mutex_);
				condition_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
				if (stopping_ && tasks_.empty()) {
					break;
				}
				task = std::move(tasks_.front());
				tasks_.pop_front();
			}
			try {
				task(database);
			} catch (const std::exception &error) {
				log(true, "Unhandled clip library task error: " + std::string(error.what()));
			} catch (...) {
				log(true, "Unhandled unknown clip library task error");
			}
		}
	}

	std::filesystem::path databasePath_;
	LogCallback logCallback_;
	std::mutex mutex_;
	std::condition_variable condition_;
	std::deque<Task> tasks_;
	bool stopping_{false};
	std::thread worker_;
};

ClipLibraryService::ClipLibraryService(std::filesystem::path databasePath, LogCallback logCallback)
	: impl_(std::make_unique<Impl>(std::move(databasePath), std::move(logCallback)))
{
}

ClipLibraryService::~ClipLibraryService() = default;

void ClipLibraryService::storeSession(SessionMetadata session, StatusCallback callback)
{
	const auto queued = impl_->enqueue([session = std::move(session), callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		SessionRepository repository(database);
		auto status = repository.insert(session);
		if (!status.success) {
			impl->log(true, "SessionRepository insert failed: " + status.error);
		}
		if (callback) {
			callback(std::move(status));
		}
	});
	if (!queued && callback) {
		callback(StorageStatus::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::endSession(std::string sessionId, std::chrono::system_clock::time_point endedAt,
				    StatusCallback callback)
{
	const auto queued = impl_->enqueue([sessionId = std::move(sessionId), endedAt, callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		SessionRepository repository(database);
		auto status = repository.endSession(sessionId, endedAt);
		if (!status.success) {
			impl->log(true, "SessionRepository end failed: " + status.error);
		}
		if (callback) {
			callback(std::move(status));
		}
	});
	if (!queued && callback) {
		callback(StorageStatus::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::storeClip(ClipMetadata clip, StatusCallback callback)
{
	const auto queued = impl_->enqueue([clip = std::move(clip), callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		ClipRepository repository(database);
		auto status = repository.insert(clip);
		if (!status.success) {
			impl->log(true, "ClipRepository insert failed: " + status.error);
		}
		if (callback) {
			callback(std::move(status));
		}
	});
	if (!queued && callback) {
		callback(StorageStatus::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::updateThumbnail(std::string clipId, std::filesystem::path thumbnailPath,
					 StatusCallback callback)
{
	const auto queued =
		impl_->enqueue([clipId = std::move(clipId), thumbnailPath = std::move(thumbnailPath),
				callback = std::move(callback), impl = impl_.get()](SqliteDatabase &database) mutable {
			ClipRepository repository(database);
			auto status = repository.updateThumbnail(clipId, thumbnailPath);
			if (!status.success)
				impl->log(true, "ClipRepository thumbnail update failed: " + status.error);
			if (callback)
				callback(std::move(status));
		});
	if (!queued && callback)
		callback(StorageStatus::fail("clip library is shutting down"));
}

void ClipLibraryService::storeAiResult(AiPersistedClipResult result, StatusCallback callback)
{
	const auto queued = impl_->enqueue([result = std::move(result), callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		ClipRepository repository(database);
		auto status = repository.updateAiResult(result);
		if (!status.success)
			impl->log(true, "AI clip metadata persistence failed: " + status.error);
		if (callback)
			callback(std::move(status));
	});
	if (!queued && callback)
		callback(StorageStatus::fail("clip library is shutting down"));
}

void ClipLibraryService::storeSessionAiSummary(std::string sessionId, std::string summary, AiLanguage language,
					       StatusCallback callback)
{
	const auto queued =
		impl_->enqueue([sessionId = std::move(sessionId), summary = std::move(summary), language,
				callback = std::move(callback), impl = impl_.get()](SqliteDatabase &database) mutable {
			SessionRepository repository(database);
			auto status = repository.updateAiSummary(sessionId, summary,
								 AiAssistantService::languageCode(language));
			if (!status.success)
				impl->log(true, "AI session summary persistence failed: " + status.error);
			if (callback)
				callback(std::move(status));
		});
	if (!queued && callback)
		callback(StorageStatus::fail("clip library is shutting down"));
}

void ClipLibraryService::setFavorite(std::string clipId, bool favorite, StatusCallback callback)
{
	const auto queued = impl_->enqueue([clipId = std::move(clipId), favorite, callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		ClipRepository repository(database);
		auto status = repository.setFavorite(clipId, favorite);
		if (!status.success) {
			impl->log(true, "ClipRepository favorite update failed: " + status.error);
		}
		if (callback) {
			callback(std::move(status));
		}
	});
	if (!queued && callback) {
		callback(StorageStatus::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::deleteClips(std::vector<std::string> clipIds, StatusCallback callback)
{
	if (clipIds.empty()) {
		if (callback) {
			callback(StorageStatus::fail("select at least one clip to delete"));
		}
		return;
	}
	const auto queued = impl_->enqueue([clipIds = std::move(clipIds), callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		ClipRepository repository(database);
		std::set<std::filesystem::path> removedPaths;
		for (const auto &clipId : clipIds) {
			const auto found = repository.findById(clipId);
			if (!found.success || !found.value.has_value()) {
				auto status = StorageStatus::fail(found.success ? "clip was not found" : found.error);
				impl->log(true, "Clip deletion failed: " + status.error);
				if (callback)
					callback(std::move(status));
				return;
			}

			const auto &clip = *found.value;
			for (const auto &path :
			     {clip.filePath, clip.thumbnailPath, clip.transcriptPath, clip.subtitlePath}) {
				if (path.empty() || !removedPaths.insert(path).second)
					continue;
				const auto fileStatus = removeLocalFile(path);
				if (!fileStatus.success) {
					impl->log(true, "Clip deletion failed: " + fileStatus.error);
					if (callback)
						callback(fileStatus);
					return;
				}
			}

			const auto status = repository.remove(clipId);
			if (!status.success) {
				impl->log(true, "ClipRepository delete failed: " + status.error);
				if (callback)
					callback(status);
				return;
			}
		}
		impl->log(false, "Deleted " + std::to_string(clipIds.size()) + " clip(s) and their local files");
		if (callback)
			callback(StorageStatus::ok());
	});
	if (!queued && callback) {
		callback(StorageStatus::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::storeExportJob(ExportJob job, StatusCallback callback)
{
	const auto queued = impl_->enqueue([job = std::move(job), callback = std::move(callback),
					    impl = impl_.get()](SqliteDatabase &database) mutable {
		ExportJobRepository exports(database);
		ClipRepository clips(database);
		auto status = exports.record(job);
		if (status.success) {
			ExportStatus clipStatus = ExportStatus::Pending;
			switch (job.state) {
			case ExportJobState::Pending:
			case ExportJobState::Cancelled:
				clipStatus = ExportStatus::Pending;
				break;
			case ExportJobState::Exporting:
				clipStatus = ExportStatus::Exporting;
				break;
			case ExportJobState::Done:
				clipStatus = ExportStatus::Exported;
				break;
			case ExportJobState::Error:
				clipStatus = ExportStatus::Failed;
				break;
			}
			status = clips.setExportStatus(job.request.clipId, clipStatus);
		}
		if (status.success && job.state == ExportJobState::Done &&
		    job.orientation == ExportOrientation::Vertical) {
			const auto source = clips.findById(job.request.clipId);
			if (!source.success) {
				status = StorageStatus::fail(source.error);
			} else if (source.value.has_value()) {
				ClipMetadata vertical = *source.value;
				vertical.id = exportedVariantId(job);
				vertical.filePath = job.outputPath;
				vertical.fileName = job.outputPath.filename().u8string();
				vertical.createdAt = job.updatedAt;
				vertical.durationSeconds = exportedDurationSeconds(job);
				vertical.orientation = ClipOrientation::Vertical;
				vertical.isFavorite = false;
				vertical.exportStatus = ExportStatus::Exported;
				vertical.thumbnailPath.clear();
				const auto existing = clips.findById(vertical.id);
				if (!existing.success) {
					status = StorageStatus::fail(existing.error);
				} else if (!existing.value.has_value()) {
					status = clips.insert(vertical);
				}
			}
		}
		if (!status.success) {
			impl->log(true, "Export metadata persistence failed: " + status.error);
		}
		if (callback) {
			callback(std::move(status));
		}
	});
	if (!queued && callback) {
		callback(StorageStatus::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::listRecentAsync(int limit, ClipsCallback callback)
{
	if (!callback) {
		return;
	}
	auto sharedCallback = std::make_shared<ClipsCallback>(std::move(callback));
	const auto queued = impl_->enqueue([limit, sharedCallback](SqliteDatabase &database) {
		ClipRepository repository(database);
		(*sharedCallback)(repository.listRecent(limit));
	});
	if (!queued) {
		(*sharedCallback)(RepositoryResult<std::vector<ClipMetadata>>::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::listBySessionAsync(std::string sessionId, ClipsCallback callback)
{
	if (!callback) {
		return;
	}
	auto sharedCallback = std::make_shared<ClipsCallback>(std::move(callback));
	const auto queued =
		impl_->enqueue([sessionId = std::move(sessionId), sharedCallback](SqliteDatabase &database) {
			ClipRepository repository(database);
			(*sharedCallback)(repository.listBySession(sessionId));
		});
	if (!queued) {
		(*sharedCallback)(RepositoryResult<std::vector<ClipMetadata>>::fail("clip library is shutting down"));
	}
}

void ClipLibraryService::loadSnapshotAsync(std::string sessionId, int recentLimit, SnapshotCallback callback)
{
	if (!callback) {
		return;
	}
	auto sharedCallback = std::make_shared<SnapshotCallback>(std::move(callback));
	const auto queued = impl_->enqueue([sessionId = std::move(sessionId), recentLimit,
					    sharedCallback](SqliteDatabase &database) {
		ClipRepository clips(database);
		SessionRepository sessions(database);
		auto clipResult = sessionId.empty() ? clips.listRecent(recentLimit) : clips.listBySession(sessionId);
		if (!clipResult.success) {
			(*sharedCallback)(RepositoryResult<ClipLibrarySnapshot>::fail(clipResult.error));
			return;
		}

		ClipLibrarySnapshot snapshot;
		snapshot.clips = std::move(clipResult.value);
		if (!sessionId.empty()) {
			auto sessionResult = sessions.findById(sessionId);
			if (!sessionResult.success) {
				(*sharedCallback)(RepositoryResult<ClipLibrarySnapshot>::fail(sessionResult.error));
				return;
			}
			snapshot.session = std::move(sessionResult.value);
		}
		(*sharedCallback)(RepositoryResult<ClipLibrarySnapshot>::ok(std::move(snapshot)));
	});
	if (!queued) {
		(*sharedCallback)(RepositoryResult<ClipLibrarySnapshot>::fail("clip library is shutting down"));
	}
}

StorageStatus ClipLibraryService::flush()
{
	auto promise = std::make_shared<std::promise<StorageStatus>>();
	auto future = promise->get_future();
	if (!impl_->enqueue([promise](SqliteDatabase &database) {
		    promise->set_value(database.isOpen() ? StorageStatus::ok()
							 : StorageStatus::fail("clip library database is unavailable"));
	    })) {
		return StorageStatus::fail("clip library is shutting down");
	}
	return future.get();
}

const std::filesystem::path &ClipLibraryService::databasePath() const noexcept
{
	return impl_->databasePath();
}

} // namespace clipcoach::storage
