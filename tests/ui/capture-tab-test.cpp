#include "../unit/test-support.hpp"

#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/core/replay-manager.hpp>
#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>
#include <clipcoach/ui/main-dock.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

class MockReplayManager final : public clipcoach::ReplayManager {
public:
	[[nodiscard]] clipcoach::ReplayState state() const noexcept override { return state_; }
	clipcoach::ReplayOperationResult start() override
	{
		++startCalls;
		return {true, {}};
	}
	clipcoach::ReplayOperationResult stop() override
	{
		++stopCalls;
		return {true, {}};
	}
	clipcoach::ReplayOperationResult save() override
	{
		++saveCalls;
		return {true, {}};
	}
	void setStateChangedCallback(StateChangedCallback callback) override { stateCallback = std::move(callback); }
	void setReplaySavedCallback(ReplaySavedCallback callback) override { savedCallback = std::move(callback); }

	clipcoach::ReplayState state_{clipcoach::ReplayState::Active};
	int saveCalls{0};
	int startCalls{0};
	int stopCalls{0};
	StateChangedCallback stateCallback;
	ReplaySavedCallback savedCallback;
};

} // namespace

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	const auto directory = std::filesystem::temp_directory_path() / "clipcoach-ui-capture-test";
	std::filesystem::remove_all(directory);
	std::filesystem::create_directories(directory);
	const auto settingsPath = directory / "settings.json";
	clipcoach::SettingsManager settings(settingsPath);
	std::string error;
	clipcoach::test::expect(settings.load(&error), "capture UI test settings must load");
	auto configuredSettings = settings.settings();
	configuredSettings.defaultDurationSeconds = 45;
	configuredSettings.triggerVoiceEnabled = true;
	configuredSettings.triggerAction = clipcoach::TriggerAction::SaveClip;
	configuredSettings.autoStartReplayBuffer = false;
	clipcoach::test::expect(settings.save(configuredSettings, &error), "capture duration fixture must save");

	MockReplayManager replay;
	replay.state_ = clipcoach::ReplayState::Inactive;
	const auto fixedTime = std::chrono::system_clock::from_time_t(1785247509);
	clipcoach::ClipManager clips(replay, [fixedTime] { return fixedTime; });
	clipcoach::FeatureGateService proGates(clipcoach::EntitlementState::ProActive);
	clipcoach::VerticalCanvasManager vertical(settings, true);
	const auto translator = [](const char *key) {
		if (std::string(key) == clipcoach::ui::strings::kCaptureSessionCount) {
			return QStringLiteral("%1 clips");
		}
		if (std::string(key) == clipcoach::ui::strings::kCaptureManualMetadata) {
			return QStringLiteral("%1 s · Manual");
		}
		if (std::string(key) == clipcoach::ui::strings::kCaptureSaved) {
			return QStringLiteral("Clip saved: %1");
		}
		return QString::fromUtf8(key);
	};
	clipcoach::ui::MainDock dock(translator, &clips, &settings, nullptr, nullptr, std::string{}, &vertical, nullptr,
				     nullptr, nullptr, nullptr, &proGates, nullptr);
	application.processEvents();
	clipcoach::test::expect(replay.startCalls == 0,
				"voice save triggers must not start Replay Buffer without explicit opt-in");
	replay.state_ = clipcoach::ReplayState::Active;
	clipcoach::test::expect(static_cast<bool>(replay.stateCallback),
				"capture UI must subscribe to Replay Buffer state changes");
	replay.stateCallback(clipcoach::ReplayState::Active);
	application.processEvents();

	auto *markMoment = dock.findChild<QPushButton *>(QStringLiteral("markMomentButton"));
	auto *footerReplay = dock.findChild<QPushButton *>(QStringLiteral("footerReplayBufferButton"));
	auto *footerClip = dock.findChild<QPushButton *>(QStringLiteral("footerSaveClipButton"));
	auto *captureRemote = dock.findChild<QPushButton *>(QStringLiteral("captureRemoteClipperButton"));
	auto *count = dock.findChild<QLabel *>(QStringLiteral("sessionClipCountLabel"));
	auto *lastClip = dock.findChild<QLabel *>(QStringLiteral("lastClipValueLabel"));
	auto *notification = dock.findChild<QLabel *>(QStringLiteral("captureNotification"));
	auto *dockHeader = dock.findChild<QWidget *>(QStringLiteral("DockHeader"));
	auto *headerStatus = dock.findChild<QLabel *>(QStringLiteral("headerStatusBadge"));
	auto *headerRemote = dock.findChild<QPushButton *>(QStringLiteral("headerRemoteOpenButton"));
	auto *remoteOverlay = dock.findChild<QWidget *>(QStringLiteral("remoteAuthOverlay"));
	auto *outputMode = dock.findChild<QComboBox *>(QStringLiteral("captureOutputModeCombo"));
	auto *outputDescription = dock.findChild<QLabel *>(QStringLiteral("captureOutputModeDescription"));
	auto *pendingDot = dock.findChild<QLabel *>(QStringLiteral("headerCapturePendingDot"));
	auto *insideObs = dock.findChild<QCheckBox *>(QStringLiteral("insideObsNotificationCheck"));
	auto *saveVertical = dock.findChild<QPushButton *>(QStringLiteral("saveVerticalClipButton"));
	auto *verticalRequirement = dock.findChild<QLabel *>(QStringLiteral("verticalProRequirementLabel"));
	auto *goVertical = dock.findChild<QPushButton *>(QStringLiteral("captureGoVerticalButton"));
	auto *bufferCard = dock.findChild<QWidget *>(QStringLiteral("captureBufferStatusCard"));
	auto *flowCard = dock.findChild<QWidget *>(QStringLiteral("captureRecommendedFlowCard"));
	auto *lastThumbnail = dock.findChild<QLabel *>(QStringLiteral("lastClipThumbnail"));
	auto *tabBar = dock.findChild<QTabBar *>(QStringLiteral("mainTabBar"));

	clipcoach::test::expect(tabBar != nullptr && tabBar->currentIndex() == 0,
				"Vertical must be the default page when ClipXtudio opens");
	clipcoach::test::expect(
		goVertical != nullptr && bufferCard != nullptr && flowCard != nullptr && lastThumbnail != nullptr,
		"Capture dashboard must expose navigation, buffer status, last clip and recommended flow");
	clipcoach::test::expect(outputMode != nullptr && outputMode->count() == 3 && outputDescription != nullptr &&
					outputDescription->wordWrap(),
				"Capture must expose one clear default format selector with readable guidance");
	outputMode->setCurrentIndex(static_cast<int>(clipcoach::CaptureOutputMode::Both));
	application.processEvents();
	clipcoach::test::expect(vertical.settings().outputMode == clipcoach::CaptureOutputMode::Both &&
					settings.settings().outputMode == clipcoach::CaptureOutputMode::Both,
				"the Capture format selector must persist Both as the shared output mode");
	outputMode->setCurrentIndex(static_cast<int>(clipcoach::CaptureOutputMode::Horizontal));
	application.processEvents();
	clipcoach::test::expect(captureRemote != nullptr && dock.findChild<QPushButton *>(QStringLiteral(
								    "footerRemoteClipperButton")) == nullptr,
				"Remote Clipper must remain reachable from Capture without a duplicate footer action");
	for (const auto *name : {"startReplayBufferButtonInfoButton", "markMomentButtonInfoButton",
				 "saveVerticalClipButtonInfoButton", "captureGoVerticalButtonInfoButton"}) {
		auto *help = dock.findChild<QToolButton *>(QString::fromLatin1(name));
		clipcoach::test::expect(help != nullptr && !help->toolTip().isEmpty(),
					"Capture quick actions must expose compact contextual help");
	}
	goVertical->click();
	clipcoach::test::expect(tabBar->currentIndex() == 0, "Go to Vertical must preserve native tab navigation");
	tabBar->setCurrentIndex(1);

	clipcoach::test::expect(markMoment != nullptr && markMoment->isEnabled(),
				"manual capture must be enabled while replay is active");
	clipcoach::test::expect(footerReplay != nullptr && footerReplay->isEnabled() && footerClip != nullptr &&
					footerClip->isEnabled(),
				"global Replay and clip actions must stay enabled outside the Capture tab");
	clipcoach::test::expect(count != nullptr && lastClip != nullptr && notification != nullptr,
				"capture session feedback widgets must exist");
	clipcoach::test::expect(
		dockHeader != nullptr && notification->parentWidget() == dockHeader,
		"capture progress and results must live in the header instead of inside the Capture page");
	clipcoach::test::expect(dock.findChild<QSpinBox *>(QStringLiteral("defaultDurationSpin")) == nullptr &&
					insideObs != nullptr,
				"capture duration must not be duplicated under Settings");
	clipcoach::test::expect(saveVertical != nullptr && saveVertical->isEnabled() &&
					verticalRequirement != nullptr && verticalRequirement->isHidden(),
				"an active Pro entitlement must unlock vertical capture");
	footerReplay->click();
	clipcoach::test::expect(replay.stopCalls == 1, "global Replay action must delegate through ClipManager");
	footerClip->click();
	clipcoach::test::expect(replay.saveCalls == 1, "global clip action must delegate through ClipManager");
	clipcoach::test::expect(pendingDot != nullptr && !pendingDot->isHidden(),
				"an accepted capture must show the pulsing pending indicator");

	const auto replayPath = directory / "Replay.mp4";
	std::ofstream(replayPath).put('\0');
	replay.savedCallback(replayPath);
	application.processEvents();
	clipcoach::test::expect(pendingDot->isHidden(),
				"the pending indicator must stop when OBS confirms the saved clip");

	clipcoach::test::expect(count->text().contains(QStringLiteral("1")),
				"session counter must update after a saved clip");
	clipcoach::test::expect(lastClip->text().contains(QStringLiteral("ClipX_")),
				"last clip field must show the readable file name");
	clipcoach::test::expect(!notification->isHidden(), "dock notification must become visible after save");
	clipcoach::test::expect(notification->text().endsWith(QStringLiteral("✓")),
				"saved notification must show a trailing check");
	clipcoach::test::expect(notification->maximumHeight() == 30, "saved notification must use the compact height");
	clipcoach::test::expect(notification->sizePolicy().horizontalPolicy() == QSizePolicy::Maximum,
				"saved notification must not stretch across the page");
	clipcoach::test::expect(headerStatus != nullptr &&
					headerStatus->text() ==
						QString::fromUtf8(clipcoach::ui::strings::kHeaderBufferActive),
				"header status must stay focused on the concise active Buffer state");
	clipcoach::test::expect(clips.sessionClips().back().durationSeconds == 45,
				"default duration changes must apply without restarting the dock");

	QEventLoop dismissWait;
	QTimer::singleShot(4200, &dismissWait, &QEventLoop::quit);
	dismissWait.exec();
	clipcoach::test::expect(notification->isHidden(), "saved notification must auto-dismiss");
	clipcoach::test::expect(headerStatus->text() == QString::fromUtf8(clipcoach::ui::strings::kHeaderBufferActive),
				"header status must retain the active Buffer state after the save confirmation");

	replay.state_ = clipcoach::ReplayState::Inactive;
	replay.stateCallback(clipcoach::ReplayState::Inactive);
	dock.showCapturePending();
	application.processEvents();
	clipcoach::test::expect(notification->isHidden() && pendingDot->isHidden(),
				"an inactive Replay Buffer must never leave a stale saving notification in the header");
	replay.state_ = clipcoach::ReplayState::Active;
	replay.stateCallback(clipcoach::ReplayState::Active);

	insideObs->setChecked(false);
	const auto secondReplayPath = directory / "Replay2.mp4";
	std::ofstream(secondReplayPath).put('\0');
	markMoment->click();
	replay.savedCallback(secondReplayPath);
	application.processEvents();
	clipcoach::test::expect(notification->isHidden(),
				"disabled in-OBS notifications must apply to the next capture");

	bool remoteSettingChanged = false;
	bool requestedRemoteState = true;
	dock.setRemoteCommandsChangedCallback([&](bool enabled) {
		remoteSettingChanged = true;
		requestedRemoteState = enabled;
	});
	clipcoach::remote::RemoteClipperStatus onlineRemote;
	onlineRemote.connection = clipcoach::remote::RemoteConnectionState::Connected;
	dock.setRemoteClipperStatus(onlineRemote);
	headerRemote->click();
	application.processEvents();
	clipcoach::test::expect(
		remoteSettingChanged && !requestedRemoteState && !settings.settings().remoteCommandsEnabled &&
			remoteOverlay != nullptr && !remoteOverlay->isHidden() && !headerRemote->isEnabled(),
		"Remote header action must persist an offline request and block the UI while it is confirmed");
	clipcoach::remote::RemoteClipperStatus pausedRemote;
	pausedRemote.connection = clipcoach::remote::RemoteConnectionState::Paused;
	dock.setRemoteClipperStatus(pausedRemote);
	application.processEvents();
	clipcoach::test::expect(remoteOverlay->isHidden() && headerRemote->isEnabled() &&
					headerRemote->property("remoteState").toString() == QStringLiteral("offline") &&
					headerRemote->toolTip() ==
						QString::fromUtf8(clipcoach::ui::strings::kRemoteToggleTurnOn),
				"confirmed local pause must finish the modal flow and show Remote offline");

	remoteSettingChanged = false;
	headerRemote->click();
	application.processEvents();
	clipcoach::test::expect(
		remoteSettingChanged && requestedRemoteState && settings.settings().remoteCommandsEnabled &&
			!remoteOverlay->isHidden() && !headerRemote->isEnabled(),
		"Remote header action must persist an online request and keep the UI blocked while the backend validates it");
	clipcoach::remote::RemoteClipperStatus connectingRemote;
	connectingRemote.connection = clipcoach::remote::RemoteConnectionState::Connecting;
	dock.setRemoteClipperStatus(connectingRemote);
	clipcoach::remote::RemoteClipperStatus reconnectedRemote;
	reconnectedRemote.connection = clipcoach::remote::RemoteConnectionState::Connected;
	dock.setRemoteClipperStatus(reconnectedRemote);
	application.processEvents();
	clipcoach::test::expect(remoteOverlay->isHidden() && headerRemote->isEnabled() &&
					headerRemote->property("remoteState").toString() == QStringLiteral("online") &&
					headerRemote->toolTip() ==
						QString::fromUtf8(clipcoach::ui::strings::kRemoteToggleTurnOff),
				"a confirmed backend heartbeat must finish the modal flow and show Remote online");

	std::filesystem::remove_all(directory);
	return clipcoach::test::pass("capture-tab-test");
}
