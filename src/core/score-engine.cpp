#include <clipcoach/core/score-engine.hpp>

#include <algorithm>
#include <cmath>
#include <set>

namespace clipcoach {
namespace {

double normalized(double value) noexcept
{
	return std::clamp(value, 0.0, 1.0);
}

double baseScore(SmartTriggerType type) noexcept
{
	switch (type) {
	case SmartTriggerType::Manual:
		return 45.0;
	case SmartTriggerType::Voice:
		return 24.0;
	case SmartTriggerType::AudioSpike:
		return 20.0;
	case SmartTriggerType::ChatPulse:
		return 22.0;
	case SmartTriggerType::Scene:
		return 18.0;
	case SmartTriggerType::Keyword:
		return 28.0;
	case SmartTriggerType::FutureAiHook:
		return 30.0;
	}
	return 0.0;
}

double signalScore(const TriggerSignal &signal) noexcept
{
	double score = baseScore(signal.type);
	score += normalized(signal.audioIntensity) * 14.0;
	score += normalized(signal.chatActivity) * 12.0;
	score += normalized(signal.keywordStrength) * 15.0;
	score += normalized(signal.voiceConfidence) * 10.0;
	score += normalized(signal.sceneRelevance) * 8.0;
	score += normalized(signal.aiConfidence) * 12.0;
	score += signal.manualMarker ? 25.0 : 0.0;
	score += ScoreEngine::durationQuality(signal.durationSeconds) * 8.0;
	return score;
}

} // namespace

int ScoreEngine::calculate(const TriggerSignal &signal) const noexcept
{
	return std::clamp(static_cast<int>(std::lround(signalScore(signal))), 0, 100);
}

int ScoreEngine::calculate(const std::vector<TriggerSignal> &inputSignals) const noexcept
{
	if (inputSignals.empty())
		return 0;

	double strongest = 0.0;
	double supporting = 0.0;
	std::set<SmartTriggerType> types;
	for (const auto &signal : inputSignals) {
		const auto value = signalScore(signal);
		if (value > strongest) {
			supporting += strongest * 0.16;
			strongest = value;
		} else {
			supporting += value * 0.16;
		}
		types.insert(signal.type);
	}

	const auto combinationBonus = std::min(18.0, static_cast<double>(types.size() - 1) * 6.0);
	return std::clamp(static_cast<int>(std::lround(strongest + supporting + combinationBonus)), 0, 100);
}

double ScoreEngine::durationQuality(int durationSeconds) noexcept
{
	if (durationSeconds <= 0)
		return 0.0;
	if (durationSeconds <= 8)
		return static_cast<double>(durationSeconds) / 16.0;
	if (durationSeconds <= 60)
		return std::min(1.0, 0.5 + static_cast<double>(durationSeconds - 8) / 44.0);
	if (durationSeconds <= 120)
		return 1.0 - static_cast<double>(durationSeconds - 60) / 240.0;
	return std::max(0.25, 0.75 - static_cast<double>(durationSeconds - 120) / 720.0);
}

} // namespace clipcoach
