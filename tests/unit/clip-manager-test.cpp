#include "test-support.hpp"

#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/core/replay-manager.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <limits>
#include <string>

namespace {

class MockReplayManager final : public clipcoach::ReplayManager {
public:
	[[nodiscard]] clipcoach::ReplayState state() const noexcept override { return state_; }
	[[nodiscard]] int bufferedDurationSeconds() const noexcept override { return bufferedSeconds; }

	clipcoach::ReplayOperationResult start() override
	{
		++startCalls;
		return {true, {}};
	}

	clipcoach::ReplayOperationResult stop() override
	{
		++stopCalls;
		return {true, {}};
	}

	clipcoach::ReplayOperationResult save() override
	{
		++saveCalls;
		return saveResult;
	}

	void setStateChangedCallback(StateChangedCallback callback) override { stateCallback = std::move(callback); }

	void setReplaySavedCallback(ReplaySavedCallback callback) override { savedCallback = std::move(callback); }

	void setState(clipcoach::ReplayState state)
	{
		state_ = state;
		if (stateCallback) {
			stateCallback(state_);
		}
	}

	void emitSaved(const std::filesystem::path &path)
	{
		if (savedCallback) {
			savedCallback(path);
		}
	}

	clipcoach::ReplayState state_{clipcoach::ReplayState::Inactive};
	clipcoach::ReplayOperationResult saveResult{true, {}};
	int startCalls{0};
	int stopCalls{0};
	int saveCalls{0};
	int bufferedSeconds{std::numeric_limits<int>::max()};
	StateChangedCallback stateCallback;
	ReplaySavedCallback savedCallback;
};

std::filesystem::path uniqueTestDirectory()
{
	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("clipcoach-clip-manager-test-" + std::to_string(stamp));
}

} // namespace

int main()
{
	using namespace std::chrono;
	using clipcoach::CaptureError;
	using clipcoach::ClipManager;
	using clipcoach::ReplayState;
	using clipcoach::test::expect;

	MockReplayManager inactiveReplay;
	ClipManager inactiveManager(inactiveReplay);
	const auto inactiveResult = inactiveManager.captureManual(30);
	expect(!inactiveResult.accepted, "capture must fail while Replay Buffer is inactive");
	expect(inactiveResult.error == CaptureError::ReplayBufferInactive,
	       "inactive Replay Buffer must return its specific error");
	expect(inactiveReplay.saveCalls == 0, "inactive capture must not ask OBS to save");

	clipcoach::FeatureGateService freeGates;
	MockReplayManager gatedReplay;
	gatedReplay.state_ = ReplayState::Active;
	ClipManager gatedManager(gatedReplay, system_clock::now, &freeGates);
	const auto freeLongDuration = gatedManager.captureManual(120);
	expect(!freeLongDuration.accepted && freeLongDuration.error == CaptureError::ProRequired,
	       "Free must reject durations outside its three quick durations in core");
	expect(gatedReplay.saveCalls == 0, "a rejected Pro duration must never reach OBS");
	freeGates.setEntitlementState(clipcoach::EntitlementState::ProActive);
	expect(gatedManager.captureManual(120).accepted, "Pro must accept extended durations");
	expect(gatedReplay.saveCalls == 1, "an allowed Pro duration must reach ReplayManager");

	const auto directory = uniqueTestDirectory();
	std::filesystem::create_directories(directory);
	const auto obsReplayPath = directory / "Replay 2026-07-28 14-05-09.mp4";
	std::ofstream(obsReplayPath).put('\0');

	MockReplayManager replay;
	replay.state_ = ReplayState::Active;
	replay.bufferedSeconds = 17;
	ClipManager warmingManager(replay);
	const auto warming = warmingManager.captureManual(30);
	expect(!warming.accepted && warming.error == CaptureError::ReplayBufferWarmingUp && replay.saveCalls == 0,
	       "capture must not silently save a shorter clip while Replay Buffer is still filling");
	replay.bufferedSeconds = std::numeric_limits<int>::max();
	const auto fixedTime = system_clock::from_time_t(1785247509);
	ClipManager manager(replay, [fixedTime] { return fixedTime; });
	const auto marker = manager.markMoment("remote_clipper");
	expect(marker.accepted && manager.markedMoments().size() == 1 &&
		       manager.markedMoments().front().label == "remote_clipper",
	       "mark moment must record an immediate local timeline marker without saving Replay Buffer");
	const auto delayedMarker = manager.markMoment("remote_clipper", 20);
	expect(delayedMarker.accepted && manager.markedMoments().size() == 2 &&
		       manager.markedMoments().back().markedAt == fixedTime - seconds(20),
	       "remote mark moment must apply the editor delay to its local timeline timestamp");
	manager.setCaptureContext("session-persisted", "0.2.0", [] { return std::string("Gameplay"); });
	std::optional<clipcoach::ClipMetadata> persistenceCandidate;
	manager.setClipPersistenceCallback([&persistenceCandidate](const auto &clip) { persistenceCandidate = clip; });

	const auto accepted = manager.captureManual(30);
	expect(accepted.accepted, "active Replay Buffer must accept a manual capture");
	expect(replay.saveCalls == 1, "manual capture must invoke ReplayManager exactly once");
	expect(manager.sessionClips().empty(), "metadata must wait for OBS saved event");

	replay.emitSaved(obsReplayPath);
	expect(manager.sessionClips().size() == 1, "saved replay must create session metadata");

	const auto &clip = manager.sessionClips().front();
	expect(clip.durationSeconds == 30, "metadata must keep requested duration");
	expect(clip.triggerType == clipcoach::TriggerType::Manual, "manual clips must be marked trigger=manual");
	expect(clip.score >= 70 && clip.score <= 100,
	       "manual clips must receive a meaningful local score instead of zero");
	expect(clip.exportStatus == clipcoach::ExportStatus::Exported,
	       "a completed source replay must not remain marked as pending");
	expect(clip.sessionId == "session-persisted", "metadata must keep session id");
	expect(clip.sourceScene == "Gameplay", "metadata must keep source scene");
	expect(clip.appVersion == "0.2.0", "metadata must keep app version");
	expect(clip.fileName == "ClipX_2026-07-28_14-05-09_manual.mp4", "metadata must use a readable file name");
	expect(clip.filePath.filename() == clip.fileName, "saved replay must be renamed to its readable metadata name");
	expect(std::filesystem::exists(clip.filePath), "renamed clip must exist");
	expect(!std::filesystem::exists(obsReplayPath), "original opaque replay name must be moved");
	expect(persistenceCandidate.has_value() && persistenceCandidate->id == clip.id,
	       "every completed capture must be emitted to persistence");

	const auto voiceReplayPath = directory / "Replay voice.mp4";
	std::ofstream(voiceReplayPath).put('\0');
	const auto voiceCapture = manager.captureTriggered(90, clipcoach::TriggerType::Voice, "saca clip", 87);
	expect(voiceCapture.accepted, "active Replay Buffer must accept a typed trigger capture");
	replay.emitSaved(voiceReplayPath);
	const auto &voiceClip = manager.sessionClips().back();
	expect(voiceClip.triggerType == clipcoach::TriggerType::Voice,
	       "triggered capture must preserve its trigger type");
	expect(voiceClip.triggerLabel == "saca clip", "triggered capture must preserve the matched phrase");
	expect(voiceClip.score == 87, "triggered capture must preserve the trigger score");
	expect(voiceClip.fileName == "ClipX_2026-07-28_14-05-09_sacaclip.mp4",
	       "trigger phrase must produce a readable safe file name");

	const auto remoteReplayPath = directory / "Replay remote.mp4";
	std::ofstream(remoteReplayPath).put('\0');
	const auto remoteCapture =
		manager.captureTriggered(60, clipcoach::TriggerType::Manual, "remote_clipper", 0, "editor@example.com");
	expect(remoteCapture.accepted, "remote capture must use the normal Replay Buffer path");
	replay.emitSaved(remoteReplayPath);
	const auto &remoteClip = manager.sessionClips().back();
	expect(remoteClip.triggerType == clipcoach::TriggerType::Manual,
	       "remote capture remains a manual OBS save at the trigger level");
	expect(remoteClip.triggerLabel == "remote_clipper", "remote capture origin must not be overwritten as manual");
	expect(remoteClip.requestedBy == "editor@example.com", "remote capture must preserve who requested it");

	std::filesystem::remove_all(directory);
	return clipcoach::test::pass("clip-manager-test");
}
