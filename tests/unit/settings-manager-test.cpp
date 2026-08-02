#include "test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		const auto uniqueValue = std::chrono::steady_clock::now().time_since_epoch().count();
		path_ = std::filesystem::temp_directory_path() /
			("clipcoach-settings-test-" + std::to_string(uniqueValue));
		std::filesystem::create_directories(path_);
	}

	~TemporaryDirectory()
	{
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}

	TemporaryDirectory(const TemporaryDirectory &) = delete;
	TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

	[[nodiscard]] const std::filesystem::path &path() const noexcept { return path_; }

private:
	std::filesystem::path path_;
};

} // namespace

int main()
{
	TemporaryDirectory temporaryDirectory;
	const auto settingsPath = temporaryDirectory.path() / "settings.json";

	clipcoach::SettingsManager firstRun(settingsPath);
	std::string error;

	clipcoach::test::expect(firstRun.load(&error), "first load must create the initial settings file");
	clipcoach::test::expect(error.empty(), "first load must not produce an error");
	clipcoach::test::expect(std::filesystem::exists(settingsPath), "settings file must exist after first load");
	clipcoach::test::expect(firstRun.settings().language == "system", "first load must expose defaults");
	clipcoach::test::expect(firstRun.settings().verticalSceneName == "ClipXtudio Vertical",
				"first load must select the managed vertical OBS scene by default");

	auto modified = firstRun.settings();
	modified.language = "es-ES";
	modified.notificationsEnabled = false;
	modified.quickDurationsSeconds = {20, 40, 80, 160, 300};
	modified.startWithObs = true;
	modified.autoStartReplayBuffer = true;
	modified.defaultDurationSeconds = 60;
	modified.preRollSeconds = 10;
	modified.postRollSeconds = 5;
	modified.outputMode = clipcoach::CaptureOutputMode::Both;
	modified.openDockAtStartup = false;
	modified.initialSetupCompleted = true;
	modified.confirmBeforeDelete = false;
	modified.clipDirectory = temporaryDirectory.path() / "clips";
	modified.exportDirectory = temporaryDirectory.path() / "exports";
	modified.thumbnailDirectory = temporaryDirectory.path() / "thumbnails";
	modified.includeScoreInFileName = true;
	modified.includeDateInFileName = false;
	modified.includeTriggerInFileName = false;
	modified.includeOrientationInFileName = true;
	modified.fileNameTemplate = "{date}_{score}_{trigger}_{title}";
	modified.soundOnSave = true;
	modified.systemToastEnabled = true;
	modified.notificationDetail = clipcoach::NotificationDetail::Detailed;
	modified.markMomentHotkey = "Ctrl+Shift+M";
	modified.save15Hotkey = "Ctrl+1";
	modified.save30Hotkey = "Ctrl+2";
	modified.save60Hotkey = "Ctrl+3";
	modified.saveVerticalHotkey = "Ctrl+4";
	modified.openVerticalDesignerHotkey = "Ctrl+5";
	modified.exportContainer = clipcoach::ExportContainer::Mov;
	modified.exportCodec = clipcoach::ExportCodec::Hevc;
	modified.exportFps = 60;
	modified.exportQuality = clipcoach::ExportQuality::High;
	modified.verticalResolution = clipcoach::VerticalResolution::Portrait720;
	modified.verticalWidth = 720;
	modified.verticalHeight = 1280;
	modified.verticalTemplate = clipcoach::VerticalTemplateId::TechReview;
	modified.verticalElementsEnabled = {true, false, true, true, true, false};
	modified.verticalSceneName = "Gameplay";
	modified.verticalSourceName = "Camera";
	modified.verticalZoomPercent = 160;
	modified.verticalPanXPercent = 20;
	modified.verticalPanYPercent = -15;
	modified.triggerManualEnabled = false;
	modified.triggerAudioSpikeEnabled = true;
	modified.triggerKeywordEnabled = true;
	modified.triggerSensitivity = 82;
	modified.triggerAction = clipcoach::TriggerAction::SaveClip;
	modified.triggerKeywordsCsv = "victoria,clutch";
	modified.triggerScenesCsv = "Gameplay,Final";
	modified.triggerCooldownSeconds = 25;
	modified.voiceTriggerPhrasesCsv = "clip,save that,modo grinch";
	modified.voiceTriggerLanguage = "es";
	modified.voiceAudioSourceName = "Mic/Aux";
	modified.voiceSpeechMode = clipcoach::SpeechProcessingMode::Local;
	modified.voiceCloudConsent = false;

	clipcoach::test::expect(firstRun.save(modified, &error), "modified settings must be saved");
	clipcoach::test::expect(error.empty(), "successful save must clear the error");

	clipcoach::SettingsManager secondRun(settingsPath);
	clipcoach::test::expect(secondRun.load(&error), "saved settings must load in a new manager");
	clipcoach::test::expect(secondRun.settings().language == "es-ES", "language must persist");
	clipcoach::test::expect(!secondRun.settings().notificationsEnabled, "notification preference must persist");
	clipcoach::test::expect(secondRun.settings().quickDurationsSeconds == std::vector<int>({20, 40, 80, 160, 300}),
				"quick capture durations must persist");
	clipcoach::test::expect(
		secondRun.settings().startWithObs && secondRun.settings().autoStartReplayBuffer &&
			!secondRun.settings().openDockAtStartup && secondRun.settings().initialSetupCompleted &&
			secondRun.settings().defaultDurationSeconds == 60 &&
			secondRun.settings().preRollSeconds == 10 && secondRun.settings().postRollSeconds == 5 &&
			!secondRun.settings().confirmBeforeDelete &&
			secondRun.settings().outputMode == clipcoach::CaptureOutputMode::Both,
		"general and capture settings must persist");
	clipcoach::test::expect(secondRun.settings().clipDirectory == temporaryDirectory.path() / "clips" &&
					secondRun.settings().exportDirectory == temporaryDirectory.path() / "exports",
				"directory settings must persist");
	clipcoach::test::expect(secondRun.settings().fileNameTemplate == "{date}_{score}_{trigger}_{title}" &&
					secondRun.settings().includeScoreInFileName &&
					!secondRun.settings().includeDateInFileName &&
					!secondRun.settings().includeTriggerInFileName &&
					secondRun.settings().includeOrientationInFileName,
				"file naming settings must persist");
	clipcoach::test::expect(secondRun.settings().notificationDetail == clipcoach::NotificationDetail::Detailed &&
					secondRun.settings().soundOnSave && secondRun.settings().systemToastEnabled &&
					secondRun.settings().markMomentHotkey == "Ctrl+Shift+M" &&
					secondRun.settings().save15Hotkey == "Ctrl+1" &&
					secondRun.settings().save30Hotkey == "Ctrl+2" &&
					secondRun.settings().save60Hotkey == "Ctrl+3" &&
					secondRun.settings().saveVerticalHotkey == "Ctrl+4" &&
					secondRun.settings().openVerticalDesignerHotkey == "Ctrl+5",
				"notification and hotkey settings must persist");
	clipcoach::test::expect(
		secondRun.settings().exportContainer == clipcoach::ExportContainer::Mov &&
			secondRun.settings().exportCodec == clipcoach::ExportCodec::Hevc &&
			secondRun.settings().exportFps == 60 &&
			secondRun.settings().exportQuality == clipcoach::ExportQuality::High &&
			secondRun.settings().verticalResolution == clipcoach::VerticalResolution::Portrait720 &&
			secondRun.settings().verticalWidth == 720 && secondRun.settings().verticalHeight == 1280 &&
			secondRun.settings().verticalTemplate == clipcoach::VerticalTemplateId::TechReview &&
			secondRun.settings().verticalSceneName == "Gameplay" &&
			secondRun.settings().verticalSourceName == "Camera" &&
			secondRun.settings().verticalZoomPercent == 160 &&
			secondRun.settings().verticalPanXPercent == 20 &&
			secondRun.settings().verticalPanYPercent == -15 &&
			secondRun.settings().verticalElementsEnabled ==
				std::array<bool, 6>({true, false, true, true, true, false}),
		"export settings must persist");
	clipcoach::test::expect(
		!secondRun.settings().triggerManualEnabled && secondRun.settings().triggerAudioSpikeEnabled &&
			secondRun.settings().triggerKeywordEnabled && secondRun.settings().triggerSensitivity == 82 &&
			secondRun.settings().triggerAction == clipcoach::TriggerAction::SaveClip &&
			secondRun.settings().triggerKeywordsCsv == "victoria,clutch" &&
			secondRun.settings().triggerScenesCsv == "Gameplay,Final" &&
			secondRun.settings().triggerCooldownSeconds == 25,
		"smart trigger settings must persist");
	clipcoach::test::expect(secondRun.settings().voiceTriggerPhrasesCsv == "clip,save that,modo grinch" &&
					secondRun.settings().voiceTriggerLanguage == "es" &&
					secondRun.settings().voiceAudioSourceName == "Mic/Aux" &&
					secondRun.settings().voiceSpeechMode ==
						clipcoach::SpeechProcessingMode::Local &&
					!secondRun.settings().voiceCloudConsent,
				"Voice Trigger local processing settings must persist");
	const auto profilePath = temporaryDirectory.path() / "clipxtudio-profile.json";
	clipcoach::test::expect(secondRun.exportProfile(profilePath, &error) &&
					std::filesystem::is_regular_file(profilePath),
				"a complete portable JSON profile must be exported");
	auto changedAfterExport = secondRun.settings();
	changedAfterExport.voiceTriggerPhrasesCsv = "temporary phrase";
	changedAfterExport.verticalSceneName = "Temporary Scene";
	changedAfterExport.outputMode = clipcoach::CaptureOutputMode::Horizontal;
	clipcoach::test::expect(secondRun.save(changedAfterExport, &error),
				"settings must be changeable after exporting a profile");
	clipcoach::test::expect(secondRun.importProfile(profilePath, &error) &&
					secondRun.settings().voiceTriggerPhrasesCsv == "clip,save that,modo grinch" &&
					secondRun.settings().verticalSceneName == "Gameplay" &&
					secondRun.settings().outputMode == clipcoach::CaptureOutputMode::Both,
				"importing a profile must atomically restore voice, vertical and capture settings");
	const auto corruptProfile = temporaryDirectory.path() / "corrupt-profile.json";
	{
		std::ofstream output(corruptProfile);
		output << "{not valid json}";
	}
	const auto settingsBeforeCorruptImport = secondRun.settings();
	clipcoach::test::expect(!secondRun.importProfile(corruptProfile, &error) &&
					secondRun.settings().voiceTriggerPhrasesCsv ==
						settingsBeforeCorruptImport.voiceTriggerPhrasesCsv &&
					secondRun.settings().verticalSceneName ==
						settingsBeforeCorruptImport.verticalSceneName,
				"an invalid profile must be rejected without changing active settings");
	auto emptySceneSettings = secondRun.settings();
	emptySceneSettings.verticalSceneName.clear();
	clipcoach::test::expect(secondRun.save(emptySceneSettings, &error),
				"legacy empty vertical scene fixture must be saved");
	clipcoach::SettingsManager repairedRun(settingsPath);
	clipcoach::test::expect(repairedRun.load(&error) &&
					repairedRun.settings().verticalSceneName == "ClipXtudio Vertical",
				"an existing empty vertical scene must be repaired to the managed default on load");

	return clipcoach::test::pass("settings-manager-test");
}
