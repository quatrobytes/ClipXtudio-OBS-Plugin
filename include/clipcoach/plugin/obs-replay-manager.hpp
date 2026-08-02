#pragma once

#include <clipcoach/core/replay-manager.hpp>

#include <obs-frontend-api.h>

namespace clipcoach::plugin {

class ObsReplayManager final : public ReplayManager {
public:
	ObsReplayManager();
	~ObsReplayManager() override;

	[[nodiscard]] ReplayState state() const noexcept override;
	[[nodiscard]] int bufferedDurationSeconds() const noexcept override;
	ReplayOperationResult start() override;
	ReplayOperationResult stop() override;
	ReplayOperationResult save() override;
	void setStateChangedCallback(StateChangedCallback callback) override;
	void setReplaySavedCallback(ReplaySavedCallback callback) override;

private:
	static void frontendEventCallback(enum obs_frontend_event event, void *privateData);
	void handleFrontendEvent(enum obs_frontend_event event);
	void setState(ReplayState state);

	ReplayState state_{ReplayState::Inactive};
	bool startPending_{false};
	bool frontendCallbacksAvailable_{true};
	StateChangedCallback stateChangedCallback_;
	ReplaySavedCallback replaySavedCallback_;
};

} // namespace clipcoach::plugin
