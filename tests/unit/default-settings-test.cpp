#include "test-support.hpp"

#include <clipcoach/core/settings.hpp>

#include <vector>

int main()
{
	const auto settings = clipcoach::Settings::defaults();

	clipcoach::test::expect(settings.schemaVersion == 14, "initial schema version must be 14");
	clipcoach::test::expect(!settings.initialSetupCompleted,
				"initial microphone and vertical scene setup must run once");
	clipcoach::test::expect(settings.voiceSpeechMode == clipcoach::SpeechProcessingMode::Local &&
					!settings.voiceCloudConsent,
				"Voice Trigger must default to local processing without cloud consent");
	clipcoach::test::expect(!settings.aiAssistantEnabled && !settings.aiPrivacyConsent &&
					settings.aiLanguage == "auto",
				"AI Assistant must default to disabled without inferred consent");
	clipcoach::test::expect(settings.language == "system", "initial language must follow the operating system");
	clipcoach::test::expect(settings.notificationsEnabled, "notifications must be enabled initially");
	clipcoach::test::expect(settings.verticalWidth == 1080, "initial vertical width must be 1080");
	clipcoach::test::expect(settings.verticalHeight == 1920, "initial vertical height must be 1920");
	clipcoach::test::expect(settings.quickDurationsSeconds == std::vector<int>({15, 30, 60, 120, 300}),
				"Free quick durations must expose the five product presets");
	clipcoach::test::expect(settings.openDockAtStartup, "dock must open at startup by default");
	clipcoach::test::expect(!settings.autoStartReplayBuffer, "Replay Buffer auto-start must be opt-in");
	clipcoach::test::expect(settings.outputMode == clipcoach::CaptureOutputMode::Vertical,
				"new and skipped setup profiles must default to vertical clips");
	clipcoach::test::expect(settings.defaultDurationSeconds == 30 && settings.preRollSeconds == 0 &&
					settings.postRollSeconds == 0,
				"capture timing defaults must be conservative");
	clipcoach::test::expect(settings.freeClipLimitPerSession == 20,
				"Free session limit must come from the settings defaults");
	clipcoach::test::expect(settings.fileNameTemplate == "{date}_{time}_{title}_{trigger}",
				"file name template must expose meaningful default tokens");

	return clipcoach::test::pass("default-settings-test");
}
