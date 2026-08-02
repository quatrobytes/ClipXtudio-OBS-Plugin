#include "test-support.hpp"

#include <clipcoach/core/score-engine.hpp>

#define assert(condition) \
	clipcoach::test::expect(static_cast<bool>(condition), #condition)

int main()
{
	using namespace clipcoach;

	ScoreEngine engine;
	TriggerSignal weakAudio;
	weakAudio.type = SmartTriggerType::AudioSpike;
	weakAudio.audioIntensity = 0.25;
	weakAudio.durationSeconds = 3;

	TriggerSignal strongAudio = weakAudio;
	strongAudio.audioIntensity = 0.95;
	strongAudio.durationSeconds = 30;
	assert(engine.calculate(strongAudio) > engine.calculate(weakAudio));

	TriggerSignal manual;
	manual.type = SmartTriggerType::Manual;
	manual.manualMarker = true;
	manual.durationSeconds = 30;
	assert(engine.calculate(manual) >= 70);

	TriggerSignal chat;
	chat.type = SmartTriggerType::ChatPulse;
	chat.chatActivity = 0.9;
	chat.durationSeconds = 30;
	TriggerSignal keyword;
	keyword.type = SmartTriggerType::Keyword;
	keyword.keywordStrength = 0.95;
	keyword.durationSeconds = 30;
	const auto combined = engine.calculate({strongAudio, chat, keyword});
	assert(combined > engine.calculate(strongAudio));
	assert(combined >= 0 && combined <= 100);

	TriggerSignal overflow = strongAudio;
	overflow.audioIntensity = 20.0;
	overflow.chatActivity = 20.0;
	overflow.keywordStrength = 20.0;
	overflow.manualMarker = true;
	assert(engine.calculate({overflow, chat, keyword, manual}) == 100);

	assert(ScoreEngine::durationQuality(0) == 0.0);
	assert(ScoreEngine::durationQuality(30) == 1.0);
	assert(ScoreEngine::durationQuality(300) < 1.0);
	return clipcoach::test::pass("score-engine-test");
}
