#pragma once

#include <clipcoach/core/settings.hpp>

#include <functional>
#include <string>
#include <vector>

namespace clipcoach {

enum class VoiceRuntimeState {
	Disabled,
	Starting,
	WaitingForAudio,
	Listening,
	SourceUnavailable,
	ModelUnavailable,
	Error,
};

struct VoiceRuntimeStatus {
	VoiceRuntimeState state{VoiceRuntimeState::Disabled};
	bool modelLoaded{false};
	bool audioReceiving{false};
	std::string sourceName;
	std::string message;
	std::string lastTranscript;
};

class VoiceTriggerController {
public:
	using StatusCallback = std::function<void(const VoiceRuntimeStatus &)>;

	virtual ~VoiceTriggerController() = default;
	virtual void applySettings(const Settings &settings) = 0;
	virtual void setProUnlocked(bool unlocked) = 0;
	virtual void shutdown() noexcept {}
	[[nodiscard]] virtual VoiceRuntimeStatus status() const = 0;
	virtual void setStatusCallback(StatusCallback callback) = 0;
	[[nodiscard]] virtual std::vector<std::string> availableAudioSources() const { return {}; }
};

} // namespace clipcoach
