#include "test-support.hpp"

#include <clipcoach/core/voice-trigger-service.hpp>

namespace {

class FakeSpeechEngine final : public clipcoach::SpeechEngine {
public:
	clipcoach::SpeechRecognitionResult next;
	int calls{0};

	clipcoach::SpeechRecognitionResult recognize(const clipcoach::AudioSegment &, std::string_view) override
	{
		++calls;
		return next;
	}
};

void expect(bool condition, const char *message)
{
	clipcoach::test::expect(condition, message);
}

clipcoach::AudioSegment segmentAt(int seconds)
{
	clipcoach::AudioSegment segment;
	segment.sampleRateHz = 16000;
	segment.duration = std::chrono::seconds(2);
	segment.pcmMono.resize(32000);
	segment.startedAt = std::chrono::system_clock::time_point(std::chrono::seconds(seconds));
	return segment;
}

} // namespace

int main()
{
	using namespace clipcoach;

	expect(VoicePhraseMatcher::normalize("  ¡GUÁRDA   ÉSO! ") == "guarda eso",
	       "Spanish accents, punctuation and whitespace must normalize");
	expect(VoicePhraseMatcher::normalize("THAT'S   A CLIP.") == "that s a clip", "English text must normalize");

	const auto spanish =
		VoicePhraseMatcher::bestMatch("Ey, eso va pa TikTók ahora", {"clip", "eso va pa TikTok"}, 80);
	expect(spanish && spanish->configuredPhrase == "eso va pa TikTok",
	       "accented Spanish transcript must match configured phrase");
	expect(VoicePhraseMatcher::bestMatch("Please save that now", {"save that"}, 80).has_value(),
	       "English configured phrases must match");
	expect(!VoicePhraseMatcher::bestMatch("haz click aquí", {"clip"}, 20),
	       "single word phrases must not fuzzy-match similar words");
	expect(!VoicePhraseMatcher::bestMatch("guarda esto", {"guarda eso"}, 90),
	       "high sensitivity must reject similar phrases");
	expect(VoicePhraseMatcher::bestMatch("eso fue dura", {"eso fue duro"}, 40).has_value(),
	       "lower sensitivity may accept a close multi-word phrase");

	FakeSpeechEngine speech;
	speech.next = {"Eso fue duro", "es", 0.96, SpeechRecognitionError::None, {}};
	TriggerEngine freeEngine(false);
	VoiceTriggerService freeService(speech, freeEngine, false);
	VoiceTriggerConfiguration config;
	config.enabled = true;
	std::string error;
	expect(!freeService.configure(config, &error), "Voice configuration must require Pro");
	const auto freeResult = freeService.process(segmentAt(10));
	expect(freeResult.status == VoiceTriggerStatus::ProRequired, "Free processing must be rejected");
	expect(speech.calls == 0, "Free rejection must happen before local or cloud speech processing");

	TriggerEngine triggerEngine(true);
	VoiceTriggerService service(speech, triggerEngine, true);
	config.preRollSeconds = 7;
	config.postRollSeconds = 3;
	config.cooldown = std::chrono::seconds(10);
	config.action = TriggerAction::SaveClip;
	config.phrases = {"clip", "guarda eso", "eso fue duro"};
	expect(service.configure(config, &error), "valid Pro Voice configuration must be accepted");

	const auto detected = service.process(segmentAt(100));
	expect(detected.status == VoiceTriggerStatus::EventCreated && detected.event.has_value(),
	       "configured phrase must create a trigger event");
	expect(detected.event->captureStart == segmentAt(100).startedAt - std::chrono::seconds(7) &&
		       detected.event->captureEnd == segmentAt(100).startedAt + std::chrono::seconds(3),
	       "Voice event must preserve pre-roll and post-roll");
	expect(detected.event->action == TriggerAction::SaveClip, "Voice event must preserve configured action");

	const auto cooldown = service.process(segmentAt(105));
	expect(cooldown.status == VoiceTriggerStatus::Cooldown,
	       "matching phrase inside cooldown must not create another event");
	expect(service.process(segmentAt(111)).status == VoiceTriggerStatus::EventCreated,
	       "matching phrase after cooldown must create a new event");

	speech.next.transcript = "conversación normal sin llamada";
	expect(service.process(segmentAt(130)).status == VoiceTriggerStatus::NoPhraseMatch,
	       "unconfigured speech must not create an event");
	speech.next = {"guarda eso", "es", 0.30, SpeechRecognitionError::None, {}};
	expect(service.process(segmentAt(140)).status == VoiceTriggerStatus::LowConfidence,
	       "low ASR confidence must be rejected");

	config.cooldown = std::chrono::milliseconds(0);
	config.phrases.push_back("saca clip");
	expect(service.configure(config, &error), "native recognition fixture must configure");
	SpeechRecognitionResult nativeRecognition{"saca clip", "es", 0.98, SpeechRecognitionError::None, {}};
	const auto nativeResult = service.processRecognition(nativeRecognition, segmentAt(180).startedAt);
	expect(nativeResult.status == VoiceTriggerStatus::EventCreated && nativeResult.match &&
		       nativeResult.match->configuredPhrase == "saca clip",
	       "native streaming recognition must use the bilingual phrase matcher");

	config.sensitivity = 94;
	expect(service.configure(config, &error), "high sensitivity native recognition fixture must configure");
	const auto quantizedModelResult = service.processRecognition(
		{"saca clip", "es", 0.55, SpeechRecognitionError::None, {}}, segmentAt(190).startedAt);
	expect(quantizedModelResult.status == VoiceTriggerStatus::EventCreated,
	       "an exact local Whisper command must remain actionable at high sensitivity");

	return clipcoach::test::pass("voice-trigger-service-test");
}
