#include "../unit/test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/network/qt-update-checker.hpp>
#include <clipcoach/ui/settings-tab.hpp>

#include <QApplication>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QFontMetrics>
#include <QGridLayout>
#include <QLineEdit>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QSettings>
#include <QSpinBox>
#include <QPushButton>
#include <QToolButton>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	const auto directory = std::filesystem::temp_directory_path() /
			       ("clipcoach-settings-ui-" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	clipcoach::SettingsManager manager(directory / "settings.json");
	std::string error;
	clipcoach::test::expect(manager.load(&error), "settings UI fixture must load");

	QSettings locale(QStringLiteral(CLIPCOACH_SOURCE_DIR "/data/locale/es-ES.ini"), QSettings::IniFormat);
	const auto translator = [&locale](const char *key) {
		return locale.value(QString::fromUtf8(key), QString::fromUtf8(key)).toString();
	};
	bool replayProfileApplied = false;
	bool restartRequested = false;
	clipcoach::ui::VerticalObsBridge replayBridge;
	replayBridge.replayEncoders = [] {
		return std::vector<clipcoach::ui::ReplayEncoderOption>{
			{"nvenc", "NVIDIA NVENC H.264", true},
			{"x264", "Software x264", false}};
	};
	replayBridge.replayProfile = [] {
		return clipcoach::ui::ReplayProfileSettings{
			"Advanced", "x264", "Software x264", false, true};
	};
	replayBridge.applyReplayProfile =
		[&replayProfileApplied](const std::string &encoder, bool enabled) {
			replayProfileApplied = encoder == "nvenc" && enabled;
			return clipcoach::ui::ReplayProfileApplyResult{
				true, true, "saved"};
		};
	replayBridge.restartObs = [&restartRequested](std::string *) {
		restartRequested = true;
		return true;
	};
	clipcoach::ui::SettingsTab tab(translator, &manager, nullptr, nullptr,
				       nullptr, replayBridge);
	tab.resize(520, 820);
	tab.show();
	application.processEvents();

	for (const auto *name :
	     {"settingsGeneralSection",
	      "settingsProfileSection",
	      "settingsReplayPerformanceSection", "settingsPathsSection", "settingsFileNamesSection",
	      "settingsNotificationsSection", "settingsHotkeysSection", "settingsExportSection",
	      "settingsIntegrationsSection", "settingsAiSection", "settingsProAccountSection",
	      "settingsRemoteClipperSection",
	      "settingsSupportSection"}) {
		clipcoach::test::expect(tab.findChild<QWidget *>(QString::fromLatin1(name)) != nullptr,
					"every required settings section must exist");
	}
	auto *sectionsContainer =
		tab.findChild<QWidget *>(QStringLiteral("settingsSectionsContainer"));
	auto *sectionsGrid =
		sectionsContainer != nullptr
			? qobject_cast<QGridLayout *>(sectionsContainer->layout())
			: nullptr;
	clipcoach::test::expect(
		sectionsGrid != nullptr && sectionsGrid->count() == 13,
		"Settings must organize the thirteen non-duplicated sections in one responsive card grid");
	for (const auto *name :
	     {"settingsGeneralSection",
	      "settingsProfileSection",
	      "settingsReplayPerformanceSection", "settingsPathsSection",
	      "settingsFileNamesSection", "settingsNotificationsSection", "settingsHotkeysSection",
	      "settingsExportSection", "settingsIntegrationsSection", "settingsAiSection",
	      "settingsProAccountSection", "settingsRemoteClipperSection", "settingsSupportSection"}) {
		auto *section = tab.findChild<QWidget *>(QString::fromLatin1(name));
		clipcoach::test::expect(
			section != nullptr &&
				section->property("settingsRole").toString() ==
					QStringLiteral("section") &&
				!section->property("sectionTone").toString().isEmpty(),
			"every Settings block must use the shared modern card treatment");
	}
	auto *remoteStatusGrid =
		tab.findChild<QWidget *>(QStringLiteral("remoteClipperStatusGrid"));
	const auto remoteStatusTiles =
		remoteStatusGrid != nullptr
			? remoteStatusGrid->findChildren<QWidget *>(
				  QStringLiteral("RemoteClipperStatusTile"),
				  Qt::FindDirectChildrenOnly)
			: QList<QWidget *>();
	clipcoach::test::expect(
		remoteStatusGrid != nullptr && remoteStatusTiles.size() == 3 &&
			qobject_cast<QGridLayout *>(remoteStatusGrid->layout()) != nullptr &&
			qobject_cast<QGridLayout *>(remoteStatusGrid->layout())
				->itemAtPosition(1, 0) != nullptr &&
			qobject_cast<QGridLayout *>(remoteStatusGrid->layout())
				->itemAtPosition(2, 0) != nullptr &&
			tab.findChild<QLabel *>(QStringLiteral("remoteClipperConnectionStatus"))
				->text() == translator("Settings.RemoteClipper.Offline") &&
			!translator("Settings.RemoteClipper.Description")
				.startsWith(QStringLiteral("Settings.")),
		"Remote Clipper settings must be localized and grouped into connection, session and activity cards");
	clipcoach::remote::RemoteClipperStatus unauthorizedRemote;
	unauthorizedRemote.connection = clipcoach::remote::RemoteConnectionState::Unauthorized;
	unauthorizedRemote.pollIntervalSeconds = 3;
	tab.setRemoteClipperStatus(unauthorizedRemote);
	auto *remoteAuthenticate = tab.findChild<QPushButton *>(
		QStringLiteral("remoteClipperAuthenticateButton"));
	clipcoach::test::expect(
		remoteAuthenticate != nullptr && remoteAuthenticate->isHidden() &&
			!remoteAuthenticate->isEnabled() &&
			remoteAuthenticate->text() ==
				translator("Settings.RemoteClipper.Authenticate"),
		"Remote authentication must be moved out of Settings into the global header");

	auto *dockToggle = tab.findChild<QCheckBox *>(QStringLiteral("openDockAtStartupCheck"));
	auto *exportProfile = tab.findChild<QPushButton *>(
		QStringLiteral("exportSettingsProfileButton"));
	auto *importProfile = tab.findChild<QPushButton *>(
		QStringLiteral("importSettingsProfileButton"));
	auto *openSetup = tab.findChild<QPushButton *>(
		QStringLiteral("openInitialSetupButton"));
	auto *profileDescription = tab.findChild<QLabel *>(
		QStringLiteral("settingsProfileDescription"));
	auto *pluginLanguage = tab.findChild<QComboBox *>(QStringLiteral("pluginLanguageCombo"));
	auto *verticalResolution =
		tab.findChild<QComboBox *>(QStringLiteral("verticalResolutionCombo"));
	auto *templateEdit = tab.findChild<QLineEdit *>(QStringLiteral("fileNameTemplateEdit"));
	bool setupRequested = false;
	tab.setSetupRequestedCallback([&setupRequested] { setupRequested = true; });
	if (openSetup != nullptr)
		openSetup->click();
	clipcoach::test::expect(dockToggle != nullptr &&
					exportProfile != nullptr && importProfile != nullptr &&
					openSetup != nullptr && setupRequested &&
					profileDescription != nullptr && profileDescription->wordWrap() &&
					pluginLanguage != nullptr && pluginLanguage->count() == 3 &&
					verticalResolution != nullptr &&
					verticalResolution->count() == 6 &&
					templateEdit != nullptr,
				"core settings controls must be native Qt widgets");
	const auto profilePath = directory / "portable-profile.json";
	clipcoach::test::expect(tab.exportProfileTo(profilePath, &error),
			       "Settings must export a JSON profile without requiring Pro");
	{
		std::ifstream input(profilePath);
		std::ostringstream document;
		document << input.rdbuf();
		clipcoach::test::expect(
			document.str().find("voice_trigger_phrases") != std::string::npos &&
				document.str().find("vertical_scene_name") != std::string::npos &&
				document.str().find("license") == std::string::npos &&
				document.str().find("token") == std::string::npos,
			"exported profiles must contain workflow settings but no license or token material");
	}
	auto changed = manager.settings();
	changed.voiceTriggerPhrasesCsv = "temporary";
	changed.verticalSceneName = "Temporary";
	clipcoach::test::expect(manager.save(changed, &error),
			       "settings profile fixture must be mutable");
	bool profileApplied = false;
	tab.setAppliedCallback([&profileApplied](const clipcoach::Settings &before,
						 const clipcoach::Settings &after) {
		profileApplied = before.voiceTriggerPhrasesCsv != after.voiceTriggerPhrasesCsv;
	});
	clipcoach::test::expect(
		tab.importProfileFrom(profilePath, &error) && profileApplied &&
			manager.settings().voiceTriggerPhrasesCsv != "temporary" &&
			manager.settings().verticalSceneName == "ClipXtudio Vertical",
		"importing a profile must restore all settings and notify the runtime");
	clipcoach::test::expect(
		tab.findChild<QWidget *>(QStringLiteral("settingsCaptureSection")) == nullptr &&
			tab.findChild<QSpinBox *>(QStringLiteral("defaultDurationSpin")) == nullptr &&
		tab.findChild<QSpinBox *>(QStringLiteral("preRollSpin")) == nullptr &&
			tab.findChild<QSpinBox *>(QStringLiteral("postRollSpin")) == nullptr &&
			tab.findChild<QCheckBox *>(QStringLiteral("soundOnSaveCheck")) == nullptr &&
			tab.findChild<QCheckBox *>(QStringLiteral("systemToastCheck")) == nullptr,
		"Settings must not duplicate Trigger timing or expose invasive Windows sound controls");
	clipcoach::test::expect(
		verticalResolution->findData(static_cast<int>(
			clipcoach::VerticalResolution::Portrait4320)) >= 0,
		"Settings must expose the 8K vertical preset using enum-backed data");
	auto *replayEncoder = tab.findChild<QComboBox *>(
		QStringLiteral("settingsReplayEncoderCombo"));
	auto *replayEnabled = tab.findChild<QCheckBox *>(
		QStringLiteral("settingsReplayEnabledCheck"));
	auto *replayApply = tab.findChild<QPushButton *>(
		QStringLiteral("settingsReplayApplyButton"));
	auto *replayStatus = tab.findChild<QLabel *>(
		QStringLiteral("settingsReplayProfileStatus"));
	clipcoach::test::expect(
		replayEncoder != nullptr && replayEncoder->count() == 2 &&
			replayEncoder->minimumWidth() >= 280 &&
			replayEncoder->view()->textElideMode() == Qt::ElideNone &&
			replayEncoder->view()->minimumWidth() >=
				QFontMetrics(replayEncoder->view()->font())
					.horizontalAdvance(
						replayEncoder->itemText(0)) &&
			replayEnabled != nullptr && replayEnabled->isChecked() &&
			replayApply != nullptr && replayStatus != nullptr,
		"Settings must expose the active OBS Replay Buffer profile and encoders");
	replayEncoder->setCurrentIndex(
		replayEncoder->findData(QStringLiteral("nvenc")));
	replayApply->click();
	application.processEvents();
	clipcoach::test::expect(
		replayProfileApplied &&
			replayStatus->text() ==
				translator("Vertical.Replay.Restart"),
		"applying a Replay Buffer encoder from Settings must use the OBS bridge");
	auto *restartDialog = tab.findChild<QMessageBox *>(
		QStringLiteral("settingsReplayRestartDialog"));
	auto *restartButton = tab.findChild<QPushButton *>(
		QStringLiteral("settingsReplayRestartNowButton"));
	clipcoach::test::expect(
		restartDialog != nullptr && restartButton != nullptr,
		"Replay Buffer changes must show a native restart confirmation in Settings");
	restartButton->click();
	application.processEvents();
	clipcoach::test::expect(
		restartRequested &&
			replayStatus->text() ==
				translator("Vertical.Replay.RestartScheduled"),
		"restart confirmation in Settings must call the OBS restart boundary");
	auto *nativeManagedHotkey = tab.findChild<QKeySequenceEdit *>(QStringLiteral("markMomentHotkeyEdit"));
	clipcoach::test::expect(nativeManagedHotkey != nullptr && nativeManagedHotkey->isEnabled(),
				"Settings must provide an editable native OBS shortcut");
	auto *replayEncoderHelp = tab.findChild<QToolButton *>(
		QStringLiteral("settingsReplayEncoderComboInfoButton"));
	auto *replaySectionHelp = tab.findChild<QToolButton *>(
		QStringLiteral("settingsReplayPerformanceSectionInfoButton"));
	const auto pathRows =
		tab.findChildren<QWidget *>(QStringLiteral("SettingsPathRow"));
	clipcoach::test::expect(
		replayEncoderHelp != nullptr && !replayEncoderHelp->toolTip().isEmpty() &&
			!replayEncoderHelp->toolTip().startsWith(QStringLiteral("Settings.")) &&
			replaySectionHelp != nullptr &&
			!replaySectionHelp->toolTip().isEmpty() &&
			pathRows.size() == 3,
		"Settings sections and fields must expose compact contextual help");
	for (auto *pathRow : pathRows) {
		auto *box = qobject_cast<QBoxLayout *>(pathRow->layout());
		clipcoach::test::expect(
			box != nullptr &&
				box->direction() == QBoxLayout::TopToBottom &&
				pathRow->property("settingsRole").toString() ==
					QStringLiteral("pathRow"),
			"storage paths must keep copy above the path and actions");
	}
	auto *clipPathContainer =
		tab.findChild<QWidget *>(QStringLiteral("clipDirectoryEditContainer"));
	auto *clipPathEdit =
		tab.findChild<QLineEdit *>(QStringLiteral("clipDirectoryEdit"));
	auto *clipPathChange =
		tab.findChild<QPushButton *>(QStringLiteral("clipDirectoryEditChange"));
	auto *clipPathOpen =
		tab.findChild<QPushButton *>(QStringLiteral("clipDirectoryEditOpen"));
	clipcoach::test::expect(
		clipPathContainer != nullptr && clipPathEdit != nullptr &&
			clipPathChange != nullptr && clipPathOpen != nullptr &&
			clipPathContainer->layout() != nullptr,
		"each storage card must keep its path, change and open actions together");
	auto *aiEnabled = tab.findChild<QCheckBox *>(QStringLiteral("aiAssistantEnabledCheck"));
	auto *aiConsent = tab.findChild<QCheckBox *>(QStringLiteral("aiPrivacyConsentCheck"));
	auto *aiLanguage = tab.findChild<QComboBox *>(QStringLiteral("aiLanguageCombo"));
	auto *aiNotice = tab.findChild<QLabel *>(QStringLiteral("aiPrivacyNotice"));
	auto *reportBug = tab.findChild<QPushButton *>(QStringLiteral("reportBugButton"));
	auto *sendSuggestion = tab.findChild<QPushButton *>(QStringLiteral("sendSuggestionButton"));
	clipcoach::test::expect(aiEnabled != nullptr && aiConsent != nullptr && aiLanguage != nullptr &&
					aiNotice != nullptr && !aiEnabled->isEnabled() && reportBug != nullptr &&
					sendSuggestion != nullptr && reportBug->text() == QStringLiteral("REPORTAR UN ERROR"),
				"Free must see AI controls and privacy notice in a locked state");
	clipcoach::test::expect(
		tab.findChild<QWidget *>(QStringLiteral("settingsAboutUpdatesSection")) == nullptr,
		"version and updates must live in the fixed global footer, not inside Settings scroll");
	auto *widthHint = tab.findChild<QLabel *>(QStringLiteral("settingsWidthHint"));
	const auto formRows = tab.findChildren<QWidget *>(QStringLiteral("SettingsFormRow"));
	clipcoach::test::expect(widthHint != nullptr && !widthHint->isHidden() && !formRows.empty(),
			       "narrow Settings tab must recommend a larger workspace");
	for (auto *row : formRows) {
		auto *box = qobject_cast<QBoxLayout *>(row->layout());
		clipcoach::test::expect(box != nullptr && box->direction() == QBoxLayout::TopToBottom,
				       "settings fields must stack instead of being clipped in a narrow dock");
	}
	tab.resize(900, 820);
	application.processEvents();
	clipcoach::test::expect(widthHint->isHidden(),
			       "workspace hint must disappear when Settings has enough width");
	for (auto *row : formRows) {
		auto *box = qobject_cast<QBoxLayout *>(row->layout());
		clipcoach::test::expect(box != nullptr && box->direction() == QBoxLayout::LeftToRight,
				       "settings fields must return to compact rows in a wide dock");
	}
	clipcoach::test::expect(
		sectionsGrid->itemAtPosition(0, 0) != nullptr &&
			sectionsGrid->itemAtPosition(0, 1) != nullptr,
		"wide Settings must use multiple columns instead of an endless vertical list");
	clipcoach::test::expect(
		clipcoach::network::QtUpdateChecker::isVersionNewer(QStringLiteral("1.2.0"),
								 QStringLiteral("1.1.9")) &&
			!clipcoach::network::QtUpdateChecker::isVersionNewer(QStringLiteral("1.0.0"),
								  QStringLiteral("1.0.0")),
		"update comparison must only notify for a newer semantic version");

	dockToggle->setChecked(false);
	pluginLanguage->setCurrentIndex(2);
	verticalResolution->setCurrentIndex(verticalResolution->findData(
		static_cast<int>(clipcoach::VerticalResolution::Portrait4320)));
	nativeManagedHotkey->setKeySequence(
		QKeySequence(QStringLiteral("Ctrl+Alt+K")));
	QMetaObject::invokeMethod(nativeManagedHotkey, "editingFinished",
				  Qt::DirectConnection);
	application.processEvents();
	clipcoach::test::expect(!manager.settings().openDockAtStartup &&
					manager.settings().defaultDurationSeconds == 30 &&
					manager.settings().language == "es-ES" &&
					manager.settings().verticalResolution ==
						clipcoach::VerticalResolution::Portrait4320 &&
					manager.settings().verticalWidth == 4320 &&
					manager.settings().verticalHeight == 7680,
				"settings and explicit Spanish UI language must apply and persist immediately");
	clipcoach::test::expect(
		manager.settings().markMomentHotkey == "Ctrl+Alt+K",
		"edited shortcuts must be saved for the native OBS hotkey adapter");

	clipcoach::SettingsManager reloaded(directory / "settings.json");
	clipcoach::test::expect(reloaded.load(&error) && !reloaded.settings().openDockAtStartup &&
					reloaded.settings().defaultDurationSeconds == 30 &&
					reloaded.settings().language == "es-ES" &&
					reloaded.settings().verticalResolution ==
						clipcoach::VerticalResolution::Portrait4320 &&
					reloaded.settings().verticalWidth == 4320 &&
					reloaded.settings().verticalHeight == 7680,
				"settings changed in the UI must survive reload");
	clipcoach::test::expect(
		reloaded.settings().markMomentHotkey == "Ctrl+Alt+K",
		"native shortcut changes must survive reload");

	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);
	return clipcoach::test::pass("settings-tab-smoke-test");
}
