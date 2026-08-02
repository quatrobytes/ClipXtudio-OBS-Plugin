#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/core/replay-manager.hpp>

#include <chrono>
#include <functional>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach {

class FeatureGateService;

enum class CaptureError {
	None,
	InvalidDuration,
	ReplayBufferInactive,
	ReplayBufferWarmingUp,
	CaptureAlreadyPending,
	SaveRejected,
	SavedFileUnavailable,
	FileRenameFailed,
	ProRequired,
};

struct CaptureResult {
	bool accepted{false};
	CaptureError error{CaptureError::None};
	std::string message;
};

struct MomentMarker {
	std::chrono::system_clock::time_point markedAt;
	std::string label;
};

class ClipManager final {
public:
	using Clock = std::function<std::chrono::system_clock::time_point()>;
	using ClipSavedCallback = std::function<void(const ClipMetadata &)>;
	using ErrorCallback = std::function<void(CaptureError, const std::string &)>;
	using ReplayStateCallback = std::function<void(ReplayState)>;
	using ClipPersistenceCallback = std::function<void(const ClipMetadata &)>;
	using SourceSceneProvider = std::function<std::string()>;
	using ObserverId = std::uint64_t;

	explicit ClipManager(ReplayManager &replayManager, Clock clock = std::chrono::system_clock::now,
			     const FeatureGateService *featureGates = nullptr);
	~ClipManager();

	ClipManager(const ClipManager &) = delete;
	ClipManager &operator=(const ClipManager &) = delete;

	[[nodiscard]] static bool isValidDuration(int seconds) noexcept;
	[[nodiscard]] static std::string createReadableFileName(std::chrono::system_clock::time_point capturedAt,
								const std::string &trigger,
								const std::string &extension);

	[[nodiscard]] ReplayState replayState() const noexcept;
	[[nodiscard]] CaptureResult startReplayBuffer();
	[[nodiscard]] CaptureResult stopReplayBuffer();
	[[nodiscard]] CaptureResult captureManual(int durationSeconds);
	[[nodiscard]] CaptureResult captureTriggered(int durationSeconds,
						     TriggerType triggerType,
						     std::string triggerLabel,
						     int score = 0,
						     std::string requestedBy = {});
	[[nodiscard]] bool capturePending() const noexcept;
	[[nodiscard]] CaptureResult markMoment(std::string label = "manual", int secondsBeforeNow = 0);
	[[nodiscard]] const std::vector<ClipMetadata> &sessionClips() const noexcept;
	[[nodiscard]] const std::vector<MomentMarker> &markedMoments() const noexcept;

	void setClipSavedCallback(ClipSavedCallback callback);
	void setErrorCallback(ErrorCallback callback);
	void setReplayStateCallback(ReplayStateCallback callback);
	void setClipPersistenceCallback(ClipPersistenceCallback callback);
	void setCaptureContext(std::string sessionId, std::string appVersion,
			       SourceSceneProvider sourceSceneProvider = {});
	[[nodiscard]] ObserverId addClipSavedObserver(ClipSavedCallback callback);
	[[nodiscard]] ObserverId addErrorObserver(ErrorCallback callback);
	void removeClipSavedObserver(ObserverId id);
	void removeErrorObserver(ObserverId id);

private:
	struct PendingCapture {
		int durationSeconds;
		TriggerType triggerType;
		std::string triggerLabel;
		std::string requestedBy;
		int score;
		std::chrono::system_clock::time_point capturedAt;
	};

	[[nodiscard]] CaptureResult beginCapture(int durationSeconds,
						TriggerType triggerType,
						std::string triggerLabel,
						int score,
						std::string requestedBy = {});
	void handleReplaySaved(const std::filesystem::path &sourcePath);
	void reportError(CaptureError error, std::string message);

	ReplayManager &replayManager_;
	const FeatureGateService *featureGates_{nullptr};
	Clock clock_;
	std::optional<PendingCapture> pendingCapture_;
	std::vector<ClipMetadata> sessionClips_;
	std::vector<MomentMarker> markedMoments_;
	ClipSavedCallback clipSavedCallback_;
	ErrorCallback errorCallback_;
	ReplayStateCallback replayStateCallback_;
	ClipPersistenceCallback clipPersistenceCallback_;
	SourceSceneProvider sourceSceneProvider_;
	std::string sessionId_{"session-current"};
	std::string appVersion_;
	std::map<ObserverId, ClipSavedCallback> clipSavedObservers_;
	std::map<ObserverId, ErrorCallback> errorObservers_;
	ObserverId nextObserverId_{1};
};

} // namespace clipcoach
