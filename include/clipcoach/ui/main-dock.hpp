#pragma once

#include <QWidget>
#include <QUrl>
#include <clipcoach/ui/caption-generator.hpp>
#include <clipcoach/ui/vertical-obs-bridge.hpp>
#include <clipcoach/remote/remote-clipper-types.hpp>

#include <functional>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QComboBox;
class QMessageBox;
class QNetworkAccessManager;
class QFrame;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QTimer;
class QVBoxLayout;

namespace clipcoach {
class ClipManager;
class ClipActionService;
class ExportManager;
enum class ExportOrientation;
class FeatureGateService;
class SettingsManager;
struct Settings;
class TriggerEngine;
class VerticalCanvasManager;
class VoiceTriggerController;
enum class CaptureError;
enum class ReplayState;
enum class TriggerType;
struct CaptureResult;
struct ClipMetadata;
namespace integrations {
class ChatIntegrationManager;
}
namespace licensing {
class LicenseManager;
}
namespace network {
class QtUpdateChecker;
class QtUpdateDownloader;
}
namespace storage {
class ClipLibraryService;
template<typename T> struct RepositoryResult;
} // namespace storage
} // namespace clipcoach

namespace clipcoach::ui {

class ClipsTab;
class ProBadge;
class SettingsTab;
class StatusCard;
class TabBar;
class TriggersTab;
class VerticalTab;

class MainDock final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using SceneProvider = std::function<std::vector<std::string>()>;
	using LanguageChangedCallback = std::function<void()>;
	using HotkeySettingsChangedCallback =
		std::function<void(const Settings &)>;
	using ReplayDurationChangedCallback = std::function<void(int)>;
	using RemoteCommandsChangedCallback = std::function<void(bool)>;

	explicit MainDock(QWidget *parent = nullptr);
	explicit MainDock(TranslationFunction translator, QWidget *parent = nullptr);
	MainDock(TranslationFunction translator, ClipManager *clipManager, SettingsManager *settingsManager,
		 storage::ClipLibraryService *libraryService = nullptr, QWidget *parent = nullptr,
		 std::string sessionId = {});
	MainDock(TranslationFunction translator, ClipManager *clipManager, SettingsManager *settingsManager,
		 storage::ClipLibraryService *libraryService, QWidget *parent, std::string sessionId,
		 VerticalCanvasManager *verticalManager, ExportManager *exportManager = nullptr,
		 TriggerEngine *triggerEngine = nullptr, integrations::ChatIntegrationManager *chatManager = nullptr,
		 licensing::LicenseManager *licenseManager = nullptr, FeatureGateService *featureGates = nullptr,
		 VoiceTriggerController *voiceController = nullptr,
		 SceneProvider sceneProvider = {}, VerticalObsBridge verticalObsBridge = {},
		 CaptionGenerator captionGenerator = {});
	~MainDock() override;
	void setLanguageChangedCallback(LanguageChangedCallback callback);
	void setHotkeySettingsChangedCallback(
		HotkeySettingsChangedCallback callback);
	void setReplayDurationChangedCallback(
		ReplayDurationChangedCallback callback);
	void setRemoteCommandsChangedCallback(RemoteCommandsChangedCallback callback);
	void setRemoteClipperStatus(const remote::RemoteClipperStatus &status);
	void refreshClipLibrary();
	void refreshVerticalObsScenes();
	void refreshCaptureOutputMode();
	void showInitialSetupIfNeeded(bool force = false);
	void showCapturePending();

	MainDock(const MainDock &) = delete;
	MainDock &operator=(const MainDock &) = delete;

	[[nodiscard]] CaptureResult requestTriggeredCapture(
		int durationSeconds, TriggerType triggerType,
		std::string triggerLabel, int score, ExportOrientation output);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	[[nodiscard]] QString text(const char *key) const;
	[[nodiscard]] QWidget *createCapturePage();
	[[nodiscard]] QWidget *createVerticalPage();
	[[nodiscard]] QWidget *createTriggersPage();
	[[nodiscard]] QWidget *createClipsPage();
	[[nodiscard]] QWidget *createSettingsPage();
	[[nodiscard]] QWidget *createGlobalFooter();
	void bindCaptureFlow();
	void toggleReplayBuffer();
	void captureSelectedClip();
	void captureVerticalClip();
	void checkForUpdates(bool userInitiated = true);
	void beginUpdateDownload(bool requireConfirmation = true);
	void showUpToDateDialog();
	void showUpdateAvailableDialog();
	void showUpdateErrorDialog(const QString &details = {});
	void setUpdateIndicatorVisible(bool visible);
	void setUpdateChecking(bool checking);
	void setFooterExpandedText(QPushButton *button,
				   const QString &expandedText);
	void updateFooterResponsivePresentation();
	void setVerticalPreviewOnly(bool enabled);
	void showFooterStatus(const QString &message, bool error = false,
			      bool autoReset = false);
	void updateReplayState(ReplayState state);
	void handleClipSaved(const ClipMetadata &clip);
	void handleCaptureError(CaptureError error);
	void selectDuration(int seconds);
	void showCaptureNotification(const QString &message, bool error,
				     bool autoDismiss = false);
	void updateFeatureGateUi();
	void loadPublicPluginConfiguration(int endpointIndex = 0);
	void setCaptionProcessing(bool busy,
				 const CaptionGenerationProgress &progress = {});
	void updateCaptionEta();
	void updateCaptionActivityMessage();
	void setCapturePendingIndicator(bool pending);
	void beginRemoteAuthentication();
	void toggleRemoteClipper();
	bool persistRemoteCommandsEnabled(bool enabled);
	void finishRemoteToggle(bool success, const QString &title,
				const QString &message, bool rollback = false);
	void openRemoteClipperAccount();
	void finishRemoteAuthentication(bool success, const QString &title,
					const QString &message, bool openAccount = false);

	TranslationFunction translator_;
	LanguageChangedCallback languageChangedCallback_;
	HotkeySettingsChangedCallback hotkeySettingsChangedCallback_;
	ReplayDurationChangedCallback replayDurationChangedCallback_;
	RemoteCommandsChangedCallback remoteCommandsChangedCallback_;
	ClipManager *clipManager_{nullptr};
	SettingsManager *settingsManager_{nullptr};
	VerticalCanvasManager *verticalManager_{nullptr};
	ExportManager *exportManager_{nullptr};
	TriggerEngine *triggerEngine_{nullptr};
	integrations::ChatIntegrationManager *chatManager_{nullptr};
	licensing::LicenseManager *licenseManager_{nullptr};
	FeatureGateService *featureGates_{nullptr};
	VoiceTriggerController *voiceController_{nullptr};
	SceneProvider sceneProvider_;
	VerticalObsBridge verticalObsBridge_;
	CaptionGenerator captionGenerator_;
	std::uint64_t licenseObserverId_{0};
	storage::ClipLibraryService *libraryService_{nullptr};
	std::string sessionId_;
	ClipsTab *clipsTab_{nullptr};
	StatusCard *replayStatusCard_{nullptr};
	StatusCard *verticalStatusCard_{nullptr};
	StatusCard *planStatusCard_{nullptr};
	QPushButton *planUpgradeButton_{nullptr};
	ProBadge *headerProBadge_{nullptr};
	QPushButton *headerDemoGuideButton_{nullptr};
	QWidget *dockHeader_{nullptr};
	QWidget *statusContainer_{nullptr};
	TabBar *mainTabBar_{nullptr};
	SettingsTab *settingsTab_{nullptr};
	TriggersTab *triggersTab_{nullptr};
	VerticalTab *verticalTab_{nullptr};
	QPushButton *startReplayButton_{nullptr};
	QPushButton *stopReplayButton_{nullptr};
	QPushButton *markMomentButton_{nullptr};
	QPushButton *saveVerticalButton_{nullptr};
	QPushButton *captureGoVerticalButton_{nullptr};
	QPushButton *captureRemoteClipperButton_{nullptr};
	QPushButton *captureShortcutMarkButton_{nullptr};
	QPushButton *captureShortcutSaveButton_{nullptr};
	QPushButton *captureShortcutVerticalButton_{nullptr};
	QPushButton *captureShortcutStopButton_{nullptr};
	QComboBox *captureOutputModeCombo_{nullptr};
	QLabel *captureOutputModeDescription_{nullptr};
	QPushButton *footerReplayButton_{nullptr};
	QPushButton *footerClipButton_{nullptr};
	QPushButton *footerUpdateButton_{nullptr};
	QWidget *globalFooter_{nullptr};
	QLabel *footerCredits_{nullptr};
	QMessageBox *updateStatusDialog_{nullptr};
	QLabel *updateAvailableIndicator_{nullptr};
	QTimer *updatePulseTimer_{nullptr};
	QTimer *updateSpinnerTimer_{nullptr};
	QLabel *headerStatus_{nullptr};
	QLabel *headerCapturePendingDot_{nullptr};
	QLabel *headerRemoteStatus_{nullptr};
	QPushButton *headerRemoteAuthenticateButton_{nullptr};
	QPushButton *headerRemoteOpenButton_{nullptr};
	QTimer *remoteStatusPulseTimer_{nullptr};
	QTimer *capturePendingPulseTimer_{nullptr};
	QUrl demoGuideUrl_;
	std::unique_ptr<QNetworkAccessManager> publicConfigNetwork_;
	QUrl updateDownloadUrl_;
	QString updateSha256_;
	QString updateVersion_;
	qint64 updateSizeBytes_{0};
	std::unique_ptr<network::QtUpdateChecker> updateChecker_;
	std::unique_ptr<network::QtUpdateDownloader> updateDownloader_;
	QLabel *captureNotification_{nullptr};
	QFrame *captionBusyOverlay_{nullptr};
	QLabel *captionBusyLabel_{nullptr};
	QLabel *captionBusyPercent_{nullptr};
	QLabel *captionBusySpinner_{nullptr};
	QLabel *captionBusyEta_{nullptr};
	QProgressBar *captionBusyProgress_{nullptr};
	QFrame *remoteAuthOverlay_{nullptr};
	QLabel *remoteAuthTitle_{nullptr};
	QLabel *remoteAuthMessage_{nullptr};
	QProgressBar *remoteAuthProgress_{nullptr};
	QTimer *remoteAuthTimeout_{nullptr};
	QTimer *captionProgressTimer_{nullptr};
	int captionEstimatedSecondsRemaining_{0};
	int captionLastPercentage_{0};
	int captionSecondsWithoutProgress_{0};
	int captionProgressTimerTicks_{0};
	int captionSpinnerAngle_{0};
	QString captionReportedStatus_;
	bool captionProcessingActive_{false};
	bool remoteAuthenticationPending_{false};
	bool remoteAuthenticationSawConnecting_{false};
	bool remoteTogglePending_{false};
	bool remoteToggleTargetEnabled_{false};
	bool remoteToggleSawConnecting_{false};
	bool remoteAuthenticated_{false};
	bool remoteOnline_{false};
	bool remoteStatusPulseBright_{true};
	QLabel *verticalLimitedLabel_{nullptr};
	QLabel *lastClipValueLabel_{nullptr};
	QLabel *lastClipMetadataLabel_{nullptr};
	QLabel *lastClipThumbnailLabel_{nullptr};
	QLabel *captureReplayStateLabel_{nullptr};
	QLabel *captureReplayStateDescription_{nullptr};
	QLabel *sessionClipCountLabel_{nullptr};
	QWidget *captureUpgradeBanner_{nullptr};
	QWidget *durationProBadge_{nullptr};
	QVBoxLayout *sessionClipLayout_{nullptr};
	std::vector<QPushButton *> durationButtons_;
	std::unique_ptr<ClipActionService> verticalCaptureActionService_;
	int selectedDurationSeconds_{30};
	std::optional<ExportOrientation> pendingCaptureExport_;
	std::uint64_t captureNotificationGeneration_{0};
	bool captureNotificationPending_{false};
	std::uint64_t headerStatusGeneration_{0};
	int updateSpinnerAngle_{0};
	bool updateCheckInProgress_{false};
	bool capturePendingPulseOn_{false};
};

} // namespace clipcoach::ui
