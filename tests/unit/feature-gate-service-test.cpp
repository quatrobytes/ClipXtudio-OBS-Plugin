#include <clipcoach/core/feature-gate-service.hpp>

#include "test-support.hpp"

int main()
{
	using namespace clipcoach;

	FeatureGateService gates;
	for (const auto feature : {Feature::ManualCapture, Feature::BasicHotkeys, Feature::ThreeQuickDurations,
				   Feature::CurrentSessionHistory, Feature::BasicHorizontalExport,
				   Feature::LimitedVerticalExport, Feature::BasicVerticalTemplate}) {
		clipcoach::test::expect(gates.isAllowed(feature), "Free must retain its useful baseline");
	}
	for (const auto feature :
	     {Feature::UnlimitedDurations, Feature::VerticalCanvas, Feature::HorizontalAndVertical,
	      Feature::VoiceTrigger, Feature::AudioSpike, Feature::ChatPulse, Feature::SceneTrigger,
	      Feature::AdvancedClipScore, Feature::AiTitles, Feature::AiCaptions, Feature::AutoSubtitles,
	      Feature::BatchExport, Feature::FullHistory, Feature::PremiumVerticalTemplates, Feature::SessionRecap}) {
		clipcoach::test::expect(!gates.isAllowed(feature), "Free must reject Pro features");
		clipcoach::test::expect(gates.check(feature).code == "PRO_REQUIRED",
					"denied features need a stable product error");
	}

	gates.setEntitlementState(EntitlementState::ProActive);
	for (const auto feature :
	     {Feature::UnlimitedDurations, Feature::VerticalCanvas, Feature::HorizontalAndVertical,
	      Feature::VoiceTrigger, Feature::AudioSpike, Feature::ChatPulse, Feature::SceneTrigger,
	      Feature::AdvancedClipScore, Feature::AiTitles, Feature::AiCaptions, Feature::AutoSubtitles,
	      Feature::BatchExport, Feature::FullHistory, Feature::PremiumVerticalTemplates, Feature::SessionRecap,
	      Feature::AiHookFinder}) {
		clipcoach::test::expect(gates.isAllowed(feature), "active Pro must unlock every Pro feature");
	}

	gates.setEntitlementState(EntitlementState::ProOfflineGrace);
	clipcoach::test::expect(gates.isAllowed(Feature::VoiceTrigger), "offline grace must preserve Pro temporarily");

	gates.setEntitlementState(EntitlementState::Expired);
	clipcoach::test::expect(!gates.isAllowed(Feature::VoiceTrigger), "expired licenses must close Pro gates");
	clipcoach::test::expect(gates.isAllowed(Feature::ManualCapture),
				"expired licenses must fall back to the Free baseline");

	gates.setEntitlementState(EntitlementState::Revoked);
	clipcoach::test::expect(!gates.isAllowed(Feature::VerticalCanvas), "backend revocation must close Pro gates");
	clipcoach::test::expect(gates.isAllowed(Feature::BasicHorizontalExport),
				"revocation must not make the Free product unusable");
}
