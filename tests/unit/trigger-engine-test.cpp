#include "test-support.hpp"

#include <clipcoach/core/trigger-engine.hpp>

#include <chrono>

#define assert(condition) \
	clipcoach::test::expect(static_cast<bool>(condition), #condition)
namespace {
using namespace clipcoach;

TriggerConfiguration enabledConfig(TriggerAction action = TriggerAction::MarkMoment)
{
	TriggerConfiguration config;
	config.enabled = true;
	config.sensitivity = 60;
	config.preRollSeconds = 8;
	config.postRollSeconds = 4;
	config.action = action;
	config.cooldown = std::chrono::seconds(10);
	config.duplicateWindow = std::chrono::seconds(2);
	return config;
}

TriggerSignal signal(SmartTriggerType type, std::chrono::system_clock::time_point at)
{
	TriggerSignal value;
	value.type = type;
	value.occurredAt = at;
	value.durationSeconds = 30;
	switch (type) {
	case SmartTriggerType::Manual:
		value.manualMarker = true;
		break;
	case SmartTriggerType::Voice:
		value.voiceConfidence = 0.9;
		break;
	case SmartTriggerType::AudioSpike:
		value.audioIntensity = 0.9;
		break;
	case SmartTriggerType::ChatPulse:
		value.chatActivity = 0.9;
		break;
	case SmartTriggerType::Scene:
		value.sceneRelevance = 0.9;
		break;
	case SmartTriggerType::Keyword:
		value.keywordStrength = 0.9;
		break;
	case SmartTriggerType::FutureAiHook:
		value.aiConfidence = 0.9;
		break;
	}
	return value;
}
} // namespace

int main()
{
	using namespace clipcoach;
	using namespace std::chrono;
	const auto now = system_clock::time_point(seconds(1000));

	TriggerEngine freeEngine(false);
	auto manualConfig = enabledConfig();
	assert(freeEngine.setConfiguration(SmartTriggerType::Manual, manualConfig));
	const auto manual = freeEngine.process(signal(SmartTriggerType::Manual, now));
	assert(manual.event.has_value());
	assert(manual.event->captureStart == now - seconds(8));
	assert(manual.event->captureEnd == now + seconds(4));
	assert(manual.event->primaryType == SmartTriggerType::Manual);

	auto audioConfig = enabledConfig();
	assert(!freeEngine.setConfiguration(SmartTriggerType::AudioSpike, audioConfig));

	TriggerEngine engine(true);
	for (const auto type : {SmartTriggerType::Voice, SmartTriggerType::AudioSpike, SmartTriggerType::ChatPulse,
				SmartTriggerType::FutureAiHook}) {
		assert(engine.setConfiguration(type, enabledConfig()));
		auto current = signal(type, now + seconds(static_cast<int>(type) * 3));
		assert(engine.process(current).event.has_value());
	}

	auto keywordConfig = enabledConfig();
	keywordConfig.keywords = {"victoria", "clutch"};
	assert(engine.setConfiguration(SmartTriggerType::Keyword, keywordConfig));
	auto keywordSignal = signal(SmartTriggerType::Keyword, now + seconds(30));
	keywordSignal.text = "Una victoria increíble";
	keywordSignal.keyword = "victoria";
	assert(engine.process(keywordSignal).event.has_value());
	keywordSignal.occurredAt += seconds(3);
	keywordSignal.text = "texto irrelevante";
	keywordSignal.keyword.clear();
	assert(engine.process(keywordSignal).rejection == TriggerRejection::NotConfigured);

	auto sceneConfig = enabledConfig();
	sceneConfig.scenes = {"Gameplay", "Final"};
	assert(engine.setConfiguration(SmartTriggerType::Scene, sceneConfig));
	auto sceneSignal = signal(SmartTriggerType::Scene, now + seconds(40));
	sceneSignal.scene = "Gameplay";
	assert(engine.process(sceneSignal).event.has_value());
	sceneSignal.occurredAt += seconds(3);
	sceneSignal.scene = "Pausa";
	assert(engine.process(sceneSignal).rejection == TriggerRejection::NotConfigured);

	TriggerEngine disabled(true);
	auto disabledConfig = enabledConfig();
	disabledConfig.enabled = false;
	assert(disabled.setConfiguration(SmartTriggerType::AudioSpike, disabledConfig));
	assert(disabled.process(signal(SmartTriggerType::AudioSpike, now)).rejection == TriggerRejection::Disabled);
	auto strictConfig = enabledConfig();
	strictConfig.sensitivity = 95;
	assert(disabled.setConfiguration(SmartTriggerType::AudioSpike, strictConfig));
	assert(disabled.process(signal(SmartTriggerType::AudioSpike, now)).rejection ==
	       TriggerRejection::BelowThreshold);

	TriggerEngine duplicate(true);
	assert(duplicate.setConfiguration(SmartTriggerType::AudioSpike, enabledConfig()));
	assert(duplicate.process(signal(SmartTriggerType::AudioSpike, now)).event);
	assert(duplicate.process(signal(SmartTriggerType::AudioSpike, now + seconds(1))).rejection ==
	       TriggerRejection::Duplicate);

	TriggerEngine cooldown(true);
	auto saveConfig = enabledConfig(TriggerAction::SaveClip);
	assert(cooldown.setConfiguration(SmartTriggerType::AudioSpike, saveConfig));
	assert(cooldown.setConfiguration(SmartTriggerType::ChatPulse, saveConfig));
	assert(cooldown.process(signal(SmartTriggerType::AudioSpike, now)).event);
	assert(cooldown.process(signal(SmartTriggerType::ChatPulse, now + seconds(3))).rejection ==
	       TriggerRejection::Cooldown);
	assert(cooldown.process(signal(SmartTriggerType::ChatPulse, now + seconds(11))).event);
	for (const auto mediaAction : {TriggerAction::SaveVerticalClip, TriggerAction::SaveBoth}) {
		TriggerEngine mediaEngine(true);
		auto mediaConfig = enabledConfig(mediaAction);
		assert(mediaEngine.setConfiguration(SmartTriggerType::Voice, mediaConfig));
		const auto mediaResult = mediaEngine.process(signal(SmartTriggerType::Voice, now));
		assert(mediaResult.event && mediaResult.event->action == mediaAction);
		assert(std::string(triggerActionName(mediaAction)).find("save_") == 0);
	}
	assert(std::string(triggerActionName(TriggerAction::AddToRecommended)) == "add_to_recommended");

	TriggerEngine combinedEngine(true);
	int callbackCount = 0;
	combinedEngine.setEventCallback([&callbackCount](const TriggerEvent &) { ++callbackCount; });
	assert(combinedEngine.setConfiguration(SmartTriggerType::AudioSpike, enabledConfig()));
	assert(combinedEngine.setConfiguration(SmartTriggerType::ChatPulse, enabledConfig()));
	assert(combinedEngine.setConfiguration(SmartTriggerType::Keyword, keywordConfig));
	auto audio = signal(SmartTriggerType::AudioSpike, now);
	auto chat = signal(SmartTriggerType::ChatPulse, now);
	auto word = signal(SmartTriggerType::Keyword, now);
	word.keyword = "clutch";
	word.text = "clutch total";
	const auto combinedResult = combinedEngine.evaluateMoment({audio, chat, word});
	assert(combinedResult.event);
	assert(combinedResult.event->contributingTypes.size() == 3);
	assert(combinedResult.event->score > ScoreEngine{}.calculate(audio));
	assert(combinedEngine.recentEvents().size() == 1);
	assert(callbackCount == 1);
	(void)combinedEngine.evaluateMoment({audio, chat, word});
	assert(callbackCount == 1);

	return clipcoach::test::pass("trigger-engine-test");
}
