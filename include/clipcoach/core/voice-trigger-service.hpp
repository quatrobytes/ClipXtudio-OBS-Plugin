#pragma once

#include <clipcoach/core/speech-engine.hpp>
#include <clipcoach/core/trigger-engine.hpp>

#include <optional>

namespace clipcoach {

struct VoicePhraseMatch {
	std::string configuredPhrase;
	std::string normalizedTranscript;
	double similarity{0.0};
};

class VoicePhraseMatcher final {
public:
	[[nodiscard]] static std::string normalize(std::string_view text);
	[[nodiscard]] static std::optional<VoicePhraseMatch>
	bestMatch(std::string_view transcript, const std::vector<std::string> &phrases, int sensitivity);
};

struct VoiceTriggerConfiguration {
	bool enabled{false};
	int sensitivity{70};
	int preRollSeconds{5};
	int postRollSeconds{5};
	TriggerAction action{TriggerAction::MarkMoment};
	std::chrono::milliseconds cooldown{15000};
	std::string language{"auto"};
	std::vector<std::string> phrases{"clip",       "guarda eso",   "eso va pa TikTok", "modo grinch",
					 "no ombe no", "eso fue duro", "pa que aprendan"};
};

enum class VoiceTriggerStatus {
	EventCreated,
	Disabled,
	ProRequired,
	RecognitionFailed,
	LowConfidence,
	NoPhraseMatch,
	Cooldown,
	TriggerRejected,
	InvalidConfiguration,
};

struct VoiceTriggerResult {
	VoiceTriggerStatus status{VoiceTriggerStatus::RecognitionFailed};
	std::optional<TriggerEvent> event;
	std::optional<VoicePhraseMatch> match;
	std::string message;
};

class VoiceTriggerService final {
public:
	VoiceTriggerService(SpeechEngine &speechEngine, TriggerEngine &triggerEngine, bool proUnlocked);

	[[nodiscard]] bool configure(const VoiceTriggerConfiguration &configuration, std::string *error = nullptr);
	[[nodiscard]] const VoiceTriggerConfiguration &configuration() const noexcept;
	void setProUnlocked(bool unlocked) noexcept;

	[[nodiscard]] VoiceTriggerResult process(const AudioSegment &segment);
	[[nodiscard]] VoiceTriggerResult
	processRecognition(const SpeechRecognitionResult &recognition, std::chrono::system_clock::time_point occurredAt,
			   std::chrono::milliseconds duration = std::chrono::milliseconds{0});

private:
	SpeechEngine &speechEngine_;
	TriggerEngine &triggerEngine_;
	VoiceTriggerConfiguration configuration_;
	std::optional<std::chrono::system_clock::time_point> lastMatchAt_;
	bool proUnlocked_{false};
};

} // namespace clipcoach
