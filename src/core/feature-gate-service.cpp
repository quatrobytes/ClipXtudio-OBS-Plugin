#include <clipcoach/core/feature-gate-service.hpp>

namespace clipcoach {
namespace {

bool isFreeFeature(Feature feature) noexcept
{
	switch (feature) {
	case Feature::ManualCapture:
	case Feature::BasicHotkeys:
	case Feature::ThreeQuickDurations:
	case Feature::CurrentSessionHistory:
	case Feature::BasicHorizontalExport:
	case Feature::LimitedVerticalExport:
	case Feature::BasicVerticalTemplate:
		return true;
	case Feature::UnlimitedDurations:
	case Feature::VerticalCanvas:
	case Feature::HorizontalAndVertical:
	case Feature::VoiceTrigger:
	case Feature::AudioSpike:
	case Feature::ChatPulse:
	case Feature::SceneTrigger:
	case Feature::AdvancedClipScore:
	case Feature::AiTitles:
	case Feature::AiCaptions:
	case Feature::AutoSubtitles:
	case Feature::BatchExport:
	case Feature::FullHistory:
	case Feature::PremiumVerticalTemplates:
	case Feature::SessionRecap:
	case Feature::AiHookFinder:
		return false;
	}
	return false;
}

} // namespace

bool PlanPolicy::allows(Plan plan, Feature feature) noexcept
{
	return plan == Plan::Pro || isFreeFeature(feature);
}

bool PlanPolicy::isFreeQuickDuration(int seconds) noexcept
{
	return seconds == 15 || seconds == 30 || seconds == 60;
}

const char *PlanPolicy::featureName(Feature feature) noexcept
{
	switch (feature) {
	case Feature::ManualCapture:
		return "manual clips";
	case Feature::BasicHotkeys:
		return "basic hotkeys";
	case Feature::ThreeQuickDurations:
		return "15s, 30s and 60s quick durations";
	case Feature::CurrentSessionHistory:
		return "current session history";
	case Feature::BasicHorizontalExport:
		return "basic horizontal export";
	case Feature::LimitedVerticalExport:
		return "limited vertical export";
	case Feature::BasicVerticalTemplate:
		return "basic vertical template";
	case Feature::UnlimitedDurations:
		return "unlimited clip durations";
	case Feature::VerticalCanvas:
		return "real Vertical Canvas";
	case Feature::HorizontalAndVertical:
		return "Horizontal + Vertical output";
	case Feature::VoiceTrigger:
		return "Voice Trigger";
	case Feature::AudioSpike:
		return "Audio Spike";
	case Feature::ChatPulse:
		return "Chat Pulse";
	case Feature::SceneTrigger:
		return "Scene Trigger";
	case Feature::AdvancedClipScore:
		return "advanced Clip Score";
	case Feature::AiTitles:
		return "AI titles";
	case Feature::AiCaptions:
		return "AI captions";
	case Feature::AutoSubtitles:
		return "automatic subtitles";
	case Feature::BatchExport:
		return "batch export";
	case Feature::FullHistory:
		return "complete history";
	case Feature::PremiumVerticalTemplates:
		return "premium vertical templates";
	case Feature::SessionRecap:
		return "Session Recap";
	case Feature::AiHookFinder:
		return "AI Hook Finder";
	}
	return "this feature";
}

FeatureGateService::FeatureGateService(EntitlementState state) noexcept : state_(state) {}

void FeatureGateService::setEntitlementState(EntitlementState state) noexcept
{
	state_.store(state, std::memory_order_release);
}

EntitlementState FeatureGateService::entitlementState() const noexcept
{
	return state_.load(std::memory_order_acquire);
}

Plan FeatureGateService::effectivePlan() const noexcept
{
	const auto state = entitlementState();
	return state == EntitlementState::ProActive || state == EntitlementState::ProOfflineGrace ? Plan::Pro
												  : Plan::Free;
}

bool FeatureGateService::isAllowed(Feature feature) const noexcept
{
	return PlanPolicy::allows(effectivePlan(), feature);
}

FeatureGateDecision FeatureGateService::check(Feature feature) const
{
	if (isAllowed(feature)) {
		return {true, {}, {}};
	}
	return {false, "PRO_REQUIRED",
		std::string(PlanPolicy::featureName(feature)) + " requires ClipXtudio Pro"};
}

} // namespace clipcoach
