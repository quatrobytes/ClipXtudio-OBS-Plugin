#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <utility>

namespace clipcoach {
namespace {

std::atomic<unsigned long long> nextJobId{1};

const char *suffix(ExportOrientation orientation)
{
	return orientation == ExportOrientation::Vertical ? "vertical" : "horizontal";
}

bool pathReserved(const std::vector<std::filesystem::path> &paths, const std::filesystem::path &candidate)
{
	return std::find(paths.begin(), paths.end(), candidate) != paths.end();
}

} // namespace

ExportManager::ExportManager(std::unique_ptr<ExportBackend> backend, JobCallback metadataCallback,
			     LogCallback logCallback, const FeatureGateService *featureGates)
	: backend_(std::move(backend)),
	  metadataCallback_(std::move(metadataCallback)),
	  logCallback_(std::move(logCallback)),
	  featureGates_(featureGates),
	  worker_([this] { run(); })
{
}

ExportManager::~ExportManager()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		stopping_ = true;
		for (auto &[id, flag] : cancelFlags_) {
			(void)id;
			flag->store(true);
		}
	}
	condition_.notify_all();
	if (worker_.joinable()) {
		worker_.join();
	}
}

std::vector<std::string> ExportManager::enqueue(ExportRequest request, std::string *error)
{
	if (error != nullptr) {
		error->clear();
	}
	if (!validate(request, error)) {
		return {};
	}
	if (featureGates_ != nullptr) {
		const auto feature =
			request.orientation == ExportOrientation::Both ? Feature::HorizontalAndVertical
			: request.verticalSource == VerticalExportSource::VerticalCanvas ? Feature::VerticalCanvas
			: request.orientation == ExportOrientation::Vertical ? Feature::LimitedVerticalExport
									     : Feature::BasicHorizontalExport;
		const auto decision = featureGates_->check(feature);
		if (!decision.allowed) {
			if (error != nullptr)
				*error = decision.message;
			return {};
		}
	}
	std::vector<std::string> result;
	if (request.orientation == ExportOrientation::Both) {
		result.push_back(addJob(request, ExportOrientation::Horizontal));
		result.push_back(addJob(request, ExportOrientation::Vertical));
	} else {
		result.push_back(addJob(request, request.orientation));
	}
	condition_.notify_one();
	return result;
}

std::vector<std::string> ExportManager::enqueueBatch(std::vector<ExportRequest> requests, std::string *error)
{
	if (error != nullptr) {
		error->clear();
	}
	if (featureGates_ != nullptr) {
		const auto decision = featureGates_->check(Feature::BatchExport);
		if (!decision.allowed) {
			if (error != nullptr)
				*error = decision.message;
			return {};
		}
	}
	for (const auto &request : requests) {
		if (!validate(request, error)) {
			return {};
		}
	}
	std::vector<std::string> result;
	for (const auto &request : requests) {
		if (request.orientation == ExportOrientation::Both) {
			result.push_back(addJob(request, ExportOrientation::Horizontal));
			result.push_back(addJob(request, ExportOrientation::Vertical));
		} else {
			result.push_back(addJob(request, request.orientation));
		}
	}
	condition_.notify_one();
	return result;
}

bool ExportManager::cancel(const std::string &jobId, std::string *error)
{
	if (error != nullptr) {
		error->clear();
	}
	ExportJob cancelledJob;
	bool publishCancellation = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const auto found = jobs_.find(jobId);
		if (found == jobs_.end()) {
			if (error != nullptr) {
				*error = "export job was not found";
			}
			return false;
		}
		if (found->second.state == ExportJobState::Done || found->second.state == ExportJobState::Error ||
		    found->second.state == ExportJobState::Cancelled) {
			if (error != nullptr) {
				*error = "export job is already finished";
			}
			return false;
		}
		cancelFlags_.at(jobId)->store(true);
		const auto queued = std::find_if(queue_.begin(), queue_.end(),
						 [&jobId](const auto &item) { return item.id == jobId; });
		if (queued != queue_.end()) {
			queue_.erase(queued);
			found->second.state = ExportJobState::Cancelled;
			found->second.error = "export cancelled";
			found->second.updatedAt = std::chrono::system_clock::now();
			cancelledJob = found->second;
			publishCancellation = true;
		}
	}
	if (publishCancellation) {
		publish(cancelledJob);
		idleCondition_.notify_all();
	}
	return true;
}

ExportJob ExportManager::job(const std::string &jobId) const
{
	std::lock_guard<std::mutex> lock(mutex_);
	const auto found = jobs_.find(jobId);
	return found != jobs_.end() ? found->second : ExportJob{};
}

std::vector<ExportJob> ExportManager::jobs() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	std::vector<ExportJob> result;
	result.reserve(jobs_.size());
	for (const auto &[id, item] : jobs_) {
		(void)id;
		result.push_back(item);
	}
	return result;
}

bool ExportManager::waitUntilIdle(std::chrono::milliseconds timeout)
{
	std::unique_lock<std::mutex> lock(mutex_);
	return idleCondition_.wait_for(lock, timeout, [this] { return queue_.empty() && !active_; });
}

void ExportManager::setJobCallback(JobCallback callback)
{
	std::lock_guard<std::mutex> lock(mutex_);
	metadataCallback_ = std::move(callback);
}

bool ExportManager::isValidPreset(ExportQualityPreset preset) noexcept
{
	const auto value = static_cast<int>(preset);
	return value >= 0 && value <= static_cast<int>(ExportQualityPreset::Maximum);
}

std::string ExportManager::cleanBaseName(const std::string &value)
{
	std::string result;
	result.reserve(value.size());
	bool previousSeparator = false;
	for (unsigned char character : value) {
		const bool accepted = std::isalnum(character) != 0 || character == '-' || character == '_';
		if (accepted) {
			result.push_back(static_cast<char>(character));
			previousSeparator = false;
		} else if (!result.empty() && !previousSeparator) {
			result.push_back('_');
			previousSeparator = true;
		}
	}
	while (!result.empty() && (result.back() == '_' || result.back() == '.')) {
		result.pop_back();
	}
	return result.empty() ? "clip" : result;
}

std::filesystem::path ExportManager::createAvailableOutputPath(const std::filesystem::path &directory,
							       const std::string &baseName,
							       ExportOrientation orientation)
{
	const auto stem = cleanBaseName(baseName) + "_" + suffix(orientation);
	auto candidate = directory / (stem + ".mp4");
	std::error_code error;
	for (int index = 2; std::filesystem::exists(candidate, error); ++index) {
		error.clear();
		candidate = directory / (stem + "_" + std::to_string(index) + ".mp4");
	}
	return candidate;
}

bool ExportManager::validate(const ExportRequest &request, std::string *error) const
{
	long long previousEnd = -1;
	bool segmentsValid = true;
	for (const auto &segment : request.keepSegments) {
		if (segment.startMilliseconds < 0 || segment.durationMilliseconds < 100 ||
		    (previousEnd >= 0 && segment.startMilliseconds < previousEnd)) {
			segmentsValid = false;
			break;
		}
		previousEnd = segment.startMilliseconds + segment.durationMilliseconds;
	}
	const bool orientationValid = static_cast<int>(request.orientation) >= 0 &&
				      static_cast<int>(request.orientation) <=
					      static_cast<int>(ExportOrientation::Both);
	if (request.clipId.empty() || request.sourcePath.empty() || request.outputDirectory.empty()) {
		if (error != nullptr) {
			*error = "export requires clip id, source and output directory";
		}
		return false;
	}
	if (!orientationValid || !isValidPreset(request.preset) || !segmentsValid || request.durationSeconds <= 0 ||
	    request.endOffsetSeconds < 0 || request.endOffsetSeconds > 86400 || request.trimStartMilliseconds < -1 ||
	    (request.trimStartMilliseconds >= 0 && request.trimDurationMilliseconds < 500) ||
	    (request.outputFps != 0 && request.outputFps != 30 && request.outputFps != 60)) {
		if (error != nullptr) {
			*error = "export orientation, preset or duration is invalid";
		}
		return false;
	}
	if ((request.orientation == ExportOrientation::Vertical || request.orientation == ExportOrientation::Both) &&
	    (request.verticalWidth <= 0 || request.verticalHeight <= 0 ||
	     static_cast<long long>(request.verticalWidth) * 16 !=
		     static_cast<long long>(request.verticalHeight) * 9)) {
		if (error != nullptr) {
			*error = "vertical export resolution must use an exact 9:16 ratio";
		}
		return false;
	}
	if (request.verticalSource == VerticalExportSource::VerticalCanvas && request.verticalCanvasPath.empty()) {
		if (error != nullptr) {
			*error = "vertical canvas export requires a rendered canvas source";
		}
		return false;
	}
	return true;
}

std::string ExportManager::addJob(const ExportRequest &request, ExportOrientation orientation)
{
	ExportJob job;
	job.id = "export-" + std::to_string(nextJobId.fetch_add(1));
	job.request = request;
	job.orientation = orientation;
	job.state = ExportJobState::Pending;
	job.createdAt = std::chrono::system_clock::now();
	job.updatedAt = job.createdAt;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		const auto baseName = cleanBaseName(request.outputBaseName.empty() ? request.sourcePath.stem().string()
										   : request.outputBaseName);
		const auto outputStem = baseName + "_" + suffix(orientation);
		auto candidate = request.outputDirectory / (outputStem + ".mp4");
		std::error_code fileError;
		int collision = 2;
		while (std::filesystem::exists(candidate, fileError) || pathReserved(reservedPaths_, candidate)) {
			fileError.clear();
			candidate = request.outputDirectory / (outputStem + "_" + std::to_string(collision++) + ".mp4");
		}
		job.outputPath = candidate;
		reservedPaths_.push_back(candidate);
		auto flag = std::make_shared<std::atomic_bool>(false);
		cancelFlags_[job.id] = flag;
		jobs_[job.id] = job;
		queue_.push_back({job.id, std::move(flag)});
	}
	publish(job);
	return job.id;
}

void ExportManager::run()
{
	while (true) {
		QueuedJob queued;
		{
			std::unique_lock<std::mutex> lock(mutex_);
			condition_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
			if (stopping_ && queue_.empty()) {
				break;
			}
			queued = queue_.front();
			queue_.erase(queue_.begin());
			active_ = true;
		}
		process(queued);
		{
			std::lock_guard<std::mutex> lock(mutex_);
			active_ = false;
		}
		idleCondition_.notify_all();
	}
}

void ExportManager::process(const QueuedJob &queued)
{
	if (queued.cancelRequested->load()) {
		update(queued.id, ExportJobState::Cancelled, 0, "export cancelled");
		return;
	}
	update(queued.id, ExportJobState::Exporting, 0);
	const auto snapshot = job(queued.id);
	std::error_code error;
	std::filesystem::create_directories(snapshot.outputPath.parent_path(), error);
	if (error) {
		update(queued.id, ExportJobState::Error, 0, "could not create export directory: " + error.message());
		return;
	}
	const auto temporaryPath =
		snapshot.outputPath.parent_path() / (snapshot.outputPath.stem().string() + ".part.mp4");
	std::filesystem::remove(temporaryPath, error);
	auto result = backend_ != nullptr
			      ? backend_->execute(
					snapshot, temporaryPath,
					[this, id = queued.id](int progress) {
						update(id, ExportJobState::Exporting, std::clamp(progress, 0, 99));
					},
					*queued.cancelRequested)
			      : ExportBackendResult::fail("export backend is unavailable");
	if (queued.cancelRequested->load() || result.cancelled) {
		std::filesystem::remove(temporaryPath, error);
		update(queued.id, ExportJobState::Cancelled, snapshot.progressPercent, "export cancelled");
		return;
	}
	if (!result.success) {
		std::filesystem::remove(temporaryPath, error);
		update(queued.id, ExportJobState::Error, snapshot.progressPercent,
		       result.error.empty() ? "FFmpeg export failed" : std::move(result.error));
		return;
	}
	if (!std::filesystem::is_regular_file(temporaryPath, error)) {
		update(queued.id, ExportJobState::Error, snapshot.progressPercent,
		       "encoder completed without producing an output file");
		return;
	}
	if (std::filesystem::exists(snapshot.outputPath, error)) {
		std::filesystem::remove(temporaryPath, error);
		update(queued.id, ExportJobState::Error, snapshot.progressPercent,
		       "export destination appeared during processing; no file was overwritten");
		return;
	}
	std::filesystem::rename(temporaryPath, snapshot.outputPath, error);
	if (error) {
		std::filesystem::remove(temporaryPath, error);
		update(queued.id, ExportJobState::Error, snapshot.progressPercent,
		       "could not publish export output: " + error.message());
		return;
	}
	update(queued.id, ExportJobState::Done, 100);
}

void ExportManager::update(const std::string &id, ExportJobState state, int progress, std::string error)
{
	ExportJob snapshot;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto found = jobs_.find(id);
		if (found == jobs_.end()) {
			return;
		}
		found->second.state = state;
		found->second.progressPercent = progress;
		found->second.error = std::move(error);
		found->second.updatedAt = std::chrono::system_clock::now();
		snapshot = found->second;
	}
	publish(snapshot);
	if (state == ExportJobState::Error) {
		log(true, "Export " + id + " failed: " + snapshot.error);
	} else if (state == ExportJobState::Done) {
		log(false, "Export " + id + " completed: " + snapshot.outputPath.u8string());
	}
}

void ExportManager::publish(const ExportJob &job)
{
	JobCallback callback;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		callback = metadataCallback_;
	}
	if (callback) {
		callback(job);
	}
}

void ExportManager::log(bool error, const std::string &message) const
{
	if (logCallback_) {
		logCallback_(error, message);
	}
}

} // namespace clipcoach
