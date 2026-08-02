#include "../unit/test-support.hpp"

#include <clipcoach/ui/main-dock.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>

#include <string>
#include <vector>

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	std::vector<std::string> requestedTranslationKeys;
	const auto translator = [&requestedTranslationKeys](const char *key) {
		requestedTranslationKeys.emplace_back(key);
		return QString::fromUtf8(key);
	};

	clipcoach::ui::MainDock dock(translator);
	const auto *tabBar = dock.findChild<QTabBar *>(QStringLiteral("mainTabBar"));
	const auto *stack = dock.findChild<QStackedWidget *>(QStringLiteral("mainTabStack"));
	const auto *markMoment = dock.findChild<QPushButton *>(QStringLiteral("markMomentButton"));
	const auto *startReplay = dock.findChild<QPushButton *>(QStringLiteral("startReplayBufferButton"));
	const auto *stopReplay = dock.findChild<QPushButton *>(QStringLiteral("stopReplayBufferButton"));
	const auto *fiveMinutes = dock.findChild<QPushButton *>(QStringLiteral("durationButton300"));
	const auto *verticalClip = dock.findChild<QPushButton *>(QStringLiteral("saveVerticalClipButton"));
	const auto *sessionCount = dock.findChild<QLabel *>(QStringLiteral("sessionClipCountLabel"));
	const auto *verticalPreview = dock.findChild<QWidget *>(QStringLiteral("verticalCanvasPreview"));
	const auto *durationProBadge =
		dock.findChild<clipcoach::ui::ProBadge *>(QStringLiteral("captureDurationProBadge"));
	const auto *upgradeBanner = dock.findChild<QWidget *>(QStringLiteral("captureUpgradeBanner"));
	const auto *footer = dock.findChild<QWidget *>(QStringLiteral("GlobalFooter"));
	const auto *brandMark = dock.findChild<QLabel *>(QStringLiteral("BrandMark"));
	const auto *brandLabel = dock.findChild<QLabel *>(QStringLiteral("BrandLabel"));
	const auto *headerStatus = dock.findChild<QLabel *>(QStringLiteral("headerStatusBadge"));
	const auto *headerRemoteAuthenticate =
		dock.findChild<QPushButton *>(QStringLiteral("headerRemoteAuthenticateButton"));
	const auto *headerRemoteOpen = dock.findChild<QPushButton *>(QStringLiteral("headerRemoteOpenButton"));
	const auto *demoGuide = dock.findChild<QPushButton *>(QStringLiteral("headerDemoGuideButton"));
	const auto *headerPro = dock.findChild<clipcoach::ui::ProBadge *>(QStringLiteral("headerProBadge"));
	const auto *planUpgrade = dock.findChild<QPushButton *>(QStringLiteral("planUpgradeButton"));
	const auto *footerVersion = dock.findChild<QLabel *>(QStringLiteral("footerVersionLabel"));
	const auto *footerCredits = dock.findChild<QLabel *>(QStringLiteral("footerDeveloperCredits"));
	const auto *footerUpdate = dock.findChild<QPushButton *>(QStringLiteral("footerCheckForUpdatesButton"));
	const auto *updateIndicator = dock.findChild<QLabel *>(QStringLiteral("updateAvailableIndicator"));
	const auto *footerReplay = dock.findChild<QPushButton *>(QStringLiteral("footerReplayBufferButton"));
	const auto *footerClip = dock.findChild<QPushButton *>(QStringLiteral("footerSaveClipButton"));
	const auto *captureRemote = dock.findChild<QPushButton *>(QStringLiteral("captureRemoteClipperButton"));
	const auto *dockHeader = dock.findChild<QWidget *>(QStringLiteral("DockHeader"));
	const auto *statusContainer = dock.findChild<QWidget *>(QStringLiteral("DockStatusContainer"));
	auto *previewOnly = dock.findChild<QPushButton *>(QStringLiteral("verticalPreviewOnlyButton"));
	auto *remoteAuthOverlay = dock.findChild<QWidget *>(QStringLiteral("remoteAuthOverlay"));
	auto *remoteAuthProgress = dock.findChild<QProgressBar *>(QStringLiteral("remoteAuthProgress"));

	clipcoach::test::expect(tabBar != nullptr, "MainDock must contain the primary TabBar");
	clipcoach::test::expect(stack != nullptr, "MainDock must contain the tab stack");
	clipcoach::test::expect(tabBar->count() == 5, "MainDock must expose exactly five primary tabs");
	clipcoach::test::expect(stack->count() == 5, "MainDock must expose five corresponding pages");
	clipcoach::test::expect(remoteAuthOverlay != nullptr && remoteAuthOverlay->isHidden() &&
					remoteAuthProgress != nullptr && remoteAuthProgress->minimum() == 0 &&
					remoteAuthProgress->maximum() == 0,
				"Remote authentication must provide a hidden blocking indeterminate progress overlay");
	clipcoach::test::expect(headerRemoteAuthenticate != nullptr && headerRemoteOpen != nullptr &&
					headerRemoteAuthenticate->text() ==
						QString::fromUtf8(clipcoach::ui::strings::kHeaderRemoteAuthenticate) &&
					headerRemoteOpen->text() ==
						QString::fromUtf8(clipcoach::ui::strings::kHeaderRemoteOpen),
				"the header must provide short authenticate and Remote Clipper buttons");
	clipcoach::test::expect(headerStatus != nullptr &&
					headerStatus->text() ==
						QString::fromUtf8(clipcoach::ui::strings::kHeaderBufferInactive),
				"the header must show a concise Replay Buffer state");
	clipcoach::test::expect(tabBar->currentIndex() == 0, "Vertical must be the default primary tab");
	clipcoach::test::expect(tabBar->tabText(0) == QString::fromUtf8(clipcoach::ui::strings::kTabVertical),
				"vertical tab must use its localization key");
	clipcoach::test::expect(tabBar->tabText(1) == QString::fromUtf8(clipcoach::ui::strings::kTabCapture),
				"capture tab must use its localization key");
	clipcoach::test::expect(tabBar->tabText(2) == QString::fromUtf8(clipcoach::ui::strings::kTabTriggers),
				"triggers tab must use its localization key");
	clipcoach::test::expect(tabBar->tabText(3) == QString::fromUtf8(clipcoach::ui::strings::kTabClips),
				"clips tab must use its localization key");
	clipcoach::test::expect(tabBar->tabText(4) == QString::fromUtf8(clipcoach::ui::strings::kTabSettings),
				"settings tab must use its localization key");
	clipcoach::test::expect(dock.findChild<QLabel *>(QStringLiteral("mainTabBadge")) == nullptr,
				"the Vertical tab must not display a redundant primary badge");
	clipcoach::test::expect(markMoment != nullptr, "capture tab must contain its primary action");
	clipcoach::test::expect(captureRemote != nullptr &&
					captureRemote->text() ==
						QString::fromUtf8(clipcoach::ui::strings::kSettingsRemoteOpenAccount),
				"capture tab must expose the localized Remote Clipper account action");
	clipcoach::test::expect(dock.findChild<QPushButton *>(QStringLiteral("footerRemoteClipperButton")) == nullptr,
				"global footer must not duplicate the Remote Clipper header action");
	clipcoach::remote::RemoteClipperStatus connectedRemote;
	connectedRemote.connection = clipcoach::remote::RemoteConnectionState::Connected;
	dock.setRemoteClipperStatus(connectedRemote);
	auto *remotePulseTimer = dock.findChild<QTimer *>(QStringLiteral("remoteStatusPulseTimer"));
	clipcoach::test::expect(
		headerRemoteAuthenticate->property("authenticated").toBool() &&
			headerRemoteAuthenticate->text() ==
				QString::fromUtf8(clipcoach::ui::strings::kHeaderRemoteAuthenticated) &&
			headerRemoteOpen->property("remoteState").toString() == QStringLiteral("online") &&
			!headerRemoteAuthenticate->icon().isNull() && !headerRemoteOpen->icon().isNull() &&
			remotePulseTimer != nullptr && remotePulseTimer->isActive(),
		"connected Remote Clipper controls must expose user/status icons and green state");
	clipcoach::remote::RemoteClipperStatus connectingRemote;
	connectingRemote.connection = clipcoach::remote::RemoteConnectionState::Connecting;
	dock.setRemoteClipperStatus(connectingRemote);
	clipcoach::test::expect(headerRemoteOpen->property("remoteState").toString() == QStringLiteral("online") &&
					remotePulseTimer->isActive(),
				"heartbeat polling must not flash the established Remote connection red");
	clipcoach::test::expect(startReplay != nullptr, "capture tab must contain Replay Buffer start action");
	clipcoach::test::expect(stopReplay != nullptr, "capture tab must contain Replay Buffer stop action");
	clipcoach::test::expect(fiveMinutes != nullptr, "capture tab must expose the five-minute quick duration");
	clipcoach::test::expect(!fiveMinutes->isEnabled(), "Free must keep five minutes visible but blocked");
	clipcoach::test::expect(durationProBadge != nullptr, "blocked duration controls must show a ProBadge");
	clipcoach::test::expect(upgradeBanner != nullptr && !upgradeBanner->isHidden(),
				"Free capture must show the upgrade banner");
	clipcoach::test::expect(verticalClip != nullptr && !verticalClip->isEnabled(),
				"Free vertical capture must remain visible and limited");
	clipcoach::test::expect(sessionCount != nullptr, "capture tab must expose its session clip counter");
	clipcoach::test::expect(verticalPreview != nullptr, "vertical tab must contain its 9:16 preview");
	clipcoach::test::expect(markMoment->text() == QString::fromUtf8(clipcoach::ui::strings::kCaptureSaveHorizontal),
				"primary capture action must identify the selected default format");
	clipcoach::test::expect(!requestedTranslationKeys.empty(),
				"MainDock must request visible strings through the translator");
	clipcoach::test::expect(footer != nullptr && footerVersion == nullptr && footerCredits != nullptr &&
					footerUpdate != nullptr && updateIndicator != nullptr &&
					updateIndicator->isHidden() && footerReplay != nullptr && footerClip != nullptr,
				"fixed footer must expose actions, hidden update indicator and developer credit");
	const auto *footerLayout = footer != nullptr ? qobject_cast<QHBoxLayout *>(footer->layout()) : nullptr;
	clipcoach::test::expect(footerLayout != nullptr, "fixed footer must use one non-wrapping horizontal row");
	clipcoach::test::expect(footerLayout->indexOf(footerReplay) < footerLayout->indexOf(footerCredits) &&
					footerLayout->indexOf(footerClip) < footerLayout->indexOf(footerCredits) &&
					footerLayout->indexOf(footerUpdate) < footerLayout->indexOf(footerCredits),
				"footer actions must stay left of developer credit");
	clipcoach::test::expect(footerReplay->height() == footerClip->height() &&
					footerClip->height() == footerUpdate->height() &&
					footerClip->height() == clipcoach::ui::tokens::kFooterControlHeight,
				"all visible footer actions must use the same compact footer height");
	clipcoach::test::expect(footerReplay->geometry().center().y() == footerClip->geometry().center().y() &&
					footerClip->geometry().center().y() == footerUpdate->geometry().center().y(),
				"footer actions must be vertically centered on the same baseline");
	dock.resize(560, 720);
	dock.show();
	application.processEvents();
	clipcoach::test::expect(footerReplay->text() == QStringLiteral("Replay") &&
					footerClip->text() == QStringLiteral("Clip") &&
					footerUpdate->text().isEmpty() &&
					footerUpdate->width() <= clipcoach::ui::tokens::kFooterControlHeight +
									 clipcoach::ui::tokens::kSpaceXs &&
					footerCredits->isHidden(),
				"narrow footer must use compact labels, an icon-only update action and hide credits");
	clipcoach::test::expect(!footerUpdate->icon().isNull() && !footerReplay->icon().isNull() &&
					!footerClip->icon().isNull(),
				"compact footer actions must remain identifiable by icon");
	clipcoach::test::expect(previewOnly != nullptr && previewOnly->isVisible() && previewOnly->isEnabled() &&
					dockHeader != nullptr && dockHeader->isVisible() &&
					statusContainer != nullptr && statusContainer->isVisible(),
				"narrow Vertical must expose a reversible preview-only action");
	previewOnly->click();
	application.processEvents();
	clipcoach::test::expect(previewOnly->isChecked(), "preview-only action must enter its checked state");
	clipcoach::test::expect(dockHeader->isHidden(), "preview-only mode must hide the dock header");
	clipcoach::test::expect(statusContainer->isHidden(), "preview-only mode must hide the status cards");
	clipcoach::test::expect(tabBar->isHidden(), "preview-only mode must hide the primary tab bar");
	clipcoach::test::expect(footer->isVisible(), "preview-only mode must keep the compact footer visible");
	clipcoach::test::expect(verticalPreview->isVisible(),
				"preview-only mode must keep the vertical camera visible");
	clipcoach::test::expect(previewOnly->isVisible(), "preview-only mode must keep a visible restore action");
	previewOnly->click();
	application.processEvents();
	clipcoach::test::expect(dockHeader->isVisible() && statusContainer->isVisible() && tabBar->isVisible(),
				"leaving preview-only mode must restore the complete dock navigation");
	dock.resize(1200, 720);
	application.processEvents();
	clipcoach::test::expect(footerReplay->text() != QStringLiteral("Replay") &&
					footerClip->text() != QStringLiteral("Clip") &&
					!footerUpdate->text().isEmpty() && !footerCredits->isHidden(),
				"wide footer must restore full labels and developer credit");
	clipcoach::test::expect(brandLabel != nullptr && brandLabel->text().contains(QStringLiteral(" - v")) &&
					headerStatus != nullptr && demoGuide != nullptr &&
					headerStatus->text() ==
						QString::fromUtf8(clipcoach::ui::strings::kHeaderBufferInactive),
				"header must show product version, demo guide action and the concise Buffer state");
	clipcoach::test::expect(headerPro != nullptr && headerPro->isHidden(),
				"header Pro badge must reflect the real Free entitlement instead of being decorative");
	clipcoach::test::expect(planUpgrade != nullptr && !planUpgrade->isHidden() &&
					planUpgrade->text() == QString::fromUtf8(clipcoach::ui::strings::kPlanUpgrade),
				"Free plan card must expose the localized upgrade action");
	clipcoach::test::expect(planUpgrade->property("externalUrl").toString().endsWith(QStringLiteral("/#pricing")),
				"plan upgrade action must target the public Pro pricing section");
	clipcoach::test::expect(brandMark != nullptr && !brandMark->pixmap().isNull() && brandMark->width() == 22 &&
					brandMark->height() == 22,
				"header must show the 22px ClipXtudio product icon");

	return clipcoach::test::pass("main-dock-ui-test");
}
