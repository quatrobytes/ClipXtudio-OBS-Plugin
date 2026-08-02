#include <clipcoach/ui/main-dock.hpp>
#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/trigger-engine.hpp>
#include <clipcoach/core/voice-trigger-controller.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>
#include <clipcoach/licensing/license-manager.hpp>
#include <clipcoach/network/qt-update-checker.hpp>
#include <clipcoach/network/qt-update-downloader.hpp>
#include <clipcoach/storage/clip-library-service.hpp>
#include <clipcoach/ui/components/clip-card.hpp>
#include <clipcoach/ui/components/context-help-button.hpp>
#include <clipcoach/ui/components/empty-state.hpp>
#include <clipcoach/ui/components/primary-button.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/components/secondary-button.hpp>
#include <clipcoach/ui/components/settings-section.hpp>
#include <clipcoach/ui/components/status-card.hpp>
#include <clipcoach/ui/components/tab-bar.hpp>
#include <clipcoach/ui/components/toggle-row.hpp>
#include <clipcoach/ui/components/upgrade-banner.hpp>
#include <clipcoach/ui/clips-tab.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/desktop-clip-action-service.hpp>
#include <clipcoach/ui/settings-tab.hpp>
#include <clipcoach/ui/triggers-tab.hpp>
#include <clipcoach/ui/ui-strings.hpp>
#include <clipcoach/ui/vertical-tab.hpp>

#include <QCoreApplication>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMetaObject>
#include <QPalette>
#include <QMessageBox>
#include <QPointer>
#include <QPixmap>
#include <QPainter>
#include <QPushButton>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QResizeEvent>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QTransform>
#include <QVBoxLayout>
#include <QUrl>
#include <QWizard>
#include <QWizardPage>

#include <algorithm>
#include <utility>

static void initializeClipXtudioResources()
{
	Q_INIT_RESOURCE(clipx_studio);
}

namespace clipcoach::ui {
namespace {

constexpr int kTransientNotificationMs = 3500;
constexpr int kCompactFooterActionsWidth = 700;
constexpr int kFooterCreditsWidth = 980;

class FinalSetupPage final : public QWizardPage {
public:
	using Validator = std::function<bool()>;
	using QWizardPage::QWizardPage;

	void setValidator(Validator validator) { validator_ = std::move(validator); }

	bool validatePage() override { return !validator_ || validator_(); }

private:
	Validator validator_;
};

QIcon recordingIcon()
{
	auto makePixmap = [](const QColor &color) {
		QPixmap pixmap(18, 18);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setPen(Qt::NoPen);
		painter.setBrush(color);
		painter.drawEllipse(QRectF(4.0, 4.0, 10.0, 10.0));
		return pixmap;
	};

	QIcon icon;
	icon.addPixmap(makePixmap(QColor(QStringLiteral("#F43F5E"))), QIcon::Normal);
	icon.addPixmap(makePixmap(QColor(QStringLiteral("#64748B"))), QIcon::Disabled);
	return icon;
}

QIcon remoteStatusIcon(bool online, bool bright = true)
{
	QPixmap pixmap(18, 18);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(QPen(online ? QColor(bright ? QStringLiteral("#BBF7D0") : QStringLiteral("#4ADE80"))
				   : QColor(QStringLiteral("#FCA5A5")),
			    1.2));
	painter.setBrush(online ? QColor(bright ? QStringLiteral("#4ADE80") : QStringLiteral("#15803D"))
				: QColor(QStringLiteral("#EF4444")));
	const qreal inset = online && bright ? 3.0 : 4.0;
	painter.drawEllipse(QRectF(inset, inset, 18.0 - (inset * 2.0), 18.0 - (inset * 2.0)));
	return QIcon(pixmap);
}

QIcon userAccountIcon()
{
	QPixmap pixmap(18, 18);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(QStringLiteral("#D8B4FE")));
	painter.drawEllipse(QRectF(6.0, 2.0, 6.0, 6.0));
	painter.drawRoundedRect(QRectF(3.5, 9.0, 11.0, 7.0), 3.5, 3.5);
	return QIcon(pixmap);
}

class ResponsiveGrid final : public QWidget {
public:
	ResponsiveGrid(int wideColumns, int mediumColumns, QWidget *parent = nullptr)
		: QWidget(parent),
		  wideColumns_(wideColumns),
		  mediumColumns_(mediumColumns)
	{
		layout_ = new QGridLayout(this);
		layout_->setContentsMargins(0, 0, 0, 0);
		layout_->setSpacing(tokens::kSpaceMd);
		layout_->setSizeConstraint(QLayout::SetMinimumSize);
		setMinimumWidth(0);
		setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	}

	void addCard(QWidget *card)
	{
		cards_.push_back(card);
		relayout();
	}

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		relayout();
	}

private:
	void relayout()
	{
		const int columns = width() >= 1120 ? wideColumns_ : width() >= 620 ? mediumColumns_ : 1;
		columns_ = columns;
		while (auto *item = layout_->takeAt(0))
			delete item;
		for (int index = 0; index < cards_.size(); ++index)
			layout_->addWidget(cards_.at(index), index / columns, index % columns);
		for (int column = 0; column < columns; ++column)
			layout_->setColumnStretch(column, 1);
	}

	QGridLayout *layout_{nullptr};
	QList<QWidget *> cards_;
	int wideColumns_{1};
	int mediumColumns_{1};
	int columns_{0};
};

void addPageHeading(QVBoxLayout *layout, const QString &title, const QString &subtitle, QWidget *parent)
{
	auto *header = new QFrame(parent);
	header->setObjectName(QStringLiteral("capturePageHeader"));
	header->setProperty("pageRole", QStringLiteral("header"));
	auto *headerLayout = new QHBoxLayout(header);
	headerLayout->setContentsMargins(tokens::kCardPaddingHorizontal, tokens::kCardPaddingVertical,
					 tokens::kCardPaddingHorizontal, tokens::kCardPaddingVertical);
	headerLayout->setSpacing(tokens::kSectionGap);

	auto *icon = new QLabel(header);
	icon->setObjectName(QStringLiteral("capturePageIcon"));
	icon->setProperty("pageRole", QStringLiteral("icon"));
	icon->setAlignment(Qt::AlignCenter);
	icon->setPixmap(parent->style()->standardIcon(QStyle::SP_DialogSaveButton).pixmap(22, 22));
	icon->setFixedSize(tokens::kPageHeaderIconSize, tokens::kPageHeaderIconSize);

	auto *copy = new QVBoxLayout();
	copy->setContentsMargins(0, 0, 0, 0);
	copy->setSpacing(tokens::kSpaceXs);
	auto *titleLabel = new QLabel(title, header);
	titleLabel->setObjectName(QStringLiteral("PageTitle"));
	auto *subtitleLabel = new QLabel(subtitle, header);
	subtitleLabel->setObjectName(QStringLiteral("PageSubtitle"));
	subtitleLabel->setWordWrap(true);
	copy->addWidget(titleLabel);
	copy->addWidget(subtitleLabel);

	headerLayout->addWidget(icon, 0, Qt::AlignVCenter);
	headerLayout->addLayout(copy, 1);
	layout->addWidget(header);
}

QScrollArea *wrapPage(QWidget *content, const QString &objectName)
{
	auto *scrollArea = new QScrollArea();
	scrollArea->setObjectName(objectName);
	scrollArea->setWidgetResizable(true);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setWidget(content);
	return scrollArea;
}

QVBoxLayout *createPageLayout(QWidget *content)
{
	auto *layout = new QVBoxLayout(content);
	layout->setContentsMargins(tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin);
	layout->setSpacing(tokens::kSectionGap);
	layout->setAlignment(Qt::AlignTop);
	return layout;
}

} // namespace

MainDock::MainDock(QWidget *parent)
	: MainDock([](const char *key) { return QCoreApplication::translate("ClipXtudioStudio", key); }, nullptr,
		   nullptr, nullptr, parent)
{
}

MainDock::MainDock(TranslationFunction translator, QWidget *parent)
	: MainDock(std::move(translator), nullptr, nullptr, nullptr, parent)
{
}

MainDock::MainDock(TranslationFunction translator, ClipManager *clipManager, SettingsManager *settingsManager,
		   storage::ClipLibraryService *libraryService, QWidget *parent, std::string sessionId)
	: MainDock(std::move(translator), clipManager, settingsManager, libraryService, parent, std::move(sessionId),
		   nullptr)
{
}

MainDock::MainDock(TranslationFunction translator, ClipManager *clipManager, SettingsManager *settingsManager,
		   storage::ClipLibraryService *libraryService, QWidget *parent, std::string sessionId,
		   VerticalCanvasManager *verticalManager, ExportManager *exportManager, TriggerEngine *triggerEngine,
		   integrations::ChatIntegrationManager *chatManager, licensing::LicenseManager *licenseManager,
		   FeatureGateService *featureGates, VoiceTriggerController *voiceController,
		   SceneProvider sceneProvider, VerticalObsBridge verticalObsBridge, CaptionGenerator captionGenerator)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  clipManager_(clipManager),
	  settingsManager_(settingsManager),
	  verticalManager_(verticalManager),
	  exportManager_(exportManager),
	  triggerEngine_(triggerEngine),
	  chatManager_(chatManager),
	  licenseManager_(licenseManager),
	  featureGates_(featureGates),
	  voiceController_(voiceController),
	  sceneProvider_(std::move(sceneProvider)),
	  verticalObsBridge_(std::move(verticalObsBridge)),
	  captionGenerator_(std::move(captionGenerator)),
	  libraryService_(libraryService),
	  sessionId_(std::move(sessionId))
{
	initializeClipXtudioResources();
	setObjectName(QStringLiteral("ClipXtudioMainDockContent"));
	setWindowIcon(QIcon(QStringLiteral(":/clipx/icon.png")));
	setAttribute(Qt::WA_StyledBackground, true);
	setMinimumSize(360, 480);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setStyleSheet(tokens::styleSheet());
	updateChecker_ = std::make_unique<network::QtUpdateChecker>(
		QList<QUrl>{QUrl(QStringLiteral(CLIPX_LOCAL_UPDATE_MANIFEST_URL)),
			    QUrl(QStringLiteral(CLIPX_UPDATE_MANIFEST_URL))});
	updateDownloader_ = std::make_unique<network::QtUpdateDownloader>();
	verticalCaptureActionService_ =
		std::make_unique<DesktopClipActionService>(exportManager_, settingsManager_, verticalManager_);

	auto *rootLayout = new QVBoxLayout(this);
	rootLayout->setContentsMargins(0, 0, 0, 0);
	rootLayout->setSpacing(0);

	dockHeader_ = new QWidget(this);
	dockHeader_->setObjectName(QStringLiteral("DockHeader"));
	dockHeader_->setAttribute(Qt::WA_StyledBackground, true);
	auto *headerLayout = new QHBoxLayout(dockHeader_);
	headerLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceMd);
	headerLayout->setSpacing(tokens::kSpaceSm);

	auto *brandMark = new QLabel(dockHeader_);
	brandMark->setObjectName(QStringLiteral("BrandMark"));
	brandMark->setPixmap(QIcon(QStringLiteral(":/clipx/icon.png")).pixmap(22, 22));
	brandMark->setFixedSize(22, 22);
	brandMark->setScaledContents(true);
	auto *brandLabel =
		new QLabel(QStringLiteral("%1 - v%2").arg(text(strings::kBrandName), QStringLiteral(CLIPCOACH_VERSION)),
			   dockHeader_);
	brandLabel->setObjectName(QStringLiteral("BrandLabel"));
	headerProBadge_ = new ProBadge(text(strings::kPro), dockHeader_);
	headerProBadge_->setObjectName(QStringLiteral("headerProBadge"));
	headerProBadge_->hide();
	headerStatus_ = new QLabel(text(strings::kFooterReady), dockHeader_);
	headerStatus_->setObjectName(QStringLiteral("headerStatusBadge"));
	headerStatus_->setProperty("notificationTone", QStringLiteral("success"));
	headerStatus_->setAlignment(Qt::AlignCenter);
	headerStatus_->setMaximumWidth(260);
	headerStatus_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	captureNotification_ = new QLabel(dockHeader_);
	captureNotification_->setObjectName(QStringLiteral("captureNotification"));
	captureNotification_->setWordWrap(false);
	captureNotification_->setAlignment(Qt::AlignCenter);
	captureNotification_->setMaximumHeight(30);
	captureNotification_->setMaximumWidth(360);
	captureNotification_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
	captureNotification_->hide();
	headerDemoGuideButton_ = new QPushButton(text(strings::kHeaderDemoGuide), dockHeader_);
	headerDemoGuideButton_->setObjectName(QStringLiteral("headerDemoGuideButton"));
	headerDemoGuideButton_->setProperty("buttonRole", QStringLiteral("link"));
	headerDemoGuideButton_->setToolTip(text(strings::kHeaderDemoGuide));
	headerDemoGuideButton_->setEnabled(false);
	headerCapturePendingDot_ = new QLabel(dockHeader_);
	headerCapturePendingDot_->setObjectName(QStringLiteral("headerCapturePendingDot"));
	headerCapturePendingDot_->setFixedSize(10, 10);
	headerCapturePendingDot_->setProperty("pulseOn", false);
	headerCapturePendingDot_->setToolTip(text(strings::kCaptureSaving));
	headerCapturePendingDot_->setAccessibleName(text(strings::kCaptureSaving));
	headerCapturePendingDot_->hide();
	headerRemoteStatus_ = new QLabel(text(strings::kHeaderRemoteOffline), dockHeader_);
	headerRemoteStatus_->setObjectName(QStringLiteral("headerRemoteClipperStatus"));
	headerRemoteStatus_->setProperty("notificationTone", QStringLiteral("warning"));
	headerRemoteStatus_->setAlignment(Qt::AlignCenter);
	headerRemoteStatus_->setToolTip(QStringLiteral("Remote Clipper"));
	headerRemoteStatus_->hide();
	headerRemoteAuthenticateButton_ = new QPushButton(text(strings::kHeaderRemoteAuthenticate), dockHeader_);
	headerRemoteAuthenticateButton_->setObjectName(QStringLiteral("headerRemoteAuthenticateButton"));
	headerRemoteAuthenticateButton_->setProperty("buttonRole", QStringLiteral("secondary"));
	headerRemoteAuthenticateButton_->setProperty("authenticated", false);
	headerRemoteAuthenticateButton_->setIcon(userAccountIcon());
	headerRemoteAuthenticateButton_->setToolTip(text(strings::kSettingsRemoteAuthenticate));
	headerRemoteOpenButton_ = new QPushButton(text(strings::kHeaderRemoteOpen), dockHeader_);
	headerRemoteOpenButton_->setObjectName(QStringLiteral("headerRemoteOpenButton"));
	headerRemoteOpenButton_->setProperty("buttonRole", QStringLiteral("secondary"));
	headerRemoteOpenButton_->setProperty("remoteState", QStringLiteral("offline"));
	headerRemoteOpenButton_->setIcon(remoteStatusIcon(false));
	headerRemoteOpenButton_->setToolTip(text(strings::kRemoteToggleTurnOn));
	remoteStatusPulseTimer_ = new QTimer(this);
	remoteStatusPulseTimer_->setObjectName(QStringLiteral("remoteStatusPulseTimer"));
	remoteStatusPulseTimer_->setInterval(720);
	connect(remoteStatusPulseTimer_, &QTimer::timeout, this, [this] {
		if (!remoteOnline_ || headerRemoteOpenButton_ == nullptr)
			return;
		remoteStatusPulseBright_ = !remoteStatusPulseBright_;
		headerRemoteOpenButton_->setIcon(remoteStatusIcon(true, remoteStatusPulseBright_));
	});
	capturePendingPulseTimer_ = new QTimer(this);
	capturePendingPulseTimer_->setInterval(420);
	connect(capturePendingPulseTimer_, &QTimer::timeout, this, [this] {
		if (headerCapturePendingDot_ == nullptr)
			return;
		capturePendingPulseOn_ = !capturePendingPulseOn_;
		headerCapturePendingDot_->setProperty("pulseOn", capturePendingPulseOn_);
		headerCapturePendingDot_->style()->unpolish(headerCapturePendingDot_);
		headerCapturePendingDot_->style()->polish(headerCapturePendingDot_);
	});
	headerLayout->addWidget(brandMark);
	headerLayout->addWidget(brandLabel);
	headerLayout->addStretch(1);
	headerLayout->addWidget(captureNotification_);
	headerLayout->addWidget(headerCapturePendingDot_, 0, Qt::AlignVCenter);
	headerLayout->addWidget(headerDemoGuideButton_);
	headerLayout->addWidget(headerRemoteAuthenticateButton_);
	headerLayout->addWidget(headerRemoteOpenButton_);
	headerLayout->addWidget(headerStatus_);
	headerLayout->addWidget(headerProBadge_);
	connect(headerDemoGuideButton_, &QPushButton::clicked, this, [this] {
		if (demoGuideUrl_.isValid())
			QDesktopServices::openUrl(demoGuideUrl_);
	});
	connect(headerRemoteAuthenticateButton_, &QPushButton::clicked, this, [this] { beginRemoteAuthentication(); });
	connect(headerRemoteOpenButton_, &QPushButton::clicked, this, [this] { toggleRemoteClipper(); });

	statusContainer_ = new QWidget(this);
	statusContainer_->setObjectName(QStringLiteral("DockStatusContainer"));
	auto *statusLayout = new QHBoxLayout(statusContainer_);
	statusLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceMd);
	statusLayout->setSpacing(tokens::kSpaceSm);
	replayStatusCard_ =
		new StatusCard(text(strings::kHeaderReplay), text(strings::kStateInactive), false, statusContainer_);
	statusLayout->addWidget(replayStatusCard_);
	const auto initialWidth = settingsManager_ != nullptr ? settingsManager_->settings().verticalWidth
							      : settings_constraints::kPortrait1080Width;
	const auto initialHeight = settingsManager_ != nullptr ? settingsManager_->settings().verticalHeight
							       : settings_constraints::kPortrait1080Height;
	verticalStatusCard_ = new StatusCard(text(strings::kHeaderVertical),
					     QStringLiteral("%1×%2").arg(initialWidth).arg(initialHeight), false,
					     statusContainer_);
	verticalStatusCard_->setObjectName(QStringLiteral("verticalCanvasStatusCard"));
	statusLayout->addWidget(verticalStatusCard_);
	planStatusCard_ = new StatusCard(text(strings::kHeaderPlan), text(strings::kPlanFree), false, statusContainer_);
	planStatusCard_->setObjectName(QStringLiteral("planStatusCard"));
	planUpgradeButton_ = planStatusCard_->addActionButton(text(strings::kPlanUpgrade));
	planUpgradeButton_->setObjectName(QStringLiteral("planUpgradeButton"));
	QUrl proPurchaseUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL));
	proPurchaseUrl.setPath(QStringLiteral("/"));
	proPurchaseUrl.setFragment(QStringLiteral("pricing"));
	planUpgradeButton_->setProperty("externalUrl", proPurchaseUrl.toString());
	planUpgradeButton_->setToolTip(text(strings::kPlanUpgrade));
	connect(planUpgradeButton_, &QPushButton::clicked, this, [proPurchaseUrl] {
		const bool securePublicUrl =
			proPurchaseUrl.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
		const bool localDevelopmentUrl =
			proPurchaseUrl.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
			(proPurchaseUrl.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0 ||
			 proPurchaseUrl.host() == QStringLiteral("127.0.0.1"));
		if (proPurchaseUrl.isValid() && (securePublicUrl || localDevelopmentUrl))
			QDesktopServices::openUrl(proPurchaseUrl);
	});
	statusLayout->addWidget(planStatusCard_);

	mainTabBar_ = new TabBar(this);
	mainTabBar_->addNavigationTab(style()->standardIcon(QStyle::SP_DesktopIcon), text(strings::kTabVertical));
	mainTabBar_->addNavigationTab(style()->standardIcon(QStyle::SP_DialogSaveButton), text(strings::kTabCapture));
	mainTabBar_->addNavigationTab(style()->standardIcon(QStyle::SP_BrowserReload), text(strings::kTabTriggers));
	mainTabBar_->addNavigationTab(style()->standardIcon(QStyle::SP_FileDialogListView), text(strings::kTabClips));
	mainTabBar_->addNavigationTab(style()->standardIcon(QStyle::SP_FileDialogDetailedView),
				      text(strings::kTabSettings));

	auto *stack = new QStackedWidget(this);
	stack->setObjectName(QStringLiteral("mainTabStack"));
	stack->addWidget(createVerticalPage());
	stack->addWidget(createCapturePage());
	stack->addWidget(createTriggersPage());
	stack->addWidget(createClipsPage());
	stack->addWidget(createSettingsPage());

	connect(mainTabBar_, &QTabBar::currentChanged, stack, &QStackedWidget::setCurrentIndex);
	connect(mainTabBar_, &QTabBar::currentChanged, this, [this](int index) {
		if (index == 0 && verticalTab_ != nullptr) {
			verticalTab_->refresh();
		}
		if (index == 3 && clipsTab_ != nullptr) {
			clipsTab_->reload();
		}
	});
	mainTabBar_->setCurrentIndex(0);

	rootLayout->addWidget(dockHeader_);
	rootLayout->addWidget(statusContainer_);
	rootLayout->addWidget(mainTabBar_);
	rootLayout->addWidget(stack, 1);
	rootLayout->addWidget(createGlobalFooter());

	captionBusyOverlay_ = new QFrame(this);
	captionBusyOverlay_->setObjectName(QStringLiteral("captionBusyOverlay"));
	captionBusyOverlay_->setAttribute(Qt::WA_StyledBackground, true);
	captionBusyOverlay_->setFocusPolicy(Qt::StrongFocus);
	auto *busyOuter = new QVBoxLayout(captionBusyOverlay_);
	busyOuter->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl);
	busyOuter->addStretch(1);
	auto *busyCard = new QFrame(captionBusyOverlay_);
	busyCard->setObjectName(QStringLiteral("captionBusyCard"));
	busyCard->setAttribute(Qt::WA_StyledBackground, true);
	busyCard->setMinimumSize(480, 220);
	busyCard->setMaximumWidth(560);
	busyCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	auto *busyLayout = new QVBoxLayout(busyCard);
	busyLayout->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl);
	busyLayout->setSpacing(tokens::kSpaceMd);
	auto *busyTitle = new QLabel(text(strings::kClipsCaptionProgressTitle), busyCard);
	busyTitle->setObjectName(QStringLiteral("captionBusyTitle"));
	busyTitle->setAlignment(Qt::AlignCenter);
	captionBusyLabel_ = new QLabel(text(strings::kClipsCaptionGenerating), busyCard);
	captionBusyLabel_->setObjectName(QStringLiteral("captionBusyLabel"));
	captionBusyLabel_->setAlignment(Qt::AlignCenter);
	captionBusyLabel_->setWordWrap(true);
	captionBusyLabel_->setMinimumHeight(44);
	captionBusyLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
	captionBusyPercent_ = new QLabel(QStringLiteral("0%"), busyCard);
	captionBusyPercent_->setObjectName(QStringLiteral("captionBusyPercent"));
	captionBusyPercent_->setAlignment(Qt::AlignCenter);
	captionBusySpinner_ = new QLabel(busyCard);
	captionBusySpinner_->setObjectName(QStringLiteral("captionBusySpinner"));
	captionBusySpinner_->setFixedSize(28, 28);
	captionBusySpinner_->setAlignment(Qt::AlignCenter);
	auto *busyPercentRow = new QHBoxLayout();
	busyPercentRow->setContentsMargins(0, 0, 0, 0);
	busyPercentRow->setSpacing(tokens::kSpaceSm);
	busyPercentRow->addStretch(1);
	busyPercentRow->addWidget(captionBusySpinner_);
	busyPercentRow->addWidget(captionBusyPercent_);
	busyPercentRow->addStretch(1);
	captionBusyProgress_ = new QProgressBar(busyCard);
	captionBusyProgress_->setObjectName(QStringLiteral("captionBusyProgress"));
	captionBusyProgress_->setRange(0, 100);
	captionBusyProgress_->setValue(0);
	captionBusyProgress_->setTextVisible(false);
	captionBusyEta_ = new QLabel(busyCard);
	captionBusyEta_->setObjectName(QStringLiteral("captionBusyEta"));
	captionBusyEta_->setAlignment(Qt::AlignCenter);
	busyLayout->addWidget(busyTitle);
	busyLayout->addWidget(captionBusyLabel_);
	busyLayout->addLayout(busyPercentRow);
	busyLayout->addWidget(captionBusyProgress_);
	busyLayout->addWidget(captionBusyEta_);
	busyOuter->addWidget(busyCard, 0, Qt::AlignHCenter);
	busyOuter->addStretch(1);
	captionBusyOverlay_->hide();
	captionBusyOverlay_->setGeometry(rect());
	remoteAuthOverlay_ = new QFrame(this);
	remoteAuthOverlay_->setObjectName(QStringLiteral("remoteAuthOverlay"));
	remoteAuthOverlay_->setAttribute(Qt::WA_StyledBackground, true);
	remoteAuthOverlay_->setFocusPolicy(Qt::StrongFocus);
	auto *remoteOuter = new QVBoxLayout(remoteAuthOverlay_);
	remoteOuter->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl);
	remoteOuter->addStretch(1);
	auto *remoteCard = new QFrame(remoteAuthOverlay_);
	remoteCard->setObjectName(QStringLiteral("remoteAuthCard"));
	remoteCard->setAttribute(Qt::WA_StyledBackground, true);
	remoteCard->setMinimumSize(440, 180);
	remoteCard->setMaximumWidth(560);
	auto *remoteLayout = new QVBoxLayout(remoteCard);
	remoteLayout->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl);
	remoteLayout->setSpacing(tokens::kSpaceLg);
	remoteAuthTitle_ = new QLabel(text(strings::kRemoteAuthTitle), remoteCard);
	remoteAuthTitle_->setObjectName(QStringLiteral("remoteAuthTitle"));
	remoteAuthTitle_->setAlignment(Qt::AlignCenter);
	remoteAuthMessage_ = new QLabel(text(strings::kRemoteAuthRefreshing), remoteCard);
	remoteAuthMessage_->setObjectName(QStringLiteral("remoteAuthMessage"));
	remoteAuthMessage_->setAlignment(Qt::AlignCenter);
	remoteAuthMessage_->setWordWrap(true);
	remoteAuthProgress_ = new QProgressBar(remoteCard);
	remoteAuthProgress_->setObjectName(QStringLiteral("remoteAuthProgress"));
	remoteAuthProgress_->setRange(0, 0);
	remoteAuthProgress_->setTextVisible(false);
	remoteLayout->addWidget(remoteAuthTitle_);
	remoteLayout->addWidget(remoteAuthMessage_);
	remoteLayout->addWidget(remoteAuthProgress_);
	remoteOuter->addWidget(remoteCard, 0, Qt::AlignHCenter);
	remoteOuter->addStretch(1);
	remoteAuthOverlay_->hide();
	remoteAuthOverlay_->setGeometry(rect());
	remoteAuthTimeout_ = new QTimer(this);
	remoteAuthTimeout_->setSingleShot(true);
	remoteAuthTimeout_->setInterval(15000);
	connect(remoteAuthTimeout_, &QTimer::timeout, this, [this] {
		if (remoteTogglePending_)
			finishRemoteToggle(false, text(strings::kRemoteToggleFailedTitle),
					   text(strings::kRemoteToggleFailed), true);
		else
			finishRemoteAuthentication(false, text(strings::kRemoteAuthFailedTitle),
						   text(strings::kRemoteAuthFailed));
	});
	captionProgressTimer_ = new QTimer(this);
	captionProgressTimer_->setObjectName(QStringLiteral("captionProgressTimer"));
	captionProgressTimer_->setInterval(250);
	connect(captionProgressTimer_, &QTimer::timeout, this, [this] {
		captionSpinnerAngle_ = (captionSpinnerAngle_ + 30) % 360;
		if (captionBusySpinner_ != nullptr) {
			const auto source = style()->standardIcon(QStyle::SP_BrowserReload).pixmap(QSize(20, 20));
			captionBusySpinner_->setPixmap(source.transformed(QTransform().rotate(captionSpinnerAngle_),
									  Qt::SmoothTransformation));
		}
		++captionProgressTimerTicks_;
		if (captionProgressTimerTicks_ % 4 == 0) {
			if (captionEstimatedSecondsRemaining_ > 0)
				--captionEstimatedSecondsRemaining_;
			++captionSecondsWithoutProgress_;
			updateCaptionEta();
			updateCaptionActivityMessage();
		}
	});
	QTimer::singleShot(1500, this, [this] { checkForUpdates(false); });
	publicConfigNetwork_ = std::make_unique<QNetworkAccessManager>();
	QTimer::singleShot(0, this, [this] { loadPublicPluginConfiguration(); });

	bindCaptureFlow();
	if (licenseManager_ != nullptr) {
		licenseObserverId_ = licenseManager_->addObserver([this](const licensing::LicenseSnapshot &snapshot) {
			const bool active = snapshot.proEnabled();
			if (triggerEngine_ != nullptr)
				triggerEngine_->setProUnlocked(active);
			if (voiceController_ != nullptr)
				voiceController_->setProUnlocked(active);
			if (verticalManager_ != nullptr)
				verticalManager_->setProUnlocked(active);
			planStatusCard_->setValue(
				snapshot.refreshInProgress                             ? text(strings::kPlanValidating)
				: snapshot.state == licensing::LicenseState::ProActive ? text(strings::kPlanProActive)
				: snapshot.state == licensing::LicenseState::ProGrace  ? text(strings::kPlanProGrace)
										       : text(strings::kPlanFree));
			planStatusCard_->setActive(active);
			planStatusCard_->setActionVisible(!active);
			headerProBadge_->setVisible(active);
			updateFeatureGateUi();
			if (settingsTab_ != nullptr)
				settingsTab_->refreshLicenseState();
			if (triggersTab_ != nullptr)
				triggersTab_->refreshLicenseState();
			if (verticalTab_ != nullptr)
				verticalTab_->refresh();
			if (!snapshot.refreshInProgress && snapshot.proEnabled() && remoteCommandsChangedCallback_)
				remoteCommandsChangedCallback_(settingsManager_ == nullptr ||
							       settingsManager_->settings().remoteCommandsEnabled);
		});
	}
	if (voiceController_ != nullptr && settingsManager_ != nullptr) {
		auto *voiceRecoveryTimer = new QTimer(this);
		voiceRecoveryTimer->setInterval(4000);
		connect(voiceRecoveryTimer, &QTimer::timeout, this, [this] {
			if (voiceController_ == nullptr || settingsManager_ == nullptr)
				return;
			const auto state = voiceController_->status().state;
			if (state == VoiceRuntimeState::WaitingForAudio ||
			    state == VoiceRuntimeState::SourceUnavailable)
				voiceController_->applySettings(settingsManager_->settings());
		});
		voiceRecoveryTimer->start();
	}
	updateFeatureGateUi();
}

void MainDock::loadPublicPluginConfiguration(int endpointIndex)
{
	const QList<QUrl> bases{
		QUrl(QStringLiteral(CLIPX_LOCAL_SERVICE_BASE_URL)),
		QUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL)),
	};
	if (publicConfigNetwork_ == nullptr || endpointIndex < 0 || endpointIndex >= bases.size())
		return;

	QUrl url = bases.at(endpointIndex);
	url.setPath(QStringLiteral("/api/public/plugin-config"));
	if (!url.isValid() || url.host().isEmpty())
		return;
	QNetworkRequest request(url);
	request.setRawHeader("Accept", "application/json");
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(5000);
	auto *reply = publicConfigNetwork_->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, reply, endpointIndex] {
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		const auto document = QJsonDocument::fromJson(reply->readAll());
		const auto data = document.object().value(QStringLiteral("data")).toObject();
		const auto demoValue = data.value(QStringLiteral("demo_video_url")).toString();
		const auto supportValue = data.value(QStringLiteral("support_url")).toString();
		const QUrl demoCandidate(demoValue);
		const QUrl supportCandidate(supportValue);
		const auto isSafe = [](const QUrl &candidate) {
			if (!candidate.isValid())
				return false;
			if (candidate.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
				return true;
			const auto host = candidate.host().toLower();
			return candidate.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
			       (host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost") ||
				host == QStringLiteral("::1"));
		};
		const QUrl candidate = isSafe(demoCandidate) ? demoCandidate : supportCandidate;
		const bool safe = isSafe(candidate);
		if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300) {
			demoGuideUrl_ = safe ? candidate : QUrl{};
			headerDemoGuideButton_->setEnabled(safe);
		} else {
			loadPublicPluginConfiguration(endpointIndex + 1);
		}
		reply->deleteLater();
	});
}

MainDock::~MainDock()
{
	if (licenseManager_ != nullptr && licenseObserverId_ != 0)
		licenseManager_->removeObserver(licenseObserverId_);
	if (clipManager_ != nullptr) {
		clipManager_->setClipSavedCallback({});
		clipManager_->setErrorCallback({});
		clipManager_->setReplayStateCallback({});
	}
}

void MainDock::refreshClipLibrary()
{
	if (clipsTab_ != nullptr)
		clipsTab_->reload();
}

void MainDock::setLanguageChangedCallback(LanguageChangedCallback callback)
{
	languageChangedCallback_ = std::move(callback);
}

void MainDock::setHotkeySettingsChangedCallback(HotkeySettingsChangedCallback callback)
{
	hotkeySettingsChangedCallback_ = std::move(callback);
}

void MainDock::setReplayDurationChangedCallback(ReplayDurationChangedCallback callback)
{
	replayDurationChangedCallback_ = std::move(callback);
}

void MainDock::setRemoteCommandsChangedCallback(RemoteCommandsChangedCallback callback)
{
	remoteCommandsChangedCallback_ = std::move(callback);
}

void MainDock::setRemoteClipperStatus(const remote::RemoteClipperStatus &status)
{
	const bool reportedOnline = status.connection == remote::RemoteConnectionState::Connected;
	// Polling briefly reports Connecting before every successful heartbeat.
	// Preserve the established online state during that transition so the
	// header does not flash red every few seconds.
	if (reportedOnline)
		remoteOnline_ = true;
	else if (status.connection != remote::RemoteConnectionState::Connecting)
		remoteOnline_ = false;
	if (status.connection == remote::RemoteConnectionState::Connected ||
	    status.connection == remote::RemoteConnectionState::Unavailable)
		remoteAuthenticated_ = true;
	else if (status.connection == remote::RemoteConnectionState::Unauthorized)
		remoteAuthenticated_ = false;
	if (headerRemoteAuthenticateButton_ != nullptr) {
		headerRemoteAuthenticateButton_->setText(text(remoteAuthenticated_
								      ? strings::kHeaderRemoteAuthenticated
								      : strings::kHeaderRemoteAuthenticate));
		headerRemoteAuthenticateButton_->setProperty("authenticated", remoteAuthenticated_);
		headerRemoteAuthenticateButton_->style()->unpolish(headerRemoteAuthenticateButton_);
		headerRemoteAuthenticateButton_->style()->polish(headerRemoteAuthenticateButton_);
	}
	if (headerRemoteOpenButton_ != nullptr) {
		headerRemoteOpenButton_->setProperty("remoteState", remoteOnline_ ? QStringLiteral("online")
										  : QStringLiteral("offline"));
		remoteStatusPulseBright_ = true;
		headerRemoteOpenButton_->setIcon(remoteStatusIcon(remoteOnline_, remoteStatusPulseBright_));
		headerRemoteOpenButton_->setToolTip(
			text(remoteOnline_ ? strings::kRemoteToggleTurnOff : strings::kRemoteToggleTurnOn));
		headerRemoteOpenButton_->style()->unpolish(headerRemoteOpenButton_);
		headerRemoteOpenButton_->style()->polish(headerRemoteOpenButton_);
	}
	if (remoteStatusPulseTimer_ != nullptr) {
		if (remoteOnline_ && !remoteStatusPulseTimer_->isActive())
			remoteStatusPulseTimer_->start();
		else if (!remoteOnline_) {
			remoteStatusPulseTimer_->stop();
			remoteStatusPulseBright_ = true;
		}
	}
	if (headerRemoteStatus_ != nullptr) {
		QString label = text(strings::kHeaderRemoteOffline);
		QString tone = QStringLiteral("warning");
		if (status.connection == remote::RemoteConnectionState::Connected) {
			label = status.pendingCommands > 0
					? text(strings::kHeaderRemotePending).arg(status.pendingCommands)
					: text(strings::kHeaderRemoteConnected);
			tone = QStringLiteral("success");
		} else if (status.connection == remote::RemoteConnectionState::Connecting) {
			label = text(strings::kHeaderRemoteConnecting);
			tone = QStringLiteral("info");
		} else if (status.connection == remote::RemoteConnectionState::Paused) {
			label = text(strings::kHeaderRemotePaused);
		} else if (status.connection == remote::RemoteConnectionState::Unauthorized) {
			label = text(strings::kHeaderRemoteUnauthorized);
			tone = QStringLiteral("error");
		}
		headerRemoteStatus_->setText(label);
		headerRemoteStatus_->setToolTip(QString::fromStdString(status.message));
		headerRemoteStatus_->setProperty("notificationTone", tone);
		headerRemoteStatus_->style()->unpolish(headerRemoteStatus_);
		headerRemoteStatus_->style()->polish(headerRemoteStatus_);
	}
	if (settingsTab_ != nullptr)
		settingsTab_->setRemoteClipperStatus(status);
	if (remoteTogglePending_) {
		if (status.connection == remote::RemoteConnectionState::Connecting) {
			remoteToggleSawConnecting_ = true;
			return;
		}
		if (!remoteToggleTargetEnabled_ && status.connection == remote::RemoteConnectionState::Paused) {
			finishRemoteToggle(true, text(strings::kRemoteToggleOfflineTitle),
					   text(strings::kRemoteToggleOffline));
			return;
		}
		if (remoteToggleTargetEnabled_ && remoteToggleSawConnecting_) {
			if (status.connection == remote::RemoteConnectionState::Connected) {
				finishRemoteToggle(true, text(strings::kRemoteToggleOnlineTitle),
						   text(strings::kRemoteToggleOnline));
			} else if (status.connection == remote::RemoteConnectionState::Unauthorized ||
				   status.connection == remote::RemoteConnectionState::Offline ||
				   status.connection == remote::RemoteConnectionState::Unavailable ||
				   status.connection == remote::RemoteConnectionState::Paused) {
				finishRemoteToggle(false, text(strings::kRemoteToggleFailedTitle),
						   text(strings::kRemoteToggleFailed), true);
			}
			return;
		}
	}
	if (!remoteAuthenticationPending_)
		return;
	if (status.connection == remote::RemoteConnectionState::Connecting) {
		remoteAuthenticationSawConnecting_ = true;
		if (remoteAuthMessage_ != nullptr)
			remoteAuthMessage_->setText(text(strings::kRemoteAuthChecking));
		return;
	}
	if (!remoteAuthenticationSawConnecting_)
		return;
	if (status.connection == remote::RemoteConnectionState::Connected) {
		finishRemoteAuthentication(true, text(strings::kRemoteAuthSuccessTitle),
					   text(strings::kRemoteAuthSuccess));
	} else if (status.connection == remote::RemoteConnectionState::Unavailable) {
		finishRemoteAuthentication(true, text(strings::kRemoteAuthNoSessionTitle),
					   text(strings::kRemoteAuthNoSession), true);
	} else if (status.connection == remote::RemoteConnectionState::Unauthorized &&
		   status.errorCode == "REMOTE_ADDON_REQUIRED") {
		finishRemoteAuthentication(false, text(strings::kRemoteAuthAddonRequiredTitle),
					   text(strings::kRemoteAuthAddonRequired), true);
	} else if (status.connection == remote::RemoteConnectionState::Unauthorized ||
		   status.connection == remote::RemoteConnectionState::Offline ||
		   status.connection == remote::RemoteConnectionState::Paused) {
		QString failureMessage = text(strings::kRemoteAuthFailed);
		if (!status.errorCode.empty()) {
			failureMessage +=
				QStringLiteral("\n\n") +
				text(strings::kRemoteAuthErrorCode).arg(QString::fromStdString(status.errorCode));
		}
		finishRemoteAuthentication(false, text(strings::kRemoteAuthFailedTitle), failureMessage);
	}
}

bool MainDock::persistRemoteCommandsEnabled(bool enabled)
{
	if (settingsManager_ == nullptr || !remoteCommandsChangedCallback_)
		return false;
	auto settings = settingsManager_->settings();
	settings.remoteCommandsEnabled = enabled;
	std::string error;
	if (!settingsManager_->save(settings, &error))
		return false;
	if (settingsTab_ != nullptr)
		settingsTab_->setRemoteCommandsEnabled(enabled);
	remoteCommandsChangedCallback_(enabled);
	return true;
}

void MainDock::toggleRemoteClipper()
{
	if (remoteTogglePending_ || remoteAuthenticationPending_)
		return;
	const bool targetEnabled = !remoteOnline_;
	if (targetEnabled && !remoteAuthenticated_) {
		auto *dialog = new QMessageBox(QMessageBox::Warning, text(strings::kRemoteToggleAuthRequiredTitle),
					       text(strings::kRemoteToggleAuthRequired), QMessageBox::Ok, this);
		dialog->setAttribute(Qt::WA_DeleteOnClose, true);
		tokens::configureRemoteAuthenticationDialog(dialog);
		dialog->open();
		return;
	}
	remoteTogglePending_ = true;
	remoteToggleTargetEnabled_ = targetEnabled;
	remoteToggleSawConnecting_ = false;
	if (headerRemoteOpenButton_ != nullptr)
		headerRemoteOpenButton_->setEnabled(false);
	if (headerRemoteAuthenticateButton_ != nullptr)
		headerRemoteAuthenticateButton_->setEnabled(false);
	if (remoteAuthTitle_ != nullptr)
		remoteAuthTitle_->setText(text(strings::kRemoteToggleTitle));
	if (remoteAuthMessage_ != nullptr)
		remoteAuthMessage_->setText(
			text(targetEnabled ? strings::kRemoteToggleConnecting : strings::kRemoteToggleDisconnecting));
	if (remoteAuthOverlay_ != nullptr) {
		remoteAuthOverlay_->setGeometry(rect());
		remoteAuthOverlay_->show();
		remoteAuthOverlay_->raise();
		remoteAuthOverlay_->setFocus(Qt::OtherFocusReason);
	}
	if (remoteAuthTimeout_ != nullptr)
		remoteAuthTimeout_->start();
	if (!persistRemoteCommandsEnabled(targetEnabled))
		finishRemoteToggle(false, text(strings::kRemoteToggleFailedTitle), text(strings::kRemoteToggleFailed));
}

void MainDock::finishRemoteToggle(bool success, const QString &title, const QString &message, bool rollback)
{
	const bool failedTarget = remoteToggleTargetEnabled_;
	remoteTogglePending_ = false;
	remoteToggleSawConnecting_ = false;
	if (remoteAuthTimeout_ != nullptr)
		remoteAuthTimeout_->stop();
	if (remoteAuthOverlay_ != nullptr)
		remoteAuthOverlay_->hide();
	if (headerRemoteOpenButton_ != nullptr)
		headerRemoteOpenButton_->setEnabled(true);
	if (headerRemoteAuthenticateButton_ != nullptr)
		headerRemoteAuthenticateButton_->setEnabled(true);
	if (rollback)
		(void)persistRemoteCommandsEnabled(!failedTarget);
	auto *dialog = new QMessageBox(success ? QMessageBox::Information : QMessageBox::Warning, title, message,
				       QMessageBox::Ok, this);
	dialog->setObjectName(QStringLiteral("remoteToggleResultDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	tokens::configureRemoteAuthenticationDialog(dialog);
	dialog->open();
}

void MainDock::openRemoteClipperAccount()
{
	QUrl url(QStringLiteral(CLIPX_SERVICE_BASE_URL));
	url.setPath(QStringLiteral("/account/remote-clipper"));
	url.setQuery(QString{});
	url.setFragment(QString{});
	const bool securePublicUrl = url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0;
	const bool localDevelopmentUrl = url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
					 (url.host().compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0 ||
					  url.host() == QStringLiteral("127.0.0.1"));
	if (url.isValid() && (securePublicUrl || localDevelopmentUrl))
		QDesktopServices::openUrl(url);
}

void MainDock::beginRemoteAuthentication()
{
	if (remoteAuthenticationPending_ || remoteTogglePending_)
		return;
	if (licenseManager_ == nullptr) {
		finishRemoteAuthentication(false, text(strings::kRemoteAuthFailedTitle),
					   text(strings::kRemoteAuthFailed));
		return;
	}
	remoteAuthenticationPending_ = true;
	remoteAuthenticationSawConnecting_ = false;
	if (headerRemoteAuthenticateButton_ != nullptr)
		headerRemoteAuthenticateButton_->setEnabled(false);
	if (headerRemoteOpenButton_ != nullptr)
		headerRemoteOpenButton_->setEnabled(false);
	if (settingsTab_ != nullptr)
		settingsTab_->setRemoteAuthenticationBusy(true);
	if (remoteAuthMessage_ != nullptr)
		remoteAuthMessage_->setText(text(strings::kRemoteAuthRefreshing));
	if (remoteAuthTitle_ != nullptr)
		remoteAuthTitle_->setText(text(strings::kRemoteAuthTitle));
	if (remoteAuthOverlay_ != nullptr) {
		remoteAuthOverlay_->setGeometry(rect());
		remoteAuthOverlay_->show();
		remoteAuthOverlay_->raise();
		remoteAuthOverlay_->setFocus(Qt::OtherFocusReason);
	}
	if (remoteAuthTimeout_ != nullptr)
		remoteAuthTimeout_->start();
	QPointer<MainDock> self(this);
	licenseManager_->refresh([self](const licensing::LicenseSnapshot &snapshot) {
		if (self.isNull())
			return;
		QMetaObject::invokeMethod(
			self,
			[self, snapshot] {
				if (self.isNull() || !self->remoteAuthenticationPending_)
					return;
				if (!snapshot.proEnabled() || !snapshot.lastErrorCode.empty()) {
					self->finishRemoteAuthentication(false,
									 self->text(strings::kRemoteAuthFailedTitle),
									 self->text(strings::kRemoteAuthFailed));
					return;
				}
				if (self->remoteAuthMessage_ != nullptr)
					self->remoteAuthMessage_->setText(self->text(strings::kRemoteAuthChecking));
				if (self->remoteCommandsChangedCallback_)
					self->remoteCommandsChangedCallback_(true);
				else
					self->finishRemoteAuthentication(false,
									 self->text(strings::kRemoteAuthFailedTitle),
									 self->text(strings::kRemoteAuthFailed));
			},
			Qt::QueuedConnection);
	});
}

void MainDock::finishRemoteAuthentication(bool success, const QString &title, const QString &message, bool openAccount)
{
	remoteAuthenticationPending_ = false;
	remoteAuthenticationSawConnecting_ = false;
	if (remoteAuthTimeout_ != nullptr)
		remoteAuthTimeout_->stop();
	if (remoteAuthOverlay_ != nullptr)
		remoteAuthOverlay_->hide();
	if (settingsTab_ != nullptr)
		settingsTab_->setRemoteAuthenticationBusy(false);
	if (headerRemoteAuthenticateButton_ != nullptr)
		headerRemoteAuthenticateButton_->setEnabled(true);
	if (headerRemoteOpenButton_ != nullptr)
		headerRemoteOpenButton_->setEnabled(true);
	auto *dialog = new QMessageBox(success ? QMessageBox::Information : QMessageBox::Warning, title, message,
				       QMessageBox::Ok, this);
	dialog->setObjectName(QStringLiteral("remoteAuthenticationResultDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setTextInteractionFlags(Qt::TextSelectableByMouse);
	if (openAccount) {
		auto *button = dialog->addButton(text(strings::kSettingsRemoteOpenAccount), QMessageBox::ActionRole);
		connect(button, &QPushButton::clicked, this, [] {
			QUrl url(QStringLiteral(CLIPX_SERVICE_BASE_URL));
			url.setPath(QStringLiteral("/account/remote-clipper"));
			url.setQuery(QString{});
			url.setFragment(QString{});
			QDesktopServices::openUrl(url);
		});
	}
	tokens::configureRemoteAuthenticationDialog(dialog);
	dialog->setWindowModality(Qt::WindowModal);
	dialog->open();
}

QWidget *MainDock::createGlobalFooter()
{
	auto *footer = new QWidget(this);
	globalFooter_ = footer;
	footer->setObjectName(QStringLiteral("GlobalFooter"));
	footer->setAttribute(Qt::WA_StyledBackground, true);
	auto *layout = new QHBoxLayout(footer);
	layout->setContentsMargins(tokens::kSpaceLg, 10, tokens::kSpaceLg, 10);
	layout->setSpacing(tokens::kSpaceSm);
	layout->setAlignment(Qt::AlignVCenter);
	footer->setMinimumHeight(tokens::kFooterControlHeight + 20);

	footerReplayButton_ = new SecondaryButton(text(strings::kActionStartReplay), footer);
	footerReplayButton_->setObjectName(QStringLiteral("footerReplayBufferButton"));
	footerReplayButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	footerClipButton_ = new PrimaryButton(text(strings::kFooterSaveClip), footer);
	footerClipButton_->setObjectName(QStringLiteral("footerSaveClipButton"));
	footerClipButton_->setIcon(recordingIcon());
	footerUpdateButton_ = new SecondaryButton(text(strings::kSettingsCheckUpdates), footer);
	footerUpdateButton_->setObjectName(QStringLiteral("footerCheckForUpdatesButton"));
	footerUpdateButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	updateAvailableIndicator_ = new QLabel(footer);
	updateAvailableIndicator_->setObjectName(QStringLiteral("updateAvailableIndicator"));
	updateAvailableIndicator_->setProperty("pulseOn", true);
	updateAvailableIndicator_->setToolTip(text(strings::kSettingsNewUpdateBadge));
	updateAvailableIndicator_->hide();
	updatePulseTimer_ = new QTimer(this);
	updatePulseTimer_->setInterval(650);
	connect(updatePulseTimer_, &QTimer::timeout, this, [this] {
		if (updateAvailableIndicator_ == nullptr)
			return;
		const bool pulseOn = !updateAvailableIndicator_->property("pulseOn").toBool();
		updateAvailableIndicator_->setProperty("pulseOn", pulseOn);
		updateAvailableIndicator_->style()->unpolish(updateAvailableIndicator_);
		updateAvailableIndicator_->style()->polish(updateAvailableIndicator_);
	});
	updateSpinnerTimer_ = new QTimer(this);
	updateSpinnerTimer_->setInterval(90);
	connect(updateSpinnerTimer_, &QTimer::timeout, this, [this] {
		if (footerUpdateButton_ == nullptr)
			return;
		updateSpinnerAngle_ = (updateSpinnerAngle_ + 30) % 360;
		const auto source = style()->standardIcon(QStyle::SP_BrowserReload).pixmap(QSize(18, 18));
		footerUpdateButton_->setIcon(
			QIcon(source.transformed(QTransform().rotate(updateSpinnerAngle_), Qt::SmoothTransformation)));
	});
	for (auto *button : {footerReplayButton_, footerClipButton_, footerUpdateButton_}) {
		button->setFixedHeight(tokens::kFooterControlHeight);
		button->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	}
	footerCredits_ = new QLabel(text(strings::kSettingsCredits), footer);
	footerCredits_->setObjectName(QStringLiteral("footerDeveloperCredits"));
	footerCredits_->setTextFormat(Qt::RichText);
	footerCredits_->setTextInteractionFlags(Qt::TextBrowserInteraction);
	footerCredits_->setOpenExternalLinks(true);
	footerCredits_->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

	// One fixed, non-wrapping command bar across every tab. Product version and
	// runtime state live in the header; the footer keeps actions and credit.
	layout->addWidget(footerReplayButton_, 0, Qt::AlignVCenter);
	layout->addWidget(footerClipButton_, 0, Qt::AlignVCenter);
	layout->addWidget(footerUpdateButton_, 0, Qt::AlignVCenter);
	layout->addWidget(updateAvailableIndicator_, 0, Qt::AlignVCenter);
	layout->addStretch(1);
	layout->addWidget(footerCredits_, 0, Qt::AlignVCenter);
	setFooterExpandedText(footerReplayButton_, text(strings::kActionStartReplay));
	setFooterExpandedText(footerClipButton_, text(strings::kFooterSaveClip));
	setFooterExpandedText(footerUpdateButton_, text(strings::kSettingsCheckUpdates));
	updateFooterResponsivePresentation();
	connect(footerUpdateButton_, &QPushButton::clicked, this, [this] {
		if (updateDownloadUrl_.isValid())
			showUpdateAvailableDialog();
		else
			checkForUpdates(true);
	});
	return footer;
}

void MainDock::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (captionBusyOverlay_ != nullptr)
		captionBusyOverlay_->setGeometry(rect());
	if (remoteAuthOverlay_ != nullptr)
		remoteAuthOverlay_->setGeometry(rect());
	updateFooterResponsivePresentation();
}

void MainDock::setCaptionProcessing(bool busy, const CaptionGenerationProgress &progress)
{
	if (captionBusyOverlay_ == nullptr)
		return;
	const int percentage = std::clamp(progress.percentage, 0, 100);
	if (!progress.status.trimmed().isEmpty())
		captionReportedStatus_ = progress.status.trimmed();
	if (!captionProcessingActive_ || percentage != captionLastPercentage_) {
		captionSecondsWithoutProgress_ = 0;
		captionLastPercentage_ = percentage;
	}
	captionProcessingActive_ = busy;
	if (captionBusyLabel_ != nullptr && !captionReportedStatus_.isEmpty())
		captionBusyLabel_->setText(captionReportedStatus_);
	if (captionBusyProgress_ != nullptr)
		captionBusyProgress_->setValue(percentage);
	if (captionBusyPercent_ != nullptr)
		captionBusyPercent_->setText(QStringLiteral("%1%").arg(percentage));
	captionEstimatedSecondsRemaining_ = std::max(0, progress.estimatedSecondsRemaining);
	updateCaptionEta();
	captionBusyOverlay_->setGeometry(rect());
	captionBusyOverlay_->setVisible(busy);
	if (busy) {
		captionBusyOverlay_->raise();
		captionBusyOverlay_->setFocus(Qt::OtherFocusReason);
		if (captionProgressTimer_ != nullptr)
			captionProgressTimer_->start();
	} else if (captionProgressTimer_ != nullptr) {
		captionProgressTimer_->stop();
		captionProgressTimerTicks_ = 0;
		captionSecondsWithoutProgress_ = 0;
		captionSpinnerAngle_ = 0;
	}
}

void MainDock::updateCaptionActivityMessage()
{
	if (!captionProcessingActive_ || captionBusyLabel_ == nullptr || captionSecondsWithoutProgress_ < 4)
		return;
	const int cycle = (captionSecondsWithoutProgress_ / 6) % 3;
	const char *key = strings::kClipsCaptionStillWorking;
	if (captionSecondsWithoutProgress_ >= 18 && cycle == 0)
		key = strings::kClipsCaptionTakingLonger;
	else if (captionLastPercentage_ >= 90 || cycle == 2)
		key = strings::kClipsCaptionFinishingDetails;
	else if (captionLastPercentage_ >= 75 || cycle == 1)
		key = strings::kClipsCaptionAlmostThere;
	captionBusyLabel_->setText(text(key));
}

void MainDock::updateCaptionEta()
{
	if (captionBusyEta_ == nullptr)
		return;
	if (captionEstimatedSecondsRemaining_ <= 0) {
		captionBusyEta_->setText(text(captionLastPercentage_ >= 90 ? strings::kClipsCaptionFinalizing
									   : strings::kClipsCaptionStillWorking));
		return;
	}
	if (captionEstimatedSecondsRemaining_ >= 90) {
		const int minutes = (captionEstimatedSecondsRemaining_ + 59) / 60;
		captionBusyEta_->setText(text(strings::kClipsCaptionEtaMinutes).arg(minutes));
		return;
	}
	captionBusyEta_->setText(text(strings::kClipsCaptionEtaSeconds).arg(captionEstimatedSecondsRemaining_));
}

void MainDock::setFooterExpandedText(QPushButton *button, const QString &expandedText)
{
	if (button == nullptr)
		return;
	button->setProperty("expandedFooterText", expandedText);
	button->setAccessibleName(expandedText);
	updateFooterResponsivePresentation();
}

void MainDock::updateFooterResponsivePresentation()
{
	if (globalFooter_ == nullptr || footerReplayButton_ == nullptr || footerClipButton_ == nullptr ||
	    footerUpdateButton_ == nullptr)
		return;

	const int availableWidth = globalFooter_->width() > 0 ? globalFooter_->width() : width();
	const bool compactActions = availableWidth < kCompactFooterActionsWidth;
	if (footerCredits_ != nullptr)
		footerCredits_->setVisible(availableWidth >= kFooterCreditsWidth);

	const auto expandedText = [](QPushButton *button) {
		return button->property("expandedFooterText").toString();
	};
	footerReplayButton_->setText(compactActions ? QStringLiteral("Replay") : expandedText(footerReplayButton_));
	footerClipButton_->setText(compactActions ? QStringLiteral("Clip") : expandedText(footerClipButton_));
	footerUpdateButton_->setText(compactActions ? QString{} : expandedText(footerUpdateButton_));

	footerUpdateButton_->setMinimumWidth(compactActions ? tokens::kFooterControlHeight + tokens::kSpaceXs : 0);
	footerUpdateButton_->setMaximumWidth(compactActions ? tokens::kFooterControlHeight + tokens::kSpaceXs
							    : QWIDGETSIZE_MAX);
	if (compactActions && footerUpdateButton_->toolTip().isEmpty())
		footerUpdateButton_->setToolTip(expandedText(footerUpdateButton_));
}

QString MainDock::text(const char *key) const
{
	if (translator_) {
		return translator_(key);
	}
	return QString::fromUtf8(key);
}

QWidget *MainDock::createCapturePage()
{
	auto *content = new QWidget();
	content->setObjectName(QStringLiteral("captureDashboard"));
	auto *layout = createPageLayout(content);
	addPageHeading(layout, text(strings::kCaptureTitle), text(strings::kCaptureSubtitle), content);

	auto makeCard = [content](const QString &name) {
		auto *card = new QFrame(content);
		card->setObjectName(name);
		card->setProperty("captureRole", QStringLiteral("card"));
		card->setFrameShape(QFrame::NoFrame);
		card->setMinimumWidth(0);
		card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		return card;
	};
	auto addSectionTitle = [](QVBoxLayout *target, const QString &value, QWidget *parent) {
		auto *label = new QLabel(value, parent);
		label->setObjectName(QStringLiteral("CaptureSectionTitle"));
		target->addWidget(label);
	};
	auto makeDescription = [](const QString &value, QWidget *parent) {
		auto *label = new QLabel(value, parent);
		label->setObjectName(QStringLiteral("CaptureDescription"));
		label->setWordWrap(true);
		return label;
	};

	auto *actionsCard = makeCard(QStringLiteral("captureQuickActionsCard"));
	actionsCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	auto *actionsRoot = new QVBoxLayout(actionsCard);
	actionsRoot->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceLg);
	actionsRoot->setSpacing(tokens::kSpaceMd);
	addSectionTitle(actionsRoot, QStringLiteral("⚡  %1").arg(text(strings::kCaptureActionsTitle)), actionsCard);
	auto *actionsGrid = new ResponsiveGrid(4, 2, actionsCard);

	auto makeActionPanel = [&](const QString &name, QPushButton *button, const QString &description, int column,
				   bool featured = false) {
		auto *panel = makeCard(name);
		panel->setProperty("captureRole",
				   featured ? QStringLiteral("actionFeatured") : QStringLiteral("action"));
		panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		panel->setMinimumHeight(76);
		auto *panelLayout = new QVBoxLayout(panel);
		panelLayout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd);
		panelLayout->setSpacing(tokens::kSpaceSm);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		button->setToolTip(description);
		auto *actionLine = new QHBoxLayout();
		actionLine->setContentsMargins(0, 0, 0, 0);
		actionLine->setSpacing(tokens::kSpaceSm);
		actionLine->addWidget(button, 1);
		auto *help = new ContextHelpButton(description, panel);
		help->setObjectName(button->objectName() + QStringLiteral("InfoButton"));
		actionLine->addWidget(help, 0, Qt::AlignVCenter);
		panelLayout->addLayout(actionLine);
		Q_UNUSED(column);
		actionsGrid->addCard(panel);
	};

	startReplayButton_ = new PrimaryButton(text(strings::kActionStartReplay), actionsCard);
	startReplayButton_->setObjectName(QStringLiteral("startReplayBufferButton"));
	startReplayButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	markMomentButton_ = new SecondaryButton(text(strings::kCaptureSaveClip), actionsCard);
	markMomentButton_->setObjectName(QStringLiteral("markMomentButton"));
	markMomentButton_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
	saveVerticalButton_ = new SecondaryButton(text(strings::kActionSaveVertical), actionsCard);
	saveVerticalButton_->setObjectName(QStringLiteral("saveVerticalClipButton"));
	saveVerticalButton_->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
	captureGoVerticalButton_ = new SecondaryButton(text(strings::kCaptureGoVertical), actionsCard);
	captureGoVerticalButton_->setObjectName(QStringLiteral("captureGoVerticalButton"));
	captureGoVerticalButton_->setIcon(style()->standardIcon(QStyle::SP_DesktopIcon));
	captureRemoteClipperButton_ = new SecondaryButton(text(strings::kSettingsRemoteOpenAccount), actionsCard);
	captureRemoteClipperButton_->setObjectName(QStringLiteral("captureRemoteClipperButton"));
	captureRemoteClipperButton_->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
	makeActionPanel(QStringLiteral("captureStartAction"), startReplayButton_,
			text(strings::kCaptureStartDescription), 0, true);
	makeActionPanel(QStringLiteral("captureSaveAction"), markMomentButton_, text(strings::kCaptureSaveDescription),
			1);
	makeActionPanel(QStringLiteral("captureVerticalAction"), saveVerticalButton_,
			text(strings::kCaptureVerticalDescription), 2);
	makeActionPanel(QStringLiteral("captureDesignerAction"), captureGoVerticalButton_,
			text(strings::kCaptureGoVerticalDescription), 3);
	makeActionPanel(QStringLiteral("captureRemoteClipperAction"), captureRemoteClipperButton_,
			text(strings::kSettingsRemoteDescription), 4);
	actionsRoot->addWidget(actionsGrid);

	verticalLimitedLabel_ = new QLabel(text(strings::kCaptureVerticalLimited), actionsCard);
	verticalLimitedLabel_->setObjectName(QStringLiteral("verticalProRequirementLabel"));
	verticalLimitedLabel_->setProperty("class", QStringLiteral("supporting"));
	actionsRoot->addWidget(verticalLimitedLabel_);
	captureUpgradeBanner_ = new UpgradeBanner(text(strings::kUpgradeTitle), text(strings::kUpgradeDescription),
						  text(strings::kUpgradeAction), text(strings::kPro), actionsCard);
	captureUpgradeBanner_->setObjectName(QStringLiteral("captureUpgradeBanner"));
	actionsRoot->addWidget(captureUpgradeBanner_);

	auto *middleHost = new ResponsiveGrid(2, 2, content);
	middleHost->setObjectName(QStringLiteral("captureMiddleGrid"));

	auto *bufferCard = makeCard(QStringLiteral("captureBufferStatusCard"));
	auto *bufferLayout = new QVBoxLayout(bufferCard);
	bufferLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceLg);
	bufferLayout->setSpacing(tokens::kSpaceMd);
	addSectionTitle(bufferLayout, text(strings::kCaptureBufferState), bufferCard);
	auto *bufferBody = new QHBoxLayout();
	auto *bufferOrb = new QLabel(QStringLiteral("■"), bufferCard);
	bufferOrb->setObjectName(QStringLiteral("captureBufferOrb"));
	bufferOrb->setAlignment(Qt::AlignCenter);
	bufferOrb->setFixedSize(108, 108);
	auto *bufferCopy = new QVBoxLayout();
	captureReplayStateLabel_ = new QLabel(text(strings::kStateInactive), bufferCard);
	captureReplayStateLabel_->setObjectName(QStringLiteral("captureReplayStateLabel"));
	captureReplayStateDescription_ = makeDescription(text(strings::kCaptureBufferInactiveDescription), bufferCard);
	captureReplayStateDescription_->setObjectName(QStringLiteral("captureReplayStateDescription"));
	bufferCopy->addStretch(1);
	bufferCopy->addWidget(captureReplayStateLabel_);
	bufferCopy->addWidget(captureReplayStateDescription_);
	bufferCopy->addStretch(1);
	bufferBody->addWidget(bufferOrb, 0, Qt::AlignVCenter);
	bufferBody->addLayout(bufferCopy, 1);
	bufferLayout->addLayout(bufferBody);

	auto *controlsCard = makeCard(QStringLiteral("captureControlsCard"));
	auto *controlsLayout = new QVBoxLayout(controlsCard);
	controlsLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceLg);
	controlsLayout->setSpacing(tokens::kSpaceMd);
	addSectionTitle(controlsLayout, text(strings::kCaptureOutputTitle), controlsCard);
	captureOutputModeCombo_ = new QComboBox(controlsCard);
	captureOutputModeCombo_->setObjectName(QStringLiteral("captureOutputModeCombo"));
	captureOutputModeCombo_->addItem(text(strings::kSettingsHorizontal),
					 static_cast<int>(CaptureOutputMode::Horizontal));
	captureOutputModeCombo_->addItem(text(strings::kSettingsVertical),
					 static_cast<int>(CaptureOutputMode::Vertical));
	captureOutputModeCombo_->addItem(text(strings::kSettingsBoth), static_cast<int>(CaptureOutputMode::Both));
	captureOutputModeCombo_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	controlsLayout->addWidget(captureOutputModeCombo_);
	captureOutputModeDescription_ = makeDescription(text(strings::kCaptureOutputDescription), controlsCard);
	captureOutputModeDescription_->setObjectName(QStringLiteral("captureOutputModeDescription"));
	controlsLayout->addWidget(captureOutputModeDescription_);

	addSectionTitle(controlsLayout, QStringLiteral("◷  %1").arg(text(strings::kCaptureQuick)), controlsCard);
	auto *durationLayout = new QHBoxLayout();
	durationLayout->setSpacing(tokens::kSpaceSm);
	const auto configuredDurations = settingsManager_ != nullptr
						 ? settingsManager_->settings().quickDurationsSeconds
						 : Settings::defaults().quickDurationsSeconds;
	for (const auto seconds : configuredDurations) {
		const QString label = seconds == 15    ? text(strings::kCaptureFifteen)
				      : seconds == 30  ? text(strings::kCaptureThirty)
				      : seconds == 60  ? text(strings::kCaptureSixty)
				      : seconds == 120 ? text(strings::kCaptureTwoMinutes)
				      : seconds == 300 ? text(strings::kCaptureFiveMinutes)
						       : text(strings::kCaptureDurationSeconds).arg(seconds);
		auto *button = new SecondaryButton(label, controlsCard);
		button->setObjectName(QStringLiteral("durationButton%1").arg(seconds));
		button->setCheckable(true);
		button->setProperty("durationSeconds", seconds);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		durationButtons_.push_back(button);
		durationLayout->addWidget(button);
	}
	durationProBadge_ = new ProBadge(text(strings::kPro), controlsCard);
	durationProBadge_->setObjectName(QStringLiteral("captureDurationProBadge"));
	durationLayout->addWidget(durationProBadge_, 0, Qt::AlignVCenter);
	controlsLayout->addLayout(durationLayout);

	addSectionTitle(controlsLayout, QStringLiteral("⌨  %1").arg(text(strings::kCaptureQuickShortcuts)),
			controlsCard);
	auto *shortcutGrid = new QGridLayout();
	shortcutGrid->setSpacing(tokens::kSpaceSm);
	const auto settings = settingsManager_ != nullptr ? settingsManager_->settings() : Settings::defaults();
	auto makeShortcut = [&](const QString &name, const QString &label, const std::string &sequence, int row,
				int column) {
		auto *button = new SecondaryButton(
			sequence.empty() ? label
					 : QStringLiteral("%1     %2").arg(label, QString::fromStdString(sequence)),
			controlsCard);
		button->setObjectName(name);
		button->setProperty("captureRole", QStringLiteral("shortcut"));
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		shortcutGrid->addWidget(button, row, column);
		return button;
	};
	captureShortcutMarkButton_ = makeShortcut(QStringLiteral("captureShortcutMarkMoment"),
						  text(strings::kActionMarkMoment), settings.markMomentHotkey, 0, 0);
	captureShortcutSaveButton_ = makeShortcut(QStringLiteral("captureShortcutSaveClip"),
						  text(strings::kCaptureSaveClip), settings.save30Hotkey, 0, 1);
	captureShortcutVerticalButton_ = makeShortcut(QStringLiteral("captureShortcutSaveVertical"),
						      text(strings::kActionSaveVertical), settings.saveVerticalHotkey,
						      1, 0);
	stopReplayButton_ = makeShortcut(QStringLiteral("stopReplayBufferButton"), text(strings::kActionStopReplay),
					 std::string{}, 1, 1);
	captureShortcutStopButton_ = stopReplayButton_;
	controlsLayout->addLayout(shortcutGrid);
	connect(captureOutputModeCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (verticalManager_ == nullptr || index < 0)
			return;
		const auto mode = static_cast<CaptureOutputMode>(captureOutputModeCombo_->itemData(index).toInt());
		std::string error;
		if (!verticalManager_->setOutputMode(mode, &error))
			showCaptureNotification(QString::fromStdString(error), true);
		refreshCaptureOutputMode();
		if (verticalTab_ != nullptr)
			verticalTab_->refresh();
	});
	refreshCaptureOutputMode();
	middleHost->addCard(bufferCard);
	middleHost->addCard(controlsCard);

	auto *bottomHost = new ResponsiveGrid(2, 2, content);
	auto *lastClipCard = makeCard(QStringLiteral("captureLastClipCard"));
	auto *lastClipLayout = new QVBoxLayout(lastClipCard);
	lastClipLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceLg);
	lastClipLayout->setSpacing(tokens::kSpaceMd);
	addSectionTitle(lastClipLayout, text(strings::kCaptureLastClip), lastClipCard);
	auto *lastClipBody = new QHBoxLayout();
	lastClipThumbnailLabel_ = new QLabel(text(strings::kCaptureNoPreview), lastClipCard);
	lastClipThumbnailLabel_->setObjectName(QStringLiteral("lastClipThumbnail"));
	lastClipThumbnailLabel_->setAlignment(Qt::AlignCenter);
	lastClipThumbnailLabel_->setWordWrap(true);
	lastClipThumbnailLabel_->setMinimumSize(180, 104);
	lastClipThumbnailLabel_->setMaximumWidth(260);
	auto *lastClipCopy = new QVBoxLayout();
	lastClipValueLabel_ = new QLabel(text(strings::kCaptureNoLastClip), lastClipCard);
	lastClipValueLabel_->setObjectName(QStringLiteral("lastClipValueLabel"));
	lastClipValueLabel_->setWordWrap(true);
	lastClipMetadataLabel_ = makeDescription(QString{}, lastClipCard);
	lastClipMetadataLabel_->setObjectName(QStringLiteral("lastClipMetadataLabel"));
	sessionClipCountLabel_ = new QLabel(text(strings::kCaptureSessionCount).arg(0), lastClipCard);
	sessionClipCountLabel_->setObjectName(QStringLiteral("sessionClipCountLabel"));
	auto *viewLibrary = new SecondaryButton(text(strings::kCaptureViewLibrary), lastClipCard);
	viewLibrary->setObjectName(QStringLiteral("captureViewLibraryButton"));
	lastClipCopy->addWidget(lastClipValueLabel_);
	lastClipCopy->addWidget(lastClipMetadataLabel_);
	lastClipCopy->addWidget(sessionClipCountLabel_);
	lastClipCopy->addStretch(1);
	lastClipCopy->addWidget(viewLibrary);
	lastClipBody->addWidget(lastClipThumbnailLabel_);
	lastClipBody->addLayout(lastClipCopy, 1);
	lastClipLayout->addLayout(lastClipBody);
	sessionClipLayout_ = lastClipLayout;

	auto *flowCard = makeCard(QStringLiteral("captureRecommendedFlowCard"));
	auto *flowLayout = new QVBoxLayout(flowCard);
	flowLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceLg);
	flowLayout->setSpacing(tokens::kSpaceMd);
	addSectionTitle(flowLayout, text(strings::kCaptureRecommendedFlow), flowCard);
	auto *flowGrid = new QGridLayout();
	flowGrid->setSpacing(tokens::kSpaceMd);
	const std::pair<const char *, const char *> flowSteps[] = {
		{strings::kCaptureFlowStartTitle, strings::kCaptureFlowStartBody},
		{strings::kCaptureFlowCaptureTitle, strings::kCaptureFlowCaptureBody},
		{strings::kCaptureFlowVerticalTitle, strings::kCaptureFlowVerticalBody},
	};
	for (int index = 0; index < 3; ++index) {
		auto *step = makeCard(QStringLiteral("captureFlowStep%1").arg(index + 1));
		step->setProperty("captureRole", QStringLiteral("flowStep"));
		auto *stepLayout = new QVBoxLayout(step);
		stepLayout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd);
		auto *number = new QLabel(QString::number(index + 1), step);
		number->setObjectName(QStringLiteral("CaptureStepNumber"));
		number->setAlignment(Qt::AlignCenter);
		number->setFixedSize(32, 32);
		auto *title = new QLabel(text(flowSteps[index].first), step);
		title->setObjectName(QStringLiteral("CaptureStepTitle"));
		title->setAlignment(Qt::AlignCenter);
		auto *body = makeDescription(text(flowSteps[index].second), step);
		body->setAlignment(Qt::AlignCenter);
		stepLayout->addWidget(number, 0, Qt::AlignHCenter);
		stepLayout->addWidget(title);
		stepLayout->addWidget(body);
		flowGrid->addWidget(step, 0, index);
		flowGrid->setColumnStretch(index, 1);
	}
	flowLayout->addLayout(flowGrid);
	bottomHost->addCard(lastClipCard);
	bottomHost->addCard(flowCard);

	layout->addWidget(actionsCard);
	layout->addWidget(middleHost);
	layout->addWidget(bottomHost);
	layout->addStretch(1);

	connect(captureGoVerticalButton_, &QPushButton::clicked, this, [this] {
		if (mainTabBar_ != nullptr)
			mainTabBar_->setCurrentIndex(0);
	});
	connect(captureRemoteClipperButton_, &QPushButton::clicked, this, [this] { openRemoteClipperAccount(); });
	connect(viewLibrary, &QPushButton::clicked, this, [this] {
		if (mainTabBar_ != nullptr)
			mainTabBar_->setCurrentIndex(3);
	});
	return wrapPage(content, QStringLiteral("captureTab"));
}

QWidget *MainDock::createVerticalPage()
{
	verticalTab_ = new VerticalTab(translator_, verticalManager_, verticalObsBridge_);
	if (auto *previewOnlyButton =
		    verticalTab_->findChild<QPushButton *>(QStringLiteral("verticalPreviewOnlyButton"));
	    previewOnlyButton != nullptr) {
		connect(previewOnlyButton, &QPushButton::toggled, this,
			[this](bool enabled) { setVerticalPreviewOnly(enabled); });
	}
	verticalTab_->setCanvasChangedCallback([this](const VerticalCanvasSettings &settings) {
		if (verticalStatusCard_ != nullptr) {
			verticalStatusCard_->setValue(QStringLiteral("%1×%2").arg(settings.width).arg(settings.height));
		}
		refreshCaptureOutputMode();
	});
	verticalTab_->setCaptureActions(
		[this] {
			if (clipManager_ != nullptr)
				(void)clipManager_->startReplayBuffer();
		},
		[this] { captureSelectedClip(); });
	return verticalTab_;
}

void MainDock::refreshVerticalObsScenes()
{
	if (verticalTab_ != nullptr)
		verticalTab_->refreshObsSceneOptions();
}

void MainDock::showInitialSetupIfNeeded(bool force)
{
	if (settingsManager_ == nullptr || findChild<QWizard *>(QStringLiteral("clipXtudioInitialSetup")) != nullptr)
		return;
	const auto &stored = settingsManager_->settings();
	if (!force && stored.initialSetupCompleted && stored.initialSetupCompletedVersion == CLIPCOACH_VERSION)
		return;

	constexpr auto managedScene = "ClipXtudio Vertical";
	auto scenes = verticalObsBridge_.scenes ? verticalObsBridge_.scenes()
						: (sceneProvider_ ? sceneProvider_() : std::vector<std::string>{});
	if (std::find(scenes.begin(), scenes.end(), managedScene) == scenes.end() && verticalManager_ != nullptr &&
	    verticalObsBridge_.createVerticalScene && verticalObsBridge_.activeScene) {
		const auto activeScene = verticalObsBridge_.activeScene();
		if (!activeScene.empty()) {
			std::string ignored;
			(void)verticalObsBridge_.createVerticalScene(activeScene, {}, verticalManager_->settings(),
								     &ignored);
			scenes = verticalObsBridge_.scenes ? verticalObsBridge_.scenes() : scenes;
		}
	}

	auto *wizard = new QWizard(this);
	wizard->setObjectName(QStringLiteral("clipXtudioInitialSetup"));
	wizard->setAttribute(Qt::WA_DeleteOnClose, true);
	wizard->setWindowTitle(text(strings::kInitialSetupTitle));
	wizard->setWizardStyle(QWizard::ModernStyle);
	wizard->setOption(QWizard::NoBackButtonOnStartPage, true);
	wizard->setModal(true);
	wizard->resize(720, 590);
	wizard->setMinimumSize(620, 500);
	wizard->setButtonText(QWizard::BackButton, text(strings::kInitialSetupBack));
	wizard->setButtonText(QWizard::NextButton, text(strings::kInitialSetupNext));
	wizard->setButtonText(QWizard::FinishButton, text(strings::kInitialSetupFinish));
	wizard->setButtonText(QWizard::CancelButton, text(strings::kInitialSetupLater));
	auto wizardPalette = wizard->palette();
	wizardPalette.setColor(QPalette::Window, QColor(QStringLiteral("#0C1016")));
	wizardPalette.setColor(QPalette::WindowText, QColor(QStringLiteral("#F4F7FB")));
	wizardPalette.setColor(QPalette::Base, QColor(QStringLiteral("#111925")));
	wizardPalette.setColor(QPalette::Text, QColor(QStringLiteral("#F4F7FB")));
	wizardPalette.setColor(QPalette::Button, QColor(QStringLiteral("#1B2533")));
	wizardPalette.setColor(QPalette::ButtonText, QColor(QStringLiteral("#F4F7FB")));
	wizardPalette.setColor(QPalette::Highlight, QColor(QStringLiteral("#7C3AED")));
	wizard->setPalette(wizardPalette);
	wizard->setAutoFillBackground(true);

	auto addCopy = [](QVBoxLayout *layout, const QString &copy, QWidget *parent,
			  const QString &role = QStringLiteral("muted")) {
		auto *label = new QLabel(copy, parent);
		label->setWordWrap(true);
		label->setProperty("textRole", role);
		layout->addWidget(label);
		return label;
	};

	// Step 1: explain the Free-first workflow and basic startup behavior.
	auto *welcomePage = new QWizardPage(wizard);
	welcomePage->setObjectName(QStringLiteral("initialSetupWelcomePage"));
	welcomePage->setTitle(text(strings::kInitialSetupWelcomeTitle));
	welcomePage->setSubTitle(text(strings::kInitialSetupWelcomeBody));
	auto *welcomeLayout = new QVBoxLayout(welcomePage);
	welcomeLayout->setSpacing(tokens::kSpaceMd);
	addCopy(welcomeLayout, text(strings::kInitialSetupWelcomeFree), welcomePage, QStringLiteral("info"));
	auto *openDock = new QCheckBox(text(strings::kInitialSetupOpenDock), welcomePage);
	openDock->setObjectName(QStringLiteral("initialSetupOpenDock"));
	openDock->setChecked(stored.openDockAtStartup);
	auto *notifications = new QCheckBox(text(strings::kInitialSetupNotifications), welcomePage);
	notifications->setObjectName(QStringLiteral("initialSetupNotifications"));
	notifications->setChecked(stored.notificationsEnabled);
	welcomeLayout->addWidget(openDock);
	welcomeLayout->addWidget(notifications);
	welcomeLayout->addStretch(1);
	wizard->addPage(welcomePage);

	// Step 2: default clip behavior. Every new profile starts with the basic
	// vertical export; Pro additionally unlocks the dual-output option.
	auto *capturePage = new QWizardPage(wizard);
	capturePage->setObjectName(QStringLiteral("initialSetupCapturePage"));
	capturePage->setTitle(text(strings::kInitialSetupCaptureTitle));
	capturePage->setSubTitle(text(strings::kInitialSetupCaptureBody));
	auto *captureLayout = new QFormLayout(capturePage);
	captureLayout->setSpacing(tokens::kSpaceMd);
	auto *outputMode = new QComboBox(capturePage);
	outputMode->setObjectName(QStringLiteral("initialSetupOutputMode"));
	outputMode->addItem(text(strings::kSettingsVertical), static_cast<int>(CaptureOutputMode::Vertical));
	outputMode->addItem(text(strings::kSettingsHorizontal), static_cast<int>(CaptureOutputMode::Horizontal));
	if (licenseManager_ != nullptr && licenseManager_->snapshot().proEnabled()) {
		outputMode->addItem(text(strings::kSettingsBoth), static_cast<int>(CaptureOutputMode::Both));
	}
	const auto outputIndex = outputMode->findData(static_cast<int>(stored.outputMode));
	outputMode->setCurrentIndex(outputIndex < 0 ? 0 : outputIndex);
	auto *duration = new QComboBox(capturePage);
	duration->setObjectName(QStringLiteral("initialSetupDuration"));
	for (const int seconds : {15, 30, 60, 120})
		duration->addItem(seconds < 60 ? QStringLiteral("%1 s").arg(seconds)
					       : QStringLiteral("%1 min").arg(seconds / 60),
				  seconds);
	const auto durationIndex = duration->findData(stored.defaultDurationSeconds);
	duration->setCurrentIndex(durationIndex < 0 ? 1 : durationIndex);
	auto *folderRow = new QWidget(capturePage);
	auto *folderLayout = new QHBoxLayout(folderRow);
	folderLayout->setContentsMargins(0, 0, 0, 0);
	folderLayout->setSpacing(tokens::kSpaceSm);
	auto *clipFolder = new QLineEdit(QString::fromStdString(stored.clipDirectory.u8string()), folderRow);
	clipFolder->setObjectName(QStringLiteral("initialSetupClipFolder"));
	auto *browse = new QPushButton(text(strings::kInitialSetupBrowse), folderRow);
	browse->setObjectName(QStringLiteral("initialSetupBrowseFolder"));
	folderLayout->addWidget(clipFolder, 1);
	folderLayout->addWidget(browse);
	connect(browse, &QPushButton::clicked, wizard, [wizard, clipFolder, this] {
		const auto selected = QFileDialog::getExistingDirectory(wizard, text(strings::kInitialSetupClipFolder),
									clipFolder->text());
		if (!selected.isEmpty())
			clipFolder->setText(selected);
	});
	captureLayout->addRow(text(strings::kInitialSetupOutputMode), outputMode);
	captureLayout->addRow(text(strings::kInitialSetupDuration), duration);
	captureLayout->addRow(text(strings::kInitialSetupClipFolder), folderRow);
	wizard->addPage(capturePage);

	// Step 3: local voice recognition.
	auto *voicePage = new QWizardPage(wizard);
	voicePage->setObjectName(QStringLiteral("initialSetupVoicePage"));
	voicePage->setTitle(text(strings::kInitialSetupVoiceTitle));
	voicePage->setSubTitle(text(strings::kInitialSetupVoiceBody));
	auto *voiceLayout = new QFormLayout(voicePage);
	voiceLayout->setSpacing(tokens::kSpaceMd);
	auto *voiceEnabled = new QCheckBox(text(strings::kInitialSetupVoiceEnabled), voicePage);
	voiceEnabled->setObjectName(QStringLiteral("initialSetupVoiceEnabled"));
	voiceEnabled->setChecked(stored.triggerVoiceEnabled);
	auto *microphoneCombo = new QComboBox(voicePage);
	microphoneCombo->setObjectName(QStringLiteral("initialSetupMicrophone"));
	microphoneCombo->addItem(text(strings::kInitialSetupAutomaticMicrophone), QString{});
	if (voiceController_ != nullptr)
		for (const auto &source : voiceController_->availableAudioSources())
			microphoneCombo->addItem(QString::fromStdString(source), QString::fromStdString(source));
	const auto microphoneIndex = microphoneCombo->findData(QString::fromStdString(stored.voiceAudioSourceName));
	if (microphoneIndex >= 0)
		microphoneCombo->setCurrentIndex(microphoneIndex);
	auto *voiceLanguage = new QComboBox(voicePage);
	voiceLanguage->setObjectName(QStringLiteral("initialSetupVoiceLanguage"));
	voiceLanguage->addItem(text(strings::kSettingsAiAuto), QStringLiteral("auto"));
	voiceLanguage->addItem(text(strings::kSettingsAiSpanish), QStringLiteral("es"));
	voiceLanguage->addItem(text(strings::kSettingsAiEnglish), QStringLiteral("en"));
	const auto voiceLanguageIndex = voiceLanguage->findData(QString::fromStdString(stored.voiceTriggerLanguage));
	voiceLanguage->setCurrentIndex(voiceLanguageIndex < 0 ? 0 : voiceLanguageIndex);
	auto *voicePhrases = new QPlainTextEdit(QString::fromStdString(stored.voiceTriggerPhrasesCsv), voicePage);
	voicePhrases->setObjectName(QStringLiteral("initialSetupVoicePhrases"));
	voicePhrases->setMaximumHeight(100);
	voiceLayout->addRow(QString{}, voiceEnabled);
	voiceLayout->addRow(text(strings::kInitialSetupMicrophone), microphoneCombo);
	voiceLayout->addRow(text(strings::kInitialSetupVoiceLanguage), voiceLanguage);
	voiceLayout->addRow(text(strings::kInitialSetupVoicePhrases), voicePhrases);
	wizard->addPage(voicePage);

	// Step 4: the OBS scene used by the vertical compositor.
	auto *verticalPage = new QWizardPage(wizard);
	verticalPage->setObjectName(QStringLiteral("initialSetupVerticalPage"));
	verticalPage->setTitle(text(strings::kInitialSetupVerticalTitle));
	verticalPage->setSubTitle(text(strings::kInitialSetupVerticalBody));
	auto *verticalLayout = new QFormLayout(verticalPage);
	verticalLayout->setSpacing(tokens::kSpaceMd);
	auto *sceneCombo = new QComboBox(verticalPage);
	sceneCombo->setObjectName(QStringLiteral("initialSetupVerticalScene"));
	for (const auto &scene : scenes)
		if (!scene.empty())
			sceneCombo->addItem(QString::fromStdString(scene), QString::fromStdString(scene));
	const auto desiredScene =
		QString::fromStdString(stored.verticalSceneName.empty() ? managedScene : stored.verticalSceneName);
	const auto sceneIndex = sceneCombo->findData(desiredScene);
	if (sceneIndex >= 0)
		sceneCombo->setCurrentIndex(sceneIndex);
	auto *verticalResolution = new QComboBox(verticalPage);
	verticalResolution->setObjectName(QStringLiteral("initialSetupVerticalResolution"));
	verticalResolution->addItem(QStringLiteral("720p · 720 × 1280"),
				    static_cast<int>(VerticalResolution::Portrait720));
	verticalResolution->addItem(QStringLiteral("1080p · 1080 × 1920"),
				    static_cast<int>(VerticalResolution::Portrait1080));
	verticalResolution->addItem(QStringLiteral("2K · 1440 × 2560"),
				    static_cast<int>(VerticalResolution::Portrait1440));
	verticalResolution->addItem(QStringLiteral("4K · 2160 × 3840"),
				    static_cast<int>(VerticalResolution::Portrait2160));
	const auto resolutionIndex = verticalResolution->findData(static_cast<int>(stored.verticalResolution));
	verticalResolution->setCurrentIndex(resolutionIndex < 0 ? 1 : resolutionIndex);
	verticalLayout->addRow(text(strings::kInitialSetupVerticalScene), sceneCombo);
	verticalLayout->addRow(text(strings::kInitialSetupVerticalResolution), verticalResolution);
	wizard->addPage(verticalPage);

	// Step 5: finish Free setup and optionally activate or purchase Pro.
	auto *readyPage = new FinalSetupPage(wizard);
	readyPage->setObjectName(QStringLiteral("initialSetupReadyPage"));
	readyPage->setTitle(text(strings::kInitialSetupReadyTitle));
	readyPage->setSubTitle(text(strings::kInitialSetupReadyBody));
	auto *readyLayout = new QVBoxLayout(readyPage);
	readyLayout->setSpacing(tokens::kSpaceMd);
	auto *proTitle = new QLabel(text(strings::kInitialSetupProTitle), readyPage);
	proTitle->setProperty("textRole", QStringLiteral("sectionTitle"));
	readyLayout->addWidget(proTitle);
	addCopy(readyLayout, text(strings::kInitialSetupProBody), readyPage);
	auto *licenseRow = new QWidget(readyPage);
	auto *licenseLayout = new QHBoxLayout(licenseRow);
	licenseLayout->setContentsMargins(0, 0, 0, 0);
	licenseLayout->setSpacing(tokens::kSpaceSm);
	auto *licenseKey = new QLineEdit(licenseRow);
	licenseKey->setObjectName(QStringLiteral("initialSetupLicenseKey"));
	licenseKey->setPlaceholderText(text(strings::kInitialSetupLicensePlaceholder));
	licenseKey->setEchoMode(QLineEdit::Password);
	licenseKey->setMaxLength(64);
	auto *activate = new QPushButton(text(strings::kInitialSetupActivate), licenseRow);
	activate->setObjectName(QStringLiteral("initialSetupActivateLicense"));
	activate->setProperty("controlRole", QStringLiteral("primary"));
	licenseLayout->addWidget(licenseKey, 1);
	licenseLayout->addWidget(activate);
	auto *licenseStatus = new QLabel(readyPage);
	licenseStatus->setObjectName(QStringLiteral("initialSetupLicenseStatus"));
	licenseStatus->setWordWrap(true);
	licenseStatus->hide();
	readyLayout->addWidget(licenseRow);
	readyLayout->addWidget(licenseStatus);
	auto *upgrade = new QPushButton(text(strings::kInitialSetupUpgrade), readyPage);
	upgrade->setObjectName(QStringLiteral("initialSetupUpgrade"));
	upgrade->setProperty("controlRole", QStringLiteral("secondary"));
	readyLayout->addWidget(upgrade);
	readyLayout->addStretch(1);
	const bool alreadyPro = licenseManager_ != nullptr && licenseManager_->snapshot().proEnabled();
	licenseRow->setVisible(!alreadyPro);
	if (alreadyPro) {
		licenseStatus->setText(text(strings::kInitialSetupActivated));
		licenseStatus->setProperty("notificationTone", QStringLiteral("success"));
		licenseStatus->show();
	}
	connect(upgrade, &QPushButton::clicked, wizard, [] {
		QUrl url(QStringLiteral(CLIPX_SERVICE_BASE_URL));
		url.setPath(QStringLiteral("/"));
		url.setFragment(QStringLiteral("pricing"));
		QDesktopServices::openUrl(url);
	});
	connect(activate, &QPushButton::clicked, wizard, [this, wizard, licenseKey, activate, licenseStatus, licenseRow] {
		if (licenseManager_ == nullptr)
			return;
		const auto key = licenseKey->text().trimmed();
		static const QRegularExpression pattern(QStringLiteral("^CCS1-(?:[A-Fa-f0-9]{5}-){3}[A-Fa-f0-9]{5}$"));
		if (!pattern.match(key).hasMatch()) {
			licenseStatus->setText(text(strings::kSettingsLicenseKeyInvalid));
			licenseStatus->setProperty("notificationTone", QStringLiteral("error"));
			licenseStatus->show();
			return;
		}
		activate->setEnabled(false);
		activate->setText(text(strings::kInitialSetupActivating));
		licenseStatus->hide();
		QPointer<QWizard> guardedWizard(wizard);
		licenseManager_->activate(
			key.toUpper().toStdString(), [guardedWizard, licenseStatus, activate, licenseRow,
						      this](const licensing::LicenseSnapshot &snapshot) {
				if (guardedWizard.isNull())
					return;
				activate->setText(text(strings::kInitialSetupActivate));
				activate->setEnabled(true);
				const bool active = snapshot.proEnabled() && snapshot.lastErrorCode.empty();
				licenseStatus->setText(active ? text(strings::kInitialSetupActivated)
							      : QString::fromStdString(snapshot.lastErrorMessage));
				licenseStatus->setProperty("notificationTone", active ? QStringLiteral("success")
										      : QStringLiteral("error"));
				licenseStatus->show();
				licenseRow->setVisible(!active);
			});
		licenseKey->clear();
	});
	wizard->addPage(readyPage);
	for (const auto role : {QWizard::NextButton, QWizard::FinishButton})
		wizard->button(role)->setProperty("controlRole", QStringLiteral("primary"));
	for (const auto role : {QWizard::BackButton, QWizard::CancelButton})
		wizard->button(role)->setProperty("controlRole", QStringLiteral("secondary"));

	readyPage->setValidator([this, wizard, openDock, notifications, outputMode, duration, clipFolder, voiceEnabled,
				 microphoneCombo, voiceLanguage, voicePhrases, sceneCombo, verticalResolution] {
		auto settings = settingsManager_->settings();
		settings.initialSetupCompleted = true;
		settings.initialSetupCompletedVersion = CLIPCOACH_VERSION;
		settings.openDockAtStartup = openDock->isChecked();
		settings.notificationsEnabled = notifications->isChecked();
		settings.autoStartReplayBuffer = false;
		settings.outputMode = static_cast<CaptureOutputMode>(outputMode->currentData().toInt());
		settings.defaultDurationSeconds = duration->currentData().toInt();
		if (!clipFolder->text().trimmed().isEmpty())
			settings.clipDirectory =
				std::filesystem::u8path(clipFolder->text().trimmed().toUtf8().constData());
		settings.triggerVoiceEnabled = voiceEnabled->isChecked();
		if (settings.triggerVoiceEnabled)
			settings.triggerAction = TriggerAction::SaveClip;
		settings.voiceAudioSourceName = microphoneCombo->currentData().toString().toStdString();
		settings.voiceTriggerLanguage = voiceLanguage->currentData().toString().toStdString();
		settings.voiceTriggerPhrasesCsv = voicePhrases->toPlainText().trimmed().toStdString();
		settings.verticalSceneName = sceneCombo->currentData().toString().toStdString();
		settings.verticalSourceName.clear();
		settings.verticalResolution =
			static_cast<VerticalResolution>(verticalResolution->currentData().toInt());
		const auto dimensions = verticalResolutionDimensions(settings.verticalResolution);
		settings.verticalWidth = dimensions.width;
		settings.verticalHeight = dimensions.height;
		std::string error;
		if (!settingsManager_->save(settings, &error)) {
			QMessageBox::warning(wizard, text(strings::kInitialSetupTitle), QString::fromStdString(error));
			return false;
		}
		if (voiceController_ != nullptr)
			voiceController_->applySettings(settingsManager_->settings());
		if (hotkeySettingsChangedCallback_)
			hotkeySettingsChangedCallback_(settingsManager_->settings());
		if (verticalTab_ != nullptr)
			verticalTab_->refreshObsSceneOptions();
		if (triggersTab_ != nullptr)
			triggersTab_->refreshConfiguration();
		refreshCaptureOutputMode();
		updateFeatureGateUi();
		return true;
	});
	wizard->open();
}

void MainDock::setVerticalPreviewOnly(bool enabled)
{
	if (dockHeader_ != nullptr)
		dockHeader_->setVisible(!enabled);
	if (statusContainer_ != nullptr)
		statusContainer_->setVisible(!enabled);
	if (mainTabBar_ != nullptr)
		mainTabBar_->setVisible(!enabled);
	updateFooterResponsivePresentation();
}

QWidget *MainDock::createTriggersPage()
{
	triggersTab_ = new TriggersTab(translator_, triggerEngine_, settingsManager_, nullptr, voiceController_,
				       sceneProvider_);
	triggersTab_->setTimingChangedCallback([this](int seconds) {
		if (replayDurationChangedCallback_)
			replayDurationChangedCallback_(seconds);
	});
	return triggersTab_;
}

QWidget *MainDock::createClipsPage()
{
	clipsTab_ = new ClipsTab(translator_, libraryService_, QString::fromStdString(sessionId_), nullptr, nullptr,
				 exportManager_, settingsManager_, verticalManager_, featureGates_, captionGenerator_);
	clipsTab_->setCaptionBusyCallback(
		[this](bool busy, const CaptionGenerationProgress &progress) { setCaptionProcessing(busy, progress); });
	return clipsTab_;
}

QWidget *MainDock::createSettingsPage()
{
	settingsTab_ = new SettingsTab(translator_, settingsManager_, nullptr, chatManager_, licenseManager_,
				       verticalObsBridge_);
	settingsTab_->setAppliedCallback([this](const Settings &before, const Settings &after) {
		if (before.remoteCommandsEnabled != after.remoteCommandsEnabled && remoteCommandsChangedCallback_)
			remoteCommandsChangedCallback_(after.remoteCommandsEnabled);
		if (before.language != after.language) {
			showFooterStatus(text(strings::kSettingsLanguageRestart), false, true);
			if (languageChangedCallback_)
				languageChangedCallback_();
		}
		if (before.defaultDurationSeconds != after.defaultDurationSeconds) {
			selectDuration(after.defaultDurationSeconds);
		}
		if (!before.autoStartReplayBuffer && after.autoStartReplayBuffer && clipManager_ != nullptr) {
			(void)clipManager_->startReplayBuffer();
		}
		if (before.verticalResolution != after.verticalResolution ||
		    before.verticalWidth != after.verticalWidth || before.verticalHeight != after.verticalHeight) {
			verticalStatusCard_->setValue(
				QStringLiteral("%1×%2").arg(after.verticalWidth).arg(after.verticalHeight));
			if (verticalTab_ != nullptr) {
				verticalTab_->refresh();
			}
		}
		if ((before.markMomentHotkey != after.markMomentHotkey || before.save15Hotkey != after.save15Hotkey ||
		     before.save30Hotkey != after.save30Hotkey || before.save60Hotkey != after.save60Hotkey ||
		     before.saveVerticalHotkey != after.saveVerticalHotkey ||
		     before.openVerticalDesignerHotkey != after.openVerticalDesignerHotkey) &&
		    hotkeySettingsChangedCallback_) {
			hotkeySettingsChangedCallback_(after);
		}
	});
	settingsTab_->setRemoteAuthenticationRequestedCallback([this] { beginRemoteAuthentication(); });
	settingsTab_->setRemoteClipperOpenRequestedCallback([this] { openRemoteClipperAccount(); });
	settingsTab_->setProfileImportedCallback([this] {
		if (languageChangedCallback_)
			languageChangedCallback_();
	});
	settingsTab_->setSetupRequestedCallback([this] { showInitialSetupIfNeeded(true); });
	return settingsTab_;
}

void MainDock::bindCaptureFlow()
{
	for (auto *button : durationButtons_) {
		connect(button, &QPushButton::clicked, this,
			[this, button] { selectDuration(button->property("durationSeconds").toInt()); });
	}

	const auto defaults = settingsManager_ != nullptr ? settingsManager_->settings().quickDurationsSeconds
							  : Settings::defaults().quickDurationsSeconds;
	if (!defaults.empty()) {
		selectDuration(settingsManager_ != nullptr ? settingsManager_->settings().defaultDurationSeconds
							   : settings_constraints::kDefaultClipDurationSeconds);
	}

	if (clipManager_ == nullptr) {
		startReplayButton_->setEnabled(false);
		stopReplayButton_->setEnabled(false);
		markMomentButton_->setEnabled(false);
		footerReplayButton_->setEnabled(false);
		footerClipButton_->setEnabled(false);
		updateReplayState(ReplayState::Inactive);
		return;
	}

	connect(startReplayButton_, &QPushButton::clicked, this, [this] { (void)clipManager_->startReplayBuffer(); });
	connect(stopReplayButton_, &QPushButton::clicked, this, [this] { (void)clipManager_->stopReplayBuffer(); });
	connect(markMomentButton_, &QPushButton::clicked, this, [this] { captureSelectedClip(); });
	connect(saveVerticalButton_, &QPushButton::clicked, this, [this] { captureVerticalClip(); });
	connect(captureShortcutMarkButton_, &QPushButton::clicked, this, [this] { captureSelectedClip(); });
	connect(captureShortcutSaveButton_, &QPushButton::clicked, this, [this] { captureSelectedClip(); });
	connect(captureShortcutVerticalButton_, &QPushButton::clicked, this, [this] { captureVerticalClip(); });
	connect(captureShortcutStopButton_, &QPushButton::clicked, this,
		[this] { (void)clipManager_->stopReplayBuffer(); });
	connect(footerReplayButton_, &QPushButton::clicked, this, [this] { toggleReplayBuffer(); });
	connect(footerClipButton_, &QPushButton::clicked, this, [this] { captureSelectedClip(); });

	clipManager_->setReplayStateCallback([this](ReplayState state) {
		QMetaObject::invokeMethod(this, [this, state] { updateReplayState(state); }, Qt::QueuedConnection);
	});
	clipManager_->setClipSavedCallback([this](const ClipMetadata &clip) {
		const auto copy = clip;
		QMetaObject::invokeMethod(this, [this, copy] { handleClipSaved(copy); }, Qt::QueuedConnection);
	});
	clipManager_->setErrorCallback([this](CaptureError error, const std::string &) {
		QMetaObject::invokeMethod(this, [this, error] { handleCaptureError(error); }, Qt::QueuedConnection);
	});

	updateReplayState(clipManager_->replayState());
	if (settingsManager_ != nullptr && settingsManager_->settings().autoStartReplayBuffer &&
	    clipManager_->replayState() == ReplayState::Inactive) {
		(void)clipManager_->startReplayBuffer();
	}
}

void MainDock::updateReplayState(ReplayState state)
{
	QString stateText;
	StatusTone tone = StatusTone::Neutral;
	switch (state) {
	case ReplayState::Active:
		stateText = text(strings::kStateActive);
		tone = StatusTone::Active;
		break;
	case ReplayState::Starting:
		stateText = text(strings::kStateStarting);
		tone = StatusTone::Pending;
		break;
	case ReplayState::Stopping:
		stateText = text(strings::kStateStopping);
		tone = StatusTone::Pending;
		break;
	case ReplayState::Error:
		stateText = text(strings::kStateError);
		tone = StatusTone::Error;
		break;
	case ReplayState::Inactive:
		stateText = text(strings::kStateInactive);
		break;
	}

	replayStatusCard_->setValue(stateText);
	replayStatusCard_->setTone(tone);
	if (headerStatus_ != nullptr) {
		const bool replayActive = state == ReplayState::Active;
		headerStatus_->setText(
			text(replayActive ? strings::kHeaderBufferActive : strings::kHeaderBufferInactive));
		headerStatus_->setToolTip(stateText);
		headerStatus_->setProperty("notificationTone",
					   replayActive ? QStringLiteral("success") : QStringLiteral("error"));
		headerStatus_->style()->unpolish(headerStatus_);
		headerStatus_->style()->polish(headerStatus_);
	}
	if (captureReplayStateLabel_ != nullptr) {
		captureReplayStateLabel_->setText(stateText);
		captureReplayStateLabel_->setProperty("replayActive", state == ReplayState::Active);
		captureReplayStateLabel_->style()->unpolish(captureReplayStateLabel_);
		captureReplayStateLabel_->style()->polish(captureReplayStateLabel_);
	}
	if (captureReplayStateDescription_ != nullptr) {
		captureReplayStateDescription_->setText(state == ReplayState::Active
								? text(strings::kCaptureBufferActiveDescription)
								: text(strings::kCaptureBufferInactiveDescription));
	}

	const bool active = state == ReplayState::Active;
	const bool transition = state == ReplayState::Starting || state == ReplayState::Stopping;
	if (!active && captureNotificationPending_) {
		++captureNotificationGeneration_;
		captureNotificationPending_ = false;
		setCapturePendingIndicator(false);
		if (captureNotification_ != nullptr)
			captureNotification_->hide();
	}
	if (verticalTab_ != nullptr)
		verticalTab_->setReplayState(active, transition);
	startReplayButton_->setEnabled(clipManager_ != nullptr && !active && !transition);
	stopReplayButton_->setEnabled(clipManager_ != nullptr && (active || state == ReplayState::Error));
	markMomentButton_->setEnabled(clipManager_ != nullptr && active && !clipManager_->capturePending());
	const bool verticalAllowed = featureGates_ != nullptr &&
				     featureGates_->isAllowed(Feature::LimitedVerticalExport);
	saveVerticalButton_->setEnabled(clipManager_ != nullptr && active && !clipManager_->capturePending() &&
					verticalAllowed);
	if (captureShortcutMarkButton_ != nullptr)
		captureShortcutMarkButton_->setEnabled(clipManager_ != nullptr && active &&
						       !clipManager_->capturePending());
	if (captureShortcutSaveButton_ != nullptr)
		captureShortcutSaveButton_->setEnabled(clipManager_ != nullptr && active &&
						       !clipManager_->capturePending());
	if (captureShortcutVerticalButton_ != nullptr)
		captureShortcutVerticalButton_->setEnabled(clipManager_ != nullptr && active &&
							   !clipManager_->capturePending() && verticalAllowed);
	if (captureShortcutStopButton_ != nullptr)
		captureShortcutStopButton_->setEnabled(clipManager_ != nullptr &&
						       (active || state == ReplayState::Error));
	footerReplayButton_->setEnabled(clipManager_ != nullptr && !transition);
	footerReplayButton_->setIcon(style()->standardIcon(active ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
	setFooterExpandedText(footerReplayButton_,
			      active ? text(strings::kActionStopReplay) : text(strings::kActionStartReplay));
	footerClipButton_->setEnabled(clipManager_ != nullptr && active && !clipManager_->capturePending());
}

void MainDock::handleClipSaved(const ClipMetadata &clip)
{
	setCapturePendingIndicator(false);
	const auto clipName = QString::fromStdString(clip.fileName);
	lastClipValueLabel_->setText(clipName);
	const auto count = clipManager_ != nullptr ? clipManager_->sessionClips().size() : 0;
	sessionClipCountLabel_->setText(text(strings::kCaptureSessionCount).arg(static_cast<qulonglong>(count)));

	const auto metadata = text(strings::kCaptureManualMetadata).arg(clip.durationSeconds);
	if (lastClipMetadataLabel_ != nullptr)
		lastClipMetadataLabel_->setText(metadata);
	if (lastClipThumbnailLabel_ != nullptr) {
		const QString thumbnailPath = QString::fromStdString(clip.thumbnailPath.string());
		QPixmap thumbnail(thumbnailPath);
		if (!thumbnail.isNull()) {
			lastClipThumbnailLabel_->setPixmap(thumbnail.scaled(lastClipThumbnailLabel_->size(),
									    Qt::KeepAspectRatioByExpanding,
									    Qt::SmoothTransformation));
			lastClipThumbnailLabel_->setText(QString{});
		} else {
			lastClipThumbnailLabel_->setText(text(strings::kCaptureNoPreview));
		}
	}
	QString completion = text(strings::kCaptureSaved).arg(clipName);
	bool completionSucceeded = true;
	if (pendingCaptureExport_.has_value()) {
		const auto output = *pendingCaptureExport_;
		pendingCaptureExport_.reset();
		const auto result = verticalCaptureActionService_ != nullptr
					    ? verticalCaptureActionService_->requestExport(clip, output)
					    : ClipActionResult::fail("vertical export service is unavailable");
		if (result.success) {
			completion = text(output == ExportOrientation::Both ? strings::kCaptureBothQueued
									    : strings::kCaptureVerticalQueued)
					     .arg(clipName);
		} else {
			completionSucceeded = false;
			showCaptureNotification(
				text(strings::kCaptureVerticalFailed).arg(QString::fromStdString(result.error)), true,
				true);
			showFooterStatus(
				text(strings::kCaptureVerticalFailed).arg(QString::fromStdString(result.error)), true);
		}
	}
	if (completionSucceeded) {
		QString notification = completion;
		if (settingsManager_ != nullptr) {
			const auto &settings = settingsManager_->settings();
			if (settings.notificationDetail == NotificationDetail::Compact) {
				notification = text(strings::kNotificationClipSavedCompact);
			} else if (settings.notificationDetail == NotificationDetail::Detailed) {
				notification =
					text(strings::kNotificationClipSavedDetailed)
						.arg(clipName)
						.arg(clip.durationSeconds)
						.arg(QString::fromStdString(clip.filePath.parent_path().string()));
			}
			// Save feedback stays inside the OBS dock. Native Windows tray
			// notifications and QApplication::beep are intentionally avoided
			// because their sound cannot be made reliably silent across systems.
		}
		showCaptureNotification(notification, false, true);
		showFooterStatus(completion, false, true);
	}
	updateReplayState(clipManager_ != nullptr ? clipManager_->replayState() : ReplayState::Inactive);
	if (clipsTab_ != nullptr) {
		clipsTab_->reload();
	}
}

void MainDock::handleCaptureError(CaptureError error)
{
	setCapturePendingIndicator(false);
	pendingCaptureExport_.reset();
	const char *key = strings::kErrorCaptureFailed;
	switch (error) {
	case CaptureError::ReplayBufferInactive:
		key = strings::kErrorReplayInactive;
		break;
	case CaptureError::ReplayBufferWarmingUp:
		key = strings::kErrorReplayWarmingUp;
		break;
	case CaptureError::CaptureAlreadyPending:
		key = strings::kErrorCapturePending;
		break;
	case CaptureError::SavedFileUnavailable:
		key = strings::kErrorSavedFileUnavailable;
		break;
	case CaptureError::FileRenameFailed:
		key = strings::kErrorRenameFailed;
		break;
	case CaptureError::None:
		return;
	case CaptureError::InvalidDuration:
	case CaptureError::SaveRejected:
		break;
	case CaptureError::ProRequired:
		key = strings::kErrorProRequired;
		break;
	}
	showCaptureNotification(text(key), true, true);
	showFooterStatus(text(key), true);
	updateReplayState(clipManager_ != nullptr ? clipManager_->replayState() : ReplayState::Inactive);
}

void MainDock::updateFeatureGateUi()
{
	const bool unlimited = featureGates_ != nullptr && featureGates_->isAllowed(Feature::UnlimitedDurations);
	for (auto *button : durationButtons_) {
		const auto seconds = button->property("durationSeconds").toInt();
		const bool freeDuration = PlanPolicy::isFreeQuickDuration(seconds);
		button->setEnabled(freeDuration || unlimited);
		if (!freeDuration)
			button->setToolTip(unlimited ? QString{} : text(strings::kErrorProRequired));
	}
	if (captureUpgradeBanner_ != nullptr)
		captureUpgradeBanner_->setVisible(!unlimited);
	if (durationProBadge_ != nullptr)
		durationProBadge_->setVisible(!unlimited);
	const bool verticalAllowed = featureGates_ != nullptr &&
				     featureGates_->isAllowed(Feature::LimitedVerticalExport);
	if (saveVerticalButton_ != nullptr) {
		const bool replayReady = clipManager_ != nullptr &&
					 clipManager_->replayState() == ReplayState::Active &&
					 !clipManager_->capturePending();
		saveVerticalButton_->setEnabled(verticalAllowed && replayReady);
		saveVerticalButton_->setToolTip(verticalAllowed ? QString{} : text(strings::kCaptureVerticalLimited));
	}
	if (verticalLimitedLabel_ != nullptr)
		verticalLimitedLabel_->setVisible(!verticalAllowed);
	if (clipsTab_ != nullptr)
		clipsTab_->refreshFeatureGates();
}

void MainDock::selectDuration(int seconds)
{
	selectedDurationSeconds_ = seconds;
	for (auto *button : durationButtons_) {
		button->setChecked(button->property("durationSeconds").toInt() == seconds);
	}
}

void MainDock::refreshCaptureOutputMode()
{
	const auto mode = verticalManager_ != nullptr   ? verticalManager_->settings().outputMode
			  : settingsManager_ != nullptr ? settingsManager_->settings().outputMode
							: CaptureOutputMode::Horizontal;
	if (captureOutputModeCombo_ != nullptr) {
		const QSignalBlocker blocker(captureOutputModeCombo_);
		const auto index = captureOutputModeCombo_->findData(static_cast<int>(mode));
		captureOutputModeCombo_->setCurrentIndex(index >= 0 ? index : 0);
		captureOutputModeCombo_->setEnabled(verticalManager_ != nullptr);
	}
	const char *description = strings::kCaptureOutputHorizontalDescription;
	const char *saveLabel = strings::kCaptureSaveHorizontal;
	if (mode == CaptureOutputMode::Vertical) {
		description = strings::kCaptureOutputVerticalDescription;
		saveLabel = strings::kCaptureSaveVertical;
	} else if (mode == CaptureOutputMode::Both) {
		description = strings::kCaptureOutputBothDescription;
		saveLabel = strings::kCaptureSaveBoth;
	}
	if (captureOutputModeDescription_ != nullptr)
		captureOutputModeDescription_->setText(text(strings::kCaptureOutputDescription) + QStringLiteral("\n") +
						       text(description));
	if (markMomentButton_ != nullptr)
		markMomentButton_->setText(text(saveLabel));
	if (captureShortcutSaveButton_ != nullptr)
		captureShortcutSaveButton_->setText(text(saveLabel));
	if (footerClipButton_ != nullptr)
		footerClipButton_->setToolTip(text(description));
}

void MainDock::showCaptureNotification(const QString &message, bool error, bool autoDismiss)
{
	if (captureNotification_ == nullptr)
		return;
	const auto generation = ++captureNotificationGeneration_;
	captureNotificationPending_ = !error && !autoDismiss;
	if (!error && settingsManager_ != nullptr && !settingsManager_->settings().notificationsEnabled) {
		captureNotificationPending_ = false;
		captureNotification_->hide();
		return;
	}
	captureNotification_->setText(autoDismiss ? message + QStringLiteral("  ✓") : message);
	captureNotification_->setToolTip(message);
	captureNotification_->setProperty("notificationTone",
					  error ? QStringLiteral("error") : QStringLiteral("success"));
	captureNotification_->style()->unpolish(captureNotification_);
	captureNotification_->style()->polish(captureNotification_);
	captureNotification_->show();
	if (autoDismiss) {
		QTimer::singleShot(kTransientNotificationMs, this, [this, generation] {
			if (generation == captureNotificationGeneration_) {
				captureNotificationPending_ = false;
				captureNotification_->hide();
			}
		});
	}
}

void MainDock::toggleReplayBuffer()
{
	if (clipManager_ == nullptr)
		return;
	if (clipManager_->replayState() == ReplayState::Active)
		(void)clipManager_->stopReplayBuffer();
	else
		(void)clipManager_->startReplayBuffer();
}

void MainDock::captureSelectedClip()
{
	if (clipManager_ == nullptr)
		return;
	const auto mode = verticalManager_ != nullptr ? verticalManager_->settings().outputMode
						      : CaptureOutputMode::Horizontal;
	const auto output = mode == CaptureOutputMode::Vertical ? ExportOrientation::Vertical
			    : mode == CaptureOutputMode::Both   ? ExportOrientation::Both
								: ExportOrientation::Horizontal;
	const auto result = requestTriggeredCapture(selectedDurationSeconds_, TriggerType::Manual, "manual", 0, output);
	if (!result.accepted)
		return;
	if (triggerEngine_ != nullptr) {
		TriggerSignal signal;
		signal.type = SmartTriggerType::Manual;
		signal.manualMarker = true;
		signal.durationSeconds = selectedDurationSeconds_;
		(void)triggerEngine_->process(signal);
	}
	markMomentButton_->setEnabled(false);
	footerClipButton_->setEnabled(false);
	showCapturePending();
}

void MainDock::captureVerticalClip()
{
	if (clipManager_ == nullptr)
		return;
	const auto gate = featureGates_ != nullptr ? featureGates_->check(Feature::LimitedVerticalExport)
						   : FeatureGateDecision{false, "PRO_REQUIRED",
									 "Vertical Canvas requires ClipXtudio Pro"};
	if (!gate.allowed) {
		handleCaptureError(CaptureError::ProRequired);
		return;
	}
	const auto result = requestTriggeredCapture(selectedDurationSeconds_, TriggerType::Manual, "manual", 0,
						    ExportOrientation::Vertical);
	if (!result.accepted)
		return;
	markMomentButton_->setEnabled(false);
	saveVerticalButton_->setEnabled(false);
	footerClipButton_->setEnabled(false);
}

CaptureResult MainDock::requestTriggeredCapture(int durationSeconds, TriggerType triggerType, std::string triggerLabel,
						int score, ExportOrientation output)
{
	if (clipManager_ == nullptr)
		return {false, CaptureError::SaveRejected, "capture service is unavailable"};
	if (output != ExportOrientation::Horizontal) {
		const auto requiredFeature = output == ExportOrientation::Both ? Feature::HorizontalAndVertical
									       : Feature::LimitedVerticalExport;
		const auto gate =
			featureGates_ != nullptr
				? featureGates_->check(requiredFeature)
				: FeatureGateDecision{false, "PRO_REQUIRED", "Vertical Canvas requires ClipXtudio Pro"};
		if (!gate.allowed) {
			handleCaptureError(CaptureError::ProRequired);
			return {false, CaptureError::ProRequired, gate.message};
		}
	}
	auto result =
		triggerType == TriggerType::Manual
			? clipManager_->captureManual(durationSeconds)
			: clipManager_->captureTriggered(durationSeconds, triggerType, std::move(triggerLabel), score);
	if (result.accepted) {
		pendingCaptureExport_ = output == ExportOrientation::Horizontal
						? std::nullopt
						: std::optional<ExportOrientation>{output};
		showCapturePending();
	}
	return result;
}

void MainDock::showCapturePending()
{
	if (clipManager_ == nullptr || clipManager_->replayState() != ReplayState::Active) {
		captureNotificationPending_ = false;
		setCapturePendingIndicator(false);
		if (captureNotification_ != nullptr)
			captureNotification_->hide();
		return;
	}
	setCapturePendingIndicator(true);
	if (markMomentButton_ != nullptr)
		markMomentButton_->setEnabled(false);
	if (saveVerticalButton_ != nullptr)
		saveVerticalButton_->setEnabled(false);
	if (footerClipButton_ != nullptr)
		footerClipButton_->setEnabled(false);
	showCaptureNotification(text(strings::kCaptureSaving), false);
}

void MainDock::setCapturePendingIndicator(bool pending)
{
	if (headerCapturePendingDot_ == nullptr || capturePendingPulseTimer_ == nullptr)
		return;
	headerCapturePendingDot_->setVisible(pending);
	if (pending) {
		capturePendingPulseOn_ = true;
		headerCapturePendingDot_->setProperty("pulseOn", true);
		capturePendingPulseTimer_->start();
	} else {
		capturePendingPulseTimer_->stop();
		capturePendingPulseOn_ = false;
		headerCapturePendingDot_->setProperty("pulseOn", false);
	}
	headerCapturePendingDot_->style()->unpolish(headerCapturePendingDot_);
	headerCapturePendingDot_->style()->polish(headerCapturePendingDot_);
}

void MainDock::showFooterStatus(const QString &message, bool error, bool autoReset)
{
	if (headerStatus_ == nullptr)
		return;
	const auto generation = ++headerStatusGeneration_;
	headerStatus_->setText(autoReset ? message + QStringLiteral("  ✓") : message);
	headerStatus_->setToolTip(message);
	// The header badge is reserved for the concise Replay Buffer state. Other
	// transient operations remain available as its tooltip and in their page
	// notification/dialog, so long messages cannot stretch the header.
	updateReplayState(clipManager_ != nullptr ? clipManager_->replayState() : ReplayState::Inactive);
	if (autoReset) {
		QTimer::singleShot(kTransientNotificationMs, this, [this, generation] {
			if (generation != headerStatusGeneration_)
				return;
			headerStatus_->setText(text(strings::kFooterReady));
			headerStatus_->setToolTip({});
			headerStatus_->setProperty("notificationTone", QStringLiteral("success"));
			headerStatus_->style()->unpolish(headerStatus_);
			headerStatus_->style()->polish(headerStatus_);
			updateReplayState(clipManager_ != nullptr ? clipManager_->replayState()
								  : ReplayState::Inactive);
		});
	}
	Q_UNUSED(error);
}

void MainDock::setUpdateIndicatorVisible(bool visible)
{
	if (updateAvailableIndicator_ == nullptr || updatePulseTimer_ == nullptr)
		return;
	updateAvailableIndicator_->setVisible(visible);
	if (visible) {
		updateAvailableIndicator_->setProperty("pulseOn", true);
		updatePulseTimer_->start();
	} else {
		updatePulseTimer_->stop();
	}
	updateAvailableIndicator_->style()->unpolish(updateAvailableIndicator_);
	updateAvailableIndicator_->style()->polish(updateAvailableIndicator_);
}

void MainDock::setUpdateChecking(bool checking)
{
	updateCheckInProgress_ = checking;
	if (footerUpdateButton_ == nullptr)
		return;
	footerUpdateButton_->setEnabled(!checking);
	setFooterExpandedText(footerUpdateButton_,
			      text(checking ? strings::kSettingsCheckingUpdates : strings::kSettingsCheckUpdates));
	if (checking) {
		updateSpinnerAngle_ = 0;
		if (updateSpinnerTimer_ != nullptr)
			updateSpinnerTimer_->start();
	} else {
		if (updateSpinnerTimer_ != nullptr)
			updateSpinnerTimer_->stop();
		footerUpdateButton_->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
	}
}

void MainDock::checkForUpdates(bool userInitiated)
{
	if (updateCheckInProgress_)
		return;
	if (updateChecker_ == nullptr || !updateChecker_->configured()) {
		if (userInitiated)
			showUpdateErrorDialog();
		return;
	}
	setUpdateChecking(true);
	setUpdateIndicatorVisible(false);
	QPointer<MainDock> self(this);
	updateChecker_->check(QStringLiteral(CLIPCOACH_VERSION),
			      [self, userInitiated](network::UpdateCheckResult result) {
				      if (self.isNull())
					      return;
				      self->setUpdateChecking(false);
				      self->updateDownloadUrl_.clear();
				      self->updateSha256_.clear();
				      self->updateVersion_.clear();
				      self->updateSizeBytes_ = 0;
				      if (!result.success) {
					      self->footerUpdateButton_->setToolTip(result.errorCode);
					      self->showFooterStatus(self->text(strings::kFooterReady));
					      if (userInitiated)
						      self->showUpdateErrorDialog(result.errorCode);
				      } else if (result.updateAvailable) {
					      self->showFooterStatus(self->text(strings::kFooterReady));
					      self->updateDownloadUrl_ = result.downloadUrl;
					      self->updateSha256_ = result.sha256;
					      self->updateVersion_ = result.latestVersion;
					      self->updateSizeBytes_ = result.sizeBytes;
					      self->setFooterExpandedText(self->footerUpdateButton_,
									  self->text(strings::kSettingsDownloadUpdate));
					      self->setUpdateIndicatorVisible(true);
					      self->showUpdateAvailableDialog();
				      } else if (userInitiated) {
					      self->footerUpdateButton_->setToolTip({});
					      self->showFooterStatus(self->text(strings::kFooterReady));
					      self->showUpToDateDialog();
				      }
			      });
}

void MainDock::showUpToDateDialog()
{
	if (updateStatusDialog_ != nullptr) {
		updateStatusDialog_->raise();
		updateStatusDialog_->activateWindow();
		return;
	}

	auto *dialog =
		new QMessageBox(QMessageBox::Information, text(strings::kSettingsUpToDateTitle),
				text(strings::kSettingsUpdateCurrentVersion).arg(QStringLiteral(CLIPCOACH_VERSION)) +
					QStringLiteral("\n\n") + text(strings::kSettingsUpToDateBody),
				QMessageBox::Ok, this);
	dialog->setObjectName(QStringLiteral("updateStatusDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setTextInteractionFlags(Qt::TextSelectableByMouse);
	tokens::configureCompactUpdateDialog(dialog, 360);
	updateStatusDialog_ = dialog;
	connect(dialog, &QObject::destroyed, this, [this] { updateStatusDialog_ = nullptr; });
	dialog->setWindowModality(Qt::WindowModal);
	dialog->open();
}

void MainDock::showUpdateAvailableDialog()
{
	if (!updateDownloadUrl_.isValid() || updateVersion_.isEmpty()) {
		checkForUpdates(true);
		return;
	}
	if (updateStatusDialog_ != nullptr) {
		updateStatusDialog_->raise();
		updateStatusDialog_->activateWindow();
		return;
	}

	const auto versionSummary =
		text(strings::kSettingsUpdateCurrentVersion).arg(QStringLiteral(CLIPCOACH_VERSION)) +
		QStringLiteral("\n") + text(strings::kSettingsUpdateAvailableVersion).arg(updateVersion_) +
		QStringLiteral("\n\n") + text(strings::kSettingsUpdateAvailableBody);
	auto *dialog = new QMessageBox(QMessageBox::Question, text(strings::kSettingsUpdateAvailableTitle),
				       versionSummary, QMessageBox::NoButton, this);
	dialog->setObjectName(QStringLiteral("updateAvailableDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setTextInteractionFlags(Qt::TextSelectableByMouse);
	tokens::configureCompactUpdateDialog(dialog, 440);
	auto *installButton = dialog->addButton(text(strings::kSettingsUpdateInstallNow), QMessageBox::AcceptRole);
	installButton->setObjectName(QStringLiteral("installAvailableUpdateButton"));
	dialog->addButton(text(strings::kSettingsUpdateLater), QMessageBox::RejectRole);
	connect(installButton, &QPushButton::clicked, this, [this] { beginUpdateDownload(false); });
	updateStatusDialog_ = dialog;
	connect(dialog, &QObject::destroyed, this, [this] { updateStatusDialog_ = nullptr; });
	dialog->setWindowModality(Qt::WindowModal);
	dialog->open();
}

void MainDock::showUpdateErrorDialog(const QString &details)
{
	if (updateStatusDialog_ != nullptr) {
		updateStatusDialog_->raise();
		updateStatusDialog_->activateWindow();
		return;
	}

	auto body = text(strings::kSettingsUpdateCurrentVersion).arg(QStringLiteral(CLIPCOACH_VERSION)) +
		    QStringLiteral("\n\n") + text(strings::kSettingsUpdateErrorBody);
	if (!details.isEmpty())
		body += QStringLiteral("\n\n%1").arg(details);
	auto *dialog = new QMessageBox(QMessageBox::Warning, text(strings::kSettingsUpdateErrorTitle), body,
				       QMessageBox::Ok, this);
	dialog->setObjectName(QStringLiteral("updateErrorDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setTextInteractionFlags(Qt::TextSelectableByMouse);
	tokens::configureCompactUpdateDialog(dialog, 460);
	updateStatusDialog_ = dialog;
	connect(dialog, &QObject::destroyed, this, [this] { updateStatusDialog_ = nullptr; });
	dialog->setWindowModality(Qt::WindowModal);
	dialog->open();
}

void MainDock::beginUpdateDownload(bool requireConfirmation)
{
	if (updateDownloader_ == nullptr || !updateDownloadUrl_.isValid() || updateVersion_.isEmpty() ||
	    updateSha256_.isEmpty() || updateSizeBytes_ <= 0)
		return;

	if (requireConfirmation) {
		showUpdateAvailableDialog();
		return;
	}

	const auto updateDirectory =
		QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QStringLiteral("/ClipXtudio/updates");
	if (!QDir().mkpath(updateDirectory)) {
		showFooterStatus(text(strings::kFooterReady));
		showUpdateErrorDialog(
			text(strings::kSettingsUpdateInstallError).arg(QStringLiteral("UPDATE_DIRECTORY_FAILED")));
		return;
	}
	const auto destination =
		QDir(updateDirectory).filePath(QStringLiteral("ClipXtudio-Setup-%1.exe").arg(updateVersion_));
	network::UpdateCheckResult update;
	update.success = true;
	update.updateAvailable = true;
	update.latestVersion = updateVersion_;
	update.downloadUrl = updateDownloadUrl_;
	update.sha256 = updateSha256_;
	update.sizeBytes = updateSizeBytes_;

	footerUpdateButton_->setEnabled(false);
	setUpdateIndicatorVisible(false);
	QPointer<MainDock> self(this);
	updateDownloader_->download(
		update, destination,
		[self](qint64 received, qint64 total) {
			if (self.isNull() || total <= 0)
				return;
			const auto percent = std::clamp(static_cast<int>((received * 100) / total), 0, 100);
			self->setFooterExpandedText(self->footerUpdateButton_,
						    self->text(strings::kSettingsDownloadingUpdate).arg(percent));
		},
		[self](network::UpdateDownloadResult result) {
			if (self.isNull())
				return;
			if (!result.success) {
				self->footerUpdateButton_->setEnabled(true);
				self->setFooterExpandedText(self->footerUpdateButton_,
							    self->text(strings::kSettingsDownloadUpdate));
				self->setUpdateIndicatorVisible(true);
				self->showFooterStatus(self->text(strings::kFooterReady));
				self->showUpdateErrorDialog(
					self->text(strings::kSettingsUpdateInstallError).arg(result.errorCode));
				return;
			}

			std::string error;
			if (!self->verticalObsBridge_.installUpdate ||
			    !self->verticalObsBridge_.installUpdate(result.filePath.toStdString(), &error)) {
				self->footerUpdateButton_->setEnabled(true);
				self->setFooterExpandedText(self->footerUpdateButton_,
							    self->text(strings::kSettingsDownloadUpdate));
				self->setUpdateIndicatorVisible(true);
				self->showFooterStatus(self->text(strings::kFooterReady));
				self->showUpdateErrorDialog(
					self->text(strings::kSettingsUpdateInstallError)
						.arg(QString::fromStdString(
							error.empty() ? "UPDATE_INSTALLER_UNAVAILABLE" : error)));
				return;
			}
			self->showFooterStatus(self->text(strings::kSettingsInstallingUpdate));
		});
}

} // namespace clipcoach::ui
