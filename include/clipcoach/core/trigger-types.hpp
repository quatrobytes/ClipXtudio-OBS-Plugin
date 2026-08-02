#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace clipcoach {

enum class SmartTriggerType {
	Manual,
	Voice,
	AudioSpike,
	ChatPulse,
	Scene,
	Keyword,
	FutureAiHook,
};

enum class TriggerAction {
	SaveClip,
	MarkMoment,
	AddToRecommended,
	SaveVerticalClip,
	SaveBoth,
};

struct TriggerConfiguration {
	bool enabled{false};
	int preRollSeconds{5};
	int postRollSeconds{5};
	int sensitivity{70};
	TriggerAction action{TriggerAction::MarkMoment};
	std::vector<std::string> keywords;
	std::vector<std::string> scenes;
	std::chrono::milliseconds cooldown{15000};
	std::chrono::milliseconds duplicateWindow{2500};
};

struct TriggerSignal {
	SmartTriggerType type{SmartTriggerType::Manual};
	std::chrono::system_clock::time_point occurredAt{
		std::chrono::system_clock::now()};
	double audioIntensity{0.0};
	double chatActivity{0.0};
	double voiceConfidence{0.0};
	double keywordStrength{0.0};
	double sceneRelevance{0.0};
	double aiConfidence{0.0};
	bool manualMarker{false};
	int durationSeconds{30};
	std::string text;
	std::string keyword;
	std::string scene;
};

struct TriggerEvent {
	std::string id;
	SmartTriggerType primaryType{SmartTriggerType::Manual};
	std::vector<SmartTriggerType> contributingTypes;
	std::chrono::system_clock::time_point occurredAt;
	std::chrono::system_clock::time_point captureStart;
	std::chrono::system_clock::time_point captureEnd;
	TriggerAction action{TriggerAction::MarkMoment};
	int score{0};
	std::string keyword;
	std::string scene;
};

[[nodiscard]] const char *triggerTypeName(SmartTriggerType type) noexcept;
[[nodiscard]] const char *triggerActionName(TriggerAction action) noexcept;

} // namespace clipcoach
