#include "test-support.hpp"

#include <clipcoach/integrations/chat-pulse-service.hpp>

namespace {

clipcoach::integrations::ChatMessage message(
	int second, std::string author, std::string text,
	bool moderator = false)
{
	clipcoach::integrations::ChatMessage result;
	result.id = std::to_string(second) + author;
	result.authorId = std::move(author);
	result.text = std::move(text);
	result.sentAt =
		std::chrono::system_clock::time_point(std::chrono::seconds(second));
	result.authorIsModerator = moderator;
	return result;
}

void expect(bool condition, const char *message)
{
	clipcoach::test::expect(condition, message);
}

} // namespace

int main()
{
	using namespace clipcoach;
	using namespace clipcoach::integrations;

	ChatPulseConfiguration config;
	config.enabled = true;
	config.burstMessageCount = 6;
	config.minimumUniqueAuthors = 3;
	config.emojiCount = 6;
	config.repeatedWordCount = 4;
	config.cooldown = std::chrono::seconds(5);

	ChatPulseDetector burst(config);
	std::optional<ChatPulseEvent> pulse;
	for (int index = 0; index < 6; ++index)
		pulse = burst.ingest(
			ChatPlatform::Twitch,
			message(100 + index, "user" + std::to_string(index % 3),
				"message" + std::to_string(index)));
	expect(pulse && pulse->reason == ChatPulseReason::MessageBurst,
	       "message burst must be detected");

	ChatPulseDetector command(config);
	pulse = command.ingest(ChatPlatform::YouTube,
			       message(200, "viewer", "!clip"));
	expect(pulse && pulse->reason == ChatPulseReason::ClipCommand &&
		       pulse->intensity == 1.0,
	       "!clip must create an immediate pulse");

	ChatPulseDetector emojis(config);
	pulse = emojis.ingest(
		ChatPlatform::Twitch,
		message(300, "a", "🔥🔥🔥"));
	expect(!pulse, "a partial emoji burst must not fire");
	pulse = emojis.ingest(
		ChatPlatform::Twitch,
		message(301, "b", "😂😂😂"));
	expect(pulse && pulse->reason == ChatPulseReason::EmojiBurst &&
		       pulse->emojiCount == 6,
	       "Unicode emojis across messages must create a pulse");

	ChatPulseDetector repeated(config);
	pulse.reset();
	for (int index = 0; index < 4; ++index) {
		const auto candidate = repeated.ingest(
			ChatPlatform::YouTube,
			message(400 + index, "u" + std::to_string(index),
				"clutch"));
		if (candidate)
			pulse = candidate;
	}
	expect(pulse && pulse->reason == ChatPulseReason::RepeatedWord &&
		       pulse->repeatedWord == "clutch",
	       "repeated meaningful words must create a pulse");

	TriggerEngine engine(true);
	ChatPulseService service(engine, true);
	config.action = TriggerAction::SaveClip;
	config.preRollSeconds = 8;
	config.postRollSeconds = 4;
	expect(service.configure(config),
	       "Pro Chat Pulse configuration must be accepted");
	const auto result = service.process(
		ChatPlatform::Twitch,
		message(500, "moderator", "!clip", true));
	expect(result.triggerEvent.has_value() &&
		       result.triggerEvent->primaryType ==
			       SmartTriggerType::ChatPulse &&
		       result.triggerEvent->captureStart ==
			       std::chrono::system_clock::time_point(
				       std::chrono::seconds(492)) &&
		       result.triggerEvent->captureEnd ==
			       std::chrono::system_clock::time_point(
				       std::chrono::seconds(504)),
	       "Chat Pulse must create a TriggerEngine event with roll window");
	expect(engine.recentEvents().size() == 1,
	       "Chat Pulse event must appear in TriggerEngine recent events");

	TriggerEngine freeEngine(false);
	ChatPulseService freeService(freeEngine, false);
	std::string error;
	expect(!freeService.configure(config, &error),
	       "Chat Pulse must require Pro");

	return clipcoach::test::pass("chat-pulse-test");
}
