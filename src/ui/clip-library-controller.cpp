#include <clipcoach/ui/clip-library-controller.hpp>

#include <clipcoach/core/clip-action-service.hpp>
#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/storage/clip-library-service.hpp>

#include <QMetaObject>
#include <QPointer>

#include <algorithm>
#include <utility>

namespace clipcoach::ui {

ClipLibraryController::ClipLibraryController(storage::ClipLibraryService *libraryService,
					     ClipActionService *actionService, std::string sessionId, QObject *parent,
					     const FeatureGateService *featureGates)
	: QObject(parent),
	  libraryService_(libraryService),
	  actionService_(actionService),
	  sessionId_(std::move(sessionId)),
	  featureGates_(featureGates),
	  viewModel_([] { return std::chrono::system_clock::now(); }, featureGates)
{
}

void ClipLibraryController::setStateCallback(StateCallback callback)
{
	stateCallback_ = std::move(callback);
	publish();
}

void ClipLibraryController::reload()
{
	const auto generation = ++requestGeneration_;
	if (libraryService_ == nullptr) {
		loadedClips_.clear();
		viewModel_.setClips({});
		viewModel_.setSession(std::nullopt);
		publish();
		return;
	}
	publish(true);
	QPointer<ClipLibraryController> guard(this);
	const auto querySession = fullHistory_ && featureGates_ != nullptr &&
						  featureGates_->isAllowed(Feature::FullHistory)
					  ? std::string{}
					  : sessionId_;
	libraryService_->loadSnapshotAsync(querySession, 1000, [guard, generation](auto result) mutable {
		if (guard.isNull()) {
			return;
		}
		QMetaObject::invokeMethod(
			guard,
			[guard, generation, result = std::move(result)]() mutable {
				if (guard.isNull() || generation != guard->requestGeneration_) {
					return;
				}
				if (!result.success) {
					guard->publish(false, ClipLibraryErrorType::Load,
						       QString::fromStdString(result.error));
					return;
				}
				guard->loadedClips_ = std::move(result.value.clips);
				guard->viewModel_.setClips(guard->loadedClips_);
				guard->viewModel_.setSession(std::move(result.value.session));
				guard->publish();
			},
			Qt::QueuedConnection);
	});
}

void ClipLibraryController::setFullHistory(bool enabled)
{
	const bool allowed = featureGates_ != nullptr && featureGates_->isAllowed(Feature::FullHistory);
	const bool next = enabled && allowed;
	if (fullHistory_ == next)
		return;
	fullHistory_ = next;
	reload();
}

void ClipLibraryController::setFilter(ClipFilter filter)
{
	viewModel_.setFilter(filter);
	publish();
}

void ClipLibraryController::setSort(ClipSort sort)
{
	viewModel_.setSort(sort);
	publish();
}

void ClipLibraryController::setSearchQuery(std::string query)
{
	viewModel_.setSearchQuery(std::move(query));
	publish();
}

void ClipLibraryController::setFavorite(const std::string &clipId, bool favorite)
{
	if (libraryService_ == nullptr || findClip(clipId) == nullptr) {
		publish(false, ClipLibraryErrorType::Favorite);
		return;
	}
	for (auto &clip : loadedClips_) {
		if (clip.id == clipId) {
			clip.isFavorite = favorite;
			break;
		}
	}
	viewModel_.updateFavorite(clipId, favorite);
	publish();

	QPointer<ClipLibraryController> guard(this);
	libraryService_->setFavorite(clipId, favorite, [guard, clipId, favorite](auto status) {
		if (guard.isNull() || status.success) {
			return;
		}
		QMetaObject::invokeMethod(
			guard,
			[guard, clipId, favorite, error = QString::fromStdString(status.error)] {
				if (guard.isNull()) {
					return;
				}
				for (auto &clip : guard->loadedClips_) {
					if (clip.id == clipId) {
						clip.isFavorite = !favorite;
						break;
					}
				}
				guard->viewModel_.updateFavorite(clipId, !favorite);
				guard->publish(false, ClipLibraryErrorType::Favorite, error);
			},
			Qt::QueuedConnection);
	});
}

void ClipLibraryController::deleteClips(std::vector<std::string> clipIds, DeleteCallback callback)
{
	if (libraryService_ == nullptr || clipIds.empty()) {
		if (callback)
			callback(storage::StorageStatus::fail("select at least one available clip"));
		return;
	}
	for (const auto &id : clipIds) {
		if (findClip(id) == nullptr) {
			if (callback)
				callback(storage::StorageStatus::fail("one of the selected clips is unavailable"));
			return;
		}
	}

	QPointer<ClipLibraryController> guard(this);
	const auto idsForCallback = clipIds;
	libraryService_->deleteClips(std::move(clipIds), [guard, clipIds = idsForCallback,
							  callback = std::move(callback)](auto status) mutable {
		if (guard.isNull())
			return;
		QMetaObject::invokeMethod(
			guard,
			[guard, clipIds = std::move(clipIds), status = std::move(status),
			 callback = std::move(callback)]() mutable {
				if (guard.isNull())
					return;
				if (status.success) {
					guard->loadedClips_.erase(
						std::remove_if(guard->loadedClips_.begin(), guard->loadedClips_.end(),
							       [&clipIds](const ClipMetadata &clip) {
								       return std::find(clipIds.begin(), clipIds.end(),
											clip.id) != clipIds.end();
							       }),
						guard->loadedClips_.end());
					guard->viewModel_.setClips(guard->loadedClips_);
					guard->publish();
				} else {
					guard->reload();
				}
				if (callback)
					callback(std::move(status));
			},
			Qt::QueuedConnection);
	});
}

ClipActionResult ClipLibraryController::preview(const std::string &clipId)
{
	const auto *clip = findClip(clipId);
	return clip != nullptr && actionService_ != nullptr ? actionService_->preview(clip->filePath)
							    : ClipActionResult::fail("clip is unavailable");
}

ClipActionResult ClipLibraryController::openFolder(const std::string &clipId)
{
	const auto *clip = findClip(clipId);
	return clip != nullptr && actionService_ != nullptr ? actionService_->openFolder(clip->filePath)
							    : ClipActionResult::fail("clip is unavailable");
}

ClipActionResult ClipLibraryController::openCaption(const std::string &clipId)
{
	const auto *clip = findClip(clipId);
	if (clip == nullptr || actionService_ == nullptr) {
		return ClipActionResult::fail("caption is unavailable");
	}
	if (!clip->caption.empty()) {
		return actionService_->presentCaption(clip->caption);
	}
	return !clip->transcriptPath.empty() ? actionService_->openTextAsset(clip->transcriptPath)
					     : ClipActionResult::fail("caption is unavailable");
}

ClipActionResult ClipLibraryController::openSubtitles(const std::string &clipId)
{
	const auto *clip = findClip(clipId);
	return clip != nullptr && actionService_ != nullptr && !clip->subtitlePath.empty()
		       ? actionService_->openTextAsset(clip->subtitlePath)
		       : ClipActionResult::fail("subtitles are unavailable");
}

ClipActionResult ClipLibraryController::exportVertical(const std::string &clipId)
{
	const auto *clip = findClip(clipId);
	return clip != nullptr && actionService_ != nullptr
		       ? actionService_->requestExport(*clip, ExportOrientation::Vertical)
		       : ClipActionResult::fail("clip is unavailable");
}

ClipActionResult ClipLibraryController::exportSelection(const std::vector<std::string> &clipIds,
							ExportOrientation orientation)
{
	if (actionService_ == nullptr || clipIds.empty()) {
		return ClipActionResult::fail("select at least one clip");
	}
	std::vector<ClipMetadata> clips;
	clips.reserve(clipIds.size());
	for (const auto &id : clipIds) {
		const auto *clip = findClip(id);
		if (clip == nullptr) {
			return ClipActionResult::fail("one of the selected clips is unavailable");
		}
		clips.push_back(*clip);
	}
	return actionService_->requestBatchExport(clips, orientation);
}

void ClipLibraryController::publish(bool loading, ClipLibraryErrorType errorType, QString error)
{
	if (stateCallback_) {
		stateCallback_({loading, errorType, std::move(error), viewModel_.visibleClips(), viewModel_.summary()});
	}
}

const ClipMetadata *ClipLibraryController::findClip(const std::string &clipId) const
{
	const auto found = std::find_if(loadedClips_.begin(), loadedClips_.end(),
					[&clipId](const ClipMetadata &clip) { return clip.id == clipId; });
	return found == loadedClips_.end() ? nullptr : &*found;
}

} // namespace clipcoach::ui
