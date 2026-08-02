#include "../unit/test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>
#include <clipcoach/core/voice-trigger-controller.hpp>
#include <clipcoach/ui/main-dock.hpp>
#include <clipcoach/ui/vertical-tab.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QWizard>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QWidget>

#include <chrono>
#include <filesystem>

namespace {

class SetupVoiceController final : public clipcoach::VoiceTriggerController {
public:
	void applySettings(const clipcoach::Settings &settings) override
	{
		applied = settings;
		status_.sourceName = settings.voiceAudioSourceName;
	}
	void setProUnlocked(bool) override {}
	clipcoach::VoiceRuntimeStatus status() const override { return status_; }
	void setStatusCallback(StatusCallback callback) override { callback_ = std::move(callback); }
	std::vector<std::string> availableAudioSources() const override { return {"Shure Mic", "USB Microphone"}; }

	clipcoach::Settings applied;
	clipcoach::VoiceRuntimeStatus status_;
	StatusCallback callback_;
};

} // namespace

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	const auto directory = std::filesystem::temp_directory_path() /
			       ("clipcoach-vertical-ui-" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	clipcoach::SettingsManager settings(directory / "settings.json");
	std::string error;
	clipcoach::test::expect(settings.load(&error), "UI settings fixture must load");
	clipcoach::VerticalCanvasManager vertical(settings, true);
	clipcoach::test::expect(vertical.setObsSelection("ClipXtudio Vertical", {}, &error) &&
					vertical.setFraming(160, -67, -100, &error),
				"managed vertical scene fixture must be persisted");
	bool livePreviewUpdated = false;
	clipcoach::ui::VerticalObsBridge::FramingChanged previewFramingChanged;
	clipcoach::ui::VerticalObsBridge bridge;
	bridge.scenes = [] {
		return std::vector<std::string>{"ClipXtudio Vertical", "Gameplay", "Podcast"};
	};
	bridge.sourcesForScene = [](const std::string &scene) {
		return scene == "Gameplay" ? std::vector<std::string>{"Camera", "Game Capture"}
					   : std::vector<std::string>{"Camera"};
	};
	bridge.createPreview = [](QWidget *parent) {
		return new QWidget(parent);
	};
	bridge.updatePreview = [&livePreviewUpdated](QWidget *, const std::string &source,
						     const clipcoach::VerticalCanvasSettings &) {
		livePreviewUpdated = source == "Camera";
	};
	bridge.bindPreviewInteraction =
		[&previewFramingChanged](QWidget *, clipcoach::ui::VerticalObsBridge::FramingChanged callback) {
			previewFramingChanged = std::move(callback);
		};
	bridge.createVerticalScene = [](const std::string &, const std::string &,
					const clipcoach::VerticalCanvasSettings &, std::string *) {
		return true;
	};
	clipcoach::ui::VerticalTab liveTab([](const char *key) { return QString::fromUtf8(key); }, &vertical, bridge);
	bool startReplayRequested = false;
	bool saveClipRequested = false;
	liveTab.setCaptureActions([&startReplayRequested] { startReplayRequested = true; },
				  [&saveClipRequested] { saveClipRequested = true; });
	liveTab.resize(460, 820);
	liveTab.show();
	application.processEvents();
	if (qEnvironmentVariableIsSet("CLIPX_RENDER_ONLY")) {
		QDir().mkpath(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR));
		clipcoach::test::expect(
			liveTab.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/vertical-layout-narrow.png")),
			"Compact Vertical layout preview must be saved");
		liveTab.resize(1400, 900);
		application.processEvents();
		clipcoach::test::expect(
			liveTab.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/vertical-layout-wide.png")),
			"Vertical layout preview must be saved");
		return clipcoach::test::pass("vertical-layout-render");
	}
	auto *pageHeader = liveTab.findChild<QWidget *>(QStringLiteral("verticalPageHeader"));
	auto *previewCard = liveTab.findChild<QWidget *>(QStringLiteral("verticalPreviewCard"));
	auto *compositionHeading = liveTab.findChild<QWidget *>(QStringLiteral("verticalCompositionHeading"));
	auto *canvasHeading = liveTab.findChild<QWidget *>(QStringLiteral("verticalCanvasHeading"));
	auto *positionHeading = liveTab.findChild<QWidget *>(QStringLiteral("verticalPositionHeading"));
	auto *compositionCard = liveTab.findChild<QWidget *>(QStringLiteral("verticalCompositionCard"));
	auto *framingControls = liveTab.findChild<QWidget *>(QStringLiteral("verticalFramingControls"));
	auto *startReplay = liveTab.findChild<QPushButton *>(QStringLiteral("verticalStartReplayButton"));
	auto *saveClip = liveTab.findChild<QPushButton *>(QStringLiteral("verticalSaveClipButton"));
	auto *createScene = liveTab.findChild<QPushButton *>(QStringLiteral("verticalCreateSceneButton"));
	auto *activeBadge = liveTab.findChild<QLabel *>(QStringLiteral("verticalActiveBadge"));
	auto *autoSaved = liveTab.findChild<QLabel *>(QStringLiteral("verticalAutoSavedLabel"));
	auto *savedNow = liveTab.findChild<QLabel *>(QStringLiteral("verticalSavedNowLabel"));
	auto *compactBar = liveTab.findChild<QWidget *>(QStringLiteral("verticalCompactActionBar"));
	auto *compactReplay = liveTab.findChild<QPushButton *>(QStringLiteral("verticalCompactReplayButton"));
	auto *compactSave = liveTab.findChild<QPushButton *>(QStringLiteral("verticalCompactSaveButton"));
	auto *compactCreate = liveTab.findChild<QPushButton *>(QStringLiteral("verticalCompactCreateSceneButton"));
	auto *previewOnly = liveTab.findChild<QPushButton *>(QStringLiteral("verticalPreviewOnlyButton"));
	clipcoach::test::expect(
		pageHeader != nullptr && previewCard != nullptr && compositionCard != nullptr &&
			framingControls != nullptr && compositionHeading != nullptr && canvasHeading != nullptr &&
			positionHeading != nullptr,
		"Vertical redesign must expose a clear header, preview card and purpose-based control groups");
	clipcoach::test::expect(startReplay != nullptr && startReplay->isEnabled() && saveClip != nullptr &&
					!saveClip->isEnabled() && createScene != nullptr && activeBadge != nullptr &&
					autoSaved != nullptr && savedNow != nullptr,
				"Vertical primary actions must reflect the inactive Replay Buffer state");
	clipcoach::test::expect(pageHeader->isHidden() && compactBar != nullptr && compactBar->isVisible() &&
					compactReplay != nullptr && compactSave != nullptr &&
					compactCreate != nullptr && previewOnly != nullptr &&
					previewOnly->isVisible() && compactReplay->text().isEmpty() &&
					compactSave->text().isEmpty() && compactCreate->text().isEmpty() &&
					compactReplay->toolTip() == QStringLiteral("Vertical.Action.StartReplay") &&
					compactSave->toolTip() == QStringLiteral("Vertical.Action.SaveClip"),
				"narrow Vertical must replace the verbose header with accessible icon-only actions");
	clipcoach::test::expect(startReplay->minimumWidth() == 194 && startReplay->maximumWidth() == 194 &&
					saveClip->minimumWidth() == 142 && saveClip->maximumWidth() == 142 &&
					activeBadge->minimumWidth() == 132 && activeBadge->maximumWidth() == 132 &&
					savedNow->text() == QStringLiteral("Vertical.State.Now"),
				"Vertical header must preserve the compact reference proportions and saved-state row");
	liveTab.resize(1400, 900);
	application.processEvents();
	clipcoach::test::expect(pageHeader->isVisible() && compactBar->isHidden() && previewOnly->isVisible() &&
					startReplay->height() == 44 && saveClip->height() == 44 &&
					createScene->height() == 44 && activeBadge->height() == 44 &&
					startReplay->geometry().top() == saveClip->geometry().top() &&
					saveClip->geometry().top() == createScene->geometry().top() &&
					createScene->geometry().top() == activeBadge->geometry().top(),
				"Vertical header actions and state badge must share one height and baseline");
	liveTab.resize(460, 820);
	application.processEvents();
	compactReplay->click();
	liveTab.setReplayState(true);
	compactSave->click();
	clipcoach::test::expect(
		startReplayRequested && saveClipRequested && activeBadge->property("verticalActive").toBool(),
		"compact Vertical actions must reuse capture callbacks and show the live Replay Buffer state");
	clipcoach::test::expect(
		liveTab.findChildren<QPushButton *>(QStringLiteral("verticalCreateSceneButton")).size() == 1,
		"Vertical redesign must keep one bound create-scene action");
	auto *sceneSelector = liveTab.findChild<QComboBox *>(QStringLiteral("verticalObsSceneCombo"));
	auto *sourceSelector = liveTab.findChild<QComboBox *>(QStringLiteral("verticalObsSourceCombo"));
	auto *zoom = liveTab.findChild<QSpinBox *>(QStringLiteral("verticalZoom"));
	clipcoach::test::expect(sceneSelector != nullptr && sourceSelector != nullptr && sceneSelector->count() == 4,
				"Vertical tab must list live OBS scenes through the adapter boundary");
	clipcoach::test::expect(sceneSelector->findData(QStringLiteral("ClipXtudio Vertical")) >= 0 &&
					sceneSelector->currentData() == QStringLiteral("ClipXtudio Vertical"),
				"the managed ClipXtudio Vertical scene must remain available and selected");
	clipcoach::test::expect(
		vertical.selectedSceneName() == "ClipXtudio Vertical" && vertical.settings().zoomPercent == 160 &&
			vertical.settings().panXPercent == -67 && vertical.settings().panYPercent == -100,
		std::string("managed preview selection and framing must be preserved: scene=") +
			vertical.selectedSceneName() + " zoom=" + std::to_string(vertical.settings().zoomPercent) +
			" x=" + std::to_string(vertical.settings().panXPercent) +
			" y=" + std::to_string(vertical.settings().panYPercent));
	sceneSelector->setCurrentIndex(sceneSelector->findData(QStringLiteral("Gameplay")));
	sourceSelector->setCurrentIndex(sourceSelector->findData(QStringLiteral("Camera")));
	zoom->setValue(135);
	application.processEvents();
	clipcoach::test::expect(livePreviewUpdated && vertical.selectedSceneName() == "Gameplay" &&
					vertical.selectedSourceName() == "Camera" &&
					vertical.settings().zoomPercent == 135,
				"Vertical scene/source selection must persist and update the live preview");
	std::vector<std::string> delayedScenes;
	clipcoach::ui::VerticalObsBridge delayedBridge = bridge;
	delayedBridge.scenes = [&delayedScenes] {
		return delayedScenes;
	};
	delayedBridge.bindPreviewInteraction = {};
	clipcoach::SettingsManager restartedSettings(directory / "settings.json");
	clipcoach::test::expect(restartedSettings.load(&error),
				"persisted vertical selection must reload after an OBS restart");
	clipcoach::VerticalCanvasManager restartedVertical(restartedSettings, true);
	clipcoach::ui::VerticalTab restartedTab([](const char *key) { return QString::fromUtf8(key); },
						&restartedVertical, delayedBridge);
	auto *restartedSceneSelector = restartedTab.findChild<QComboBox *>(QStringLiteral("verticalObsSceneCombo"));
	clipcoach::test::expect(restartedVertical.selectedSceneName() == "Gameplay" &&
					restartedSceneSelector != nullptr &&
					restartedSceneSelector->currentData().toString().isEmpty(),
				"an empty OBS scene list during startup must not erase the persisted selection");
	delayedScenes = {"ClipXtudio Vertical", "Gameplay", "Podcast"};
	restartedTab.refreshObsSceneOptions();
	clipcoach::test::expect(
		restartedSceneSelector->currentData() == QStringLiteral("Gameplay") &&
			restartedVertical.selectedSceneName() == "Gameplay",
		"the last selected scene must be restored when OBS finishes loading its scene collection");
	clipcoach::test::expect(static_cast<bool>(previewFramingChanged),
				"Vertical preview must expose direct drag and wheel framing interaction");
	clipcoach::test::expect(
		liveTab.findChild<QWidget *>(QStringLiteral("verticalReplayProfileSection")) == nullptr &&
			liveTab.findChild<QComboBox *>(QStringLiteral("verticalReplayEncoderCombo")) == nullptr,
		"Vertical must stay focused on 9:16 composition; Replay Buffer performance belongs in Settings");
	auto *responsivePreview = liveTab.findChild<QWidget *>(QStringLiteral("verticalCanvasPreview"));
	auto *responsiveControls = liveTab.findChild<QWidget *>(QStringLiteral("verticalCanvasSettingsSection"));
	auto *responsivePreviewColumn = liveTab.findChild<QWidget *>(QStringLiteral("verticalPreviewColumn"));
	auto *widthHint = liveTab.findChild<QLabel *>(QStringLiteral("verticalWidthHint"));
	const auto geometryInTab = [&liveTab](QWidget *widget) {
		return QRect(widget->mapTo(&liveTab, QPoint(0, 0)), widget->size());
	};
	clipcoach::test::expect(
		responsivePreview != nullptr && responsiveControls != nullptr && widthHint != nullptr &&
			widthHint->isHidden() && compositionCard->isVisible() && responsiveControls->isVisible() &&
			compactBar->isVisible() &&
			geometryInTab(previewCard).bottom() <= geometryInTab(compactBar).top() &&
			compositionCard->width() <= liveTab.width() && responsiveControls->width() <= liveTab.width(),
		"narrow Vertical tab must show the 9:16 preview before its compact actions and controls");
	previewOnly->click();
	application.processEvents();
	clipcoach::test::expect(responsivePreview->isVisible() && previewOnly->isVisible() &&
					responsivePreview->width() > 315 && compactBar->isHidden() &&
					compositionCard->isHidden() && responsiveControls->isHidden(),
				"preview-only mode must keep the camera visible while hiding canvas controls");
	previewOnly->click();
	application.processEvents();
	clipcoach::test::expect(compactBar->isVisible() && compositionCard->isVisible() &&
					responsiveControls->isVisible(),
				"preview-only mode must restore all compact Vertical controls");
	liveTab.resize(1400, 900);
	application.processEvents();
	clipcoach::test::expect(
		widthHint->isHidden() && responsivePreview->width() >= 270 && responsivePreview->height() >= 480 &&
			responsivePreviewColumn != nullptr &&
			compositionCard->parentWidget() == responsivePreviewColumn &&
			geometryInTab(responsiveControls).left() >= geometryInTab(responsivePreviewColumn).right(),
		"wide Vertical tab must keep composition beneath the preview and restore side-by-side controls");
	auto *canvasCard = liveTab.findChild<QWidget *>(QStringLiteral("verticalCanvasCard"));
	auto *positionCard = liveTab.findChild<QWidget *>(QStringLiteral("verticalPositionCard"));
	clipcoach::test::expect(canvasCard != nullptr && positionCard != nullptr &&
					geometryInTab(canvasCard).top() == geometryInTab(positionCard).top() &&
					geometryInTab(canvasCard).right() <= geometryInTab(positionCard).left(),
				"wide Vertical must place Canvas and Position together on one row");
	previewFramingChanged(160, -25, 35);
	application.processEvents();
	clipcoach::test::expect(vertical.settings().zoomPercent == 160 && vertical.settings().panXPercent == -25 &&
					vertical.settings().panYPercent == 35 && zoom->value() == 160,
				"Direct preview interaction must persist framing and synchronize controls");

	clipcoach::ui::MainDock dock([](const char *key) { return QString::fromUtf8(key); }, nullptr, &settings,
				     nullptr, nullptr, {}, &vertical);

	auto *preview = dock.findChild<QWidget *>(QStringLiteral("verticalCanvasPreview"));
	auto *output = dock.findChild<QComboBox *>(QStringLiteral("verticalOutputModeCombo"));
	auto *resolution = dock.findChild<QComboBox *>(QStringLiteral("verticalCanvasResolutionCombo"));
	auto *scroll = dock.findChild<QScrollArea *>(QStringLiteral("verticalCanvasScroll"));
	auto *workflow = dock.findChild<QWidget *>(QStringLiteral("verticalWorkflowHelp"));
	auto *settingsDescription = dock.findChild<QWidget *>(QStringLiteral("verticalSettingsDescription"));

	clipcoach::test::expect(preview != nullptr && preview->minimumHeight() > preview->minimumWidth(),
				"Vertical tab must expose a portrait 9:16 preview");
	clipcoach::test::expect(preview->minimumWidth() * 16 == preview->minimumHeight() * 9,
				"live preview widget itself must keep an exact 9:16 shape");
	clipcoach::test::expect(output != nullptr && output->count() == 3,
				"Vertical tab must expose three output modes");
	clipcoach::test::expect(dock.findChild<QComboBox *>(QStringLiteral("verticalTemplateCombo")) == nullptr &&
					dock.findChild<QWidget *>(QStringLiteral("verticalVisibleLayersSection")) ==
						nullptr,
				"Vertical must not expose the removed template and synthetic-layer controls");
	clipcoach::test::expect(resolution != nullptr && resolution->count() == 6,
				"Vertical tab must expose 720p, 1080p, 2K, 4K, 8K and Custom");
	clipcoach::test::expect(
		resolution != nullptr &&
			resolution->findData(static_cast<int>(clipcoach::VerticalResolution::Portrait1440)) >= 0 &&
			resolution->findData(static_cast<int>(clipcoach::VerticalResolution::Portrait2160)) >= 0 &&
			resolution->findData(static_cast<int>(clipcoach::VerticalResolution::Portrait4320)) >= 0,
		"Vertical tab must map every high-resolution preset to its persisted enum value");
	clipcoach::test::expect(scroll != nullptr && scroll->verticalScrollBarPolicy() == Qt::ScrollBarAsNeeded &&
					workflow != nullptr && settingsDescription != nullptr,
				"Vertical tab must provide a scrollable, localized workflow explanation");
	output->setCurrentIndex(static_cast<int>(clipcoach::CaptureOutputMode::Vertical));
	resolution->setCurrentIndex(resolution->findData(static_cast<int>(clipcoach::VerticalResolution::Portrait720)));
	application.processEvents();
	auto *verticalStatus = dock.findChild<QWidget *>(QStringLiteral("verticalCanvasStatusCard"));
	auto *verticalStatusValue = verticalStatus != nullptr
					    ? verticalStatus->findChild<QLabel *>(QStringLiteral("StatusValue"))
					    : nullptr;
	clipcoach::test::expect(vertical.settings().outputMode == clipcoach::CaptureOutputMode::Vertical &&
					vertical.settings().resolution == clipcoach::VerticalResolution::Portrait720 &&
					verticalStatusValue != nullptr &&
					verticalStatusValue->text() == QStringLiteral("720×1280"),
				"Vertical tab changes must pass through core and immediately update the header canvas");

	clipcoach::test::expect(vertical.setObsSelection("ClipXtudio Vertical", {}, &error),
				"initial setup fixture must select the managed vertical scene");
	SetupVoiceController setupVoice;
	clipcoach::ui::MainDock setupDock([](const char *key) { return QString::fromUtf8(key); }, nullptr, &settings,
					  nullptr, nullptr, {}, &vertical, nullptr, nullptr, nullptr, nullptr, nullptr,
					  &setupVoice, {}, bridge);
	setupDock.showInitialSetupIfNeeded();
	auto *setupDialog = setupDock.findChild<QWizard *>(QStringLiteral("clipXtudioInitialSetup"));
	auto *setupScene = setupDialog != nullptr
				   ? setupDialog->findChild<QComboBox *>(QStringLiteral("initialSetupVerticalScene"))
				   : nullptr;
	auto *setupMicrophone = setupDialog != nullptr
					? setupDialog->findChild<QComboBox *>(QStringLiteral("initialSetupMicrophone"))
					: nullptr;
	auto *setupVoiceEnabled =
		setupDialog != nullptr ? setupDialog->findChild<QCheckBox *>(QStringLiteral("initialSetupVoiceEnabled"))
				       : nullptr;
	auto *setupOutputMode = setupDialog != nullptr
					? setupDialog->findChild<QComboBox *>(QStringLiteral("initialSetupOutputMode"))
					: nullptr;
	clipcoach::test::expect(setupDialog != nullptr && setupScene != nullptr && setupMicrophone != nullptr &&
					setupVoiceEnabled != nullptr && setupOutputMode != nullptr &&
					setupDialog->pageIds().size() == 5 &&
					setupOutputMode->currentData().toInt() ==
						static_cast<int>(clipcoach::CaptureOutputMode::Vertical) &&
					setupScene->currentData() == QStringLiteral("ClipXtudio Vertical") &&
					setupMicrophone->findData(QStringLiteral("Shure Mic")) >= 0,
				"first OBS launch must select Vertical and provide the five-step Free setup");
	setupMicrophone->setCurrentIndex(setupMicrophone->findData(QStringLiteral("Shure Mic")));
	setupVoiceEnabled->setChecked(true);
	while (setupDialog->currentId() != setupDialog->pageIds().back())
		setupDialog->next();
	setupDialog->button(QWizard::FinishButton)->click();
	application.processEvents();
	clipcoach::test::expect(
		settings.settings().initialSetupCompleted &&
			!settings.settings().initialSetupCompletedVersion.empty() &&
			settings.settings().triggerVoiceEnabled &&
			settings.settings().triggerAction == clipcoach::TriggerAction::SaveClip &&
			settings.settings().voiceAudioSourceName == "Shure Mic" &&
			setupVoice.applied.voiceAudioSourceName == "Shure Mic" &&
			vertical.selectedSceneName() == "ClipXtudio Vertical",
		"initial setup must persist and immediately apply the selected microphone and vertical scene");
	setupDock.showInitialSetupIfNeeded();
	clipcoach::test::expect(setupDock.findChild<QDialog *>(QStringLiteral("clipXtudioInitialSetup")) == nullptr,
				"completed initial setup must not appear again on later OBS launches");
	auto previousVersion = settings.settings();
	previousVersion.initialSetupCompletedVersion = "0.0.0";
	clipcoach::test::expect(settings.save(previousVersion, &error),
				"setup fixture must persist a previous completed version");
	setupDock.showInitialSetupIfNeeded();
	auto *updatedSetup = setupDock.findChild<QWizard *>(QStringLiteral("clipXtudioInitialSetup"));
	clipcoach::test::expect(updatedSetup != nullptr,
				"a newly installed plugin version must show its updated setup once");
	if (updatedSetup != nullptr)
		updatedSetup->reject();

	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);
	return clipcoach::test::pass("vertical-tab-test");
}
