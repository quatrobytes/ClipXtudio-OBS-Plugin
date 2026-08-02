#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach::remote {

enum class RemoteCommandType {
	MarkMoment,
	SaveClip30,
	SaveClip60,
	SaveCustom,
	SaveVertical,
	SaveBoth,
	Unknown,
};

enum class RemoteConnectionState {
	Paused,
	Connecting,
	Connected,
	Offline,
	Unauthorized,
	Unavailable,
};

struct RemoteCommand {
	std::string uuid;
	RemoteCommandType type{RemoteCommandType::Unknown};
	int durationSeconds{0};
	int delayCompensationSeconds{0};
	std::string note;
	std::string requestedBy;
	std::chrono::system_clock::time_point expiresAt{};
};

struct RemoteCapturePlan {
	std::string commandUuid;
	RemoteCommandType type{RemoteCommandType::Unknown};
	int durationSeconds{0};
	int delayCompensationSeconds{0};
	int replayWindowSeconds{0};
	bool vertical{false};
	bool both{false};
	std::string note;
	std::string requestedBy;
};

struct RemoteCommandResult {
	std::string commandUuid;
	bool success{false};
	std::string clipId;
	std::string fileName;
	int durationSeconds{0};
	std::string orientation;
	std::string message;
	std::string errorCode;
	std::string errorMessage;
};

struct RemoteHeartbeatRequest {
	std::string deviceActivationId;
	std::string pluginVersion;
	std::string obsVersion;
	std::string replayBufferStatus{"unknown"};
	std::string verticalCanvasStatus{"unknown"};
	std::string currentScene;
};

struct RemoteHeartbeatResponse {
	bool remoteEnabled{false};
	int pollIntervalSeconds{3};
	std::string sessionId;
	std::string message;
};

struct RemoteClientError {
	int httpStatus{0};
	std::string code;
	std::string message;
	bool retryable{false};
	bool unauthorized{false};
};

template<typename T> struct RemoteClientResult {
	std::optional<T> value;
	RemoteClientError error;

	[[nodiscard]] bool succeeded() const noexcept { return value.has_value(); }
};

struct RemoteClipperStatus {
	RemoteConnectionState connection{RemoteConnectionState::Paused};
	bool remoteEnabled{false};
	bool localCommandsEnabled{true};
	std::string sessionId;
	std::string message;
	std::string errorCode;
	int pollIntervalSeconds{3};
	int pendingCommands{0};
	std::optional<std::chrono::system_clock::time_point> lastHeartbeatAt;
};

[[nodiscard]] RemoteCommandType remoteCommandTypeFromString(const std::string &value) noexcept;
[[nodiscard]] const char *remoteCommandTypeName(RemoteCommandType type) noexcept;
[[nodiscard]] bool isValidCommandUuid(const std::string &value) noexcept;

} // namespace clipcoach::remote
