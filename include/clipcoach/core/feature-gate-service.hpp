#pragma once

#include <atomic>
#include <string>

namespace clipcoach {

enum class Feature {
	ManualCapture,
	BasicHotkeys,
	ThreeQuickDurations,
	CurrentSessionHistory,
	BasicHorizontalExport,
	LimitedVerticalExport,
	BasicVerticalTemplate,
	UnlimitedDurations,
	VerticalCanvas,
	HorizontalAndVertical,
	VoiceTrigger,
	AudioSpike,
	ChatPulse,
	SceneTrigger,
	AdvancedClipScore,
	AiTitles,
	AiCaptions,
	AutoSubtitles,
	BatchExport,
	FullHistory,
	PremiumVerticalTemplates,
	SessionRecap,
	AiHookFinder,
};

enum class Plan {
	Free,
	Pro,
};

enum class EntitlementState {
	Free,
	ProActive,
	ProOfflineGrace,
	Expired,
	Revoked,
};

struct FeatureGateDecision {
	bool allowed{false};
	std::string code;
	std::string message;
};

class PlanPolicy final {
public:
	[[nodiscard]] static bool allows(Plan plan, Feature feature) noexcept;
	[[nodiscard]] static bool isFreeQuickDuration(int seconds) noexcept;
	[[nodiscard]] static const char *featureName(Feature feature) noexcept;
};

class FeatureGateService final {
public:
	explicit FeatureGateService(EntitlementState state = EntitlementState::Free) noexcept;

	void setEntitlementState(EntitlementState state) noexcept;
	[[nodiscard]] EntitlementState entitlementState() const noexcept;
	[[nodiscard]] Plan effectivePlan() const noexcept;
	[[nodiscard]] bool isAllowed(Feature feature) const noexcept;
	[[nodiscard]] FeatureGateDecision check(Feature feature) const;

private:
	std::atomic<EntitlementState> state_;
};

} // namespace clipcoach
