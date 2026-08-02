#include <clipcoach/plugin/obs-replay-manager.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>

#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <utility>

namespace clipcoach::plugin {
namespace {
constexpr const char *kLogPrefix = "[ClipXtudio]";
}

ObsReplayManager::ObsReplayManager()
	: state_(obs_frontend_replay_buffer_active() ? ReplayState::Active
						    : ReplayState::Inactive)
{
	obs_frontend_add_event_callback(frontendEventCallback, this);
}

ObsReplayManager::~ObsReplayManager()
{
	if (frontendCallbacksAvailable_) {
		obs_frontend_remove_event_callback(frontendEventCallback, this);
	}
}

ReplayState ObsReplayManager::state() const noexcept
{
	return state_;
}

int ObsReplayManager::bufferedDurationSeconds() const noexcept
{
	obs_output_t *output = obs_frontend_get_replay_buffer_output();
	if (output == nullptr)
		return 0;
	const auto frames = std::max(0, obs_output_get_total_frames(output));
	obs_output_release(output);
	obs_video_info video{};
	if (!obs_get_video_info(&video) || video.fps_num == 0)
		return 0;
	return static_cast<int>(
		(static_cast<std::uint64_t>(frames) * video.fps_den) /
		video.fps_num);
}

ReplayOperationResult ObsReplayManager::start()
{
	if (obs_frontend_replay_buffer_active()) {
		setState(ReplayState::Active);
		return {true, {}};
	}

	setState(ReplayState::Starting);
	startPending_ = true;
	blog(LOG_INFO, "%s Replay Buffer start requested", kLogPrefix);
	obs_frontend_replay_buffer_start();
	return {true, {}};
}

ReplayOperationResult ObsReplayManager::stop()
{
	if (!obs_frontend_replay_buffer_active() && state_ != ReplayState::Starting) {
		setState(ReplayState::Inactive);
		return {true, {}};
	}

	setState(ReplayState::Stopping);
	startPending_ = false;
	blog(LOG_INFO, "%s Replay Buffer stop requested", kLogPrefix);
	obs_frontend_replay_buffer_stop();
	return {true, {}};
}

ReplayOperationResult ObsReplayManager::save()
{
	if (!obs_frontend_replay_buffer_active()) {
		setState(ReplayState::Error);
		blog(LOG_WARNING, "%s Replay save rejected because the buffer is inactive",
		     kLogPrefix);
		return {false, "Replay Buffer is not active"};
	}

	blog(LOG_INFO, "%s Manual replay save requested", kLogPrefix);
	obs_frontend_replay_buffer_save();
	return {true, {}};
}

void ObsReplayManager::setStateChangedCallback(StateChangedCallback callback)
{
	stateChangedCallback_ = std::move(callback);
}

void ObsReplayManager::setReplaySavedCallback(ReplaySavedCallback callback)
{
	replaySavedCallback_ = std::move(callback);
}

void ObsReplayManager::frontendEventCallback(enum obs_frontend_event event, void *privateData)
{
	static_cast<ObsReplayManager *>(privateData)->handleFrontendEvent(event);
}

void ObsReplayManager::handleFrontendEvent(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		setState(ReplayState::Starting);
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
		startPending_ = false;
		setState(ReplayState::Active);
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPING:
		setState(ReplayState::Stopping);
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		setState(startPending_ ? ReplayState::Error : ReplayState::Inactive);
		startPending_ = false;
		break;
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED: {
		char *rawPath = obs_frontend_get_last_replay();
		if (rawPath == nullptr || *rawPath == '\0') {
			if (rawPath != nullptr) {
				bfree(rawPath);
			}
			setState(ReplayState::Error);
			blog(LOG_ERROR, "%s OBS emitted a replay saved event without a file path",
			     kLogPrefix);
			return;
		}

		const std::filesystem::path path(rawPath);
		bfree(rawPath);
		if (replaySavedCallback_) {
			replaySavedCallback_(path);
		}
		blog(LOG_INFO, "%s Replay saved event processed", kLogPrefix);
		break;
	}
	case OBS_FRONTEND_EVENT_EXIT:
		frontendCallbacksAvailable_ = false;
		break;
	default:
		break;
	}
}

void ObsReplayManager::setState(ReplayState state)
{
	if (state_ == state) {
		return;
	}
	state_ = state;
	const char *stateName = "inactive";
	switch (state_) {
	case ReplayState::Starting:
		stateName = "starting";
		break;
	case ReplayState::Active:
		stateName = "active";
		break;
	case ReplayState::Stopping:
		stateName = "stopping";
		break;
	case ReplayState::Error:
		stateName = "error";
		break;
	case ReplayState::Inactive:
		break;
	}
	blog(LOG_INFO, "%s Replay Buffer state changed: %s", kLogPrefix, stateName);
	if (stateChangedCallback_) {
		stateChangedCallback_(state_);
	}
}

} // namespace clipcoach::plugin
