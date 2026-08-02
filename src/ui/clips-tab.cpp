#include <clipcoach/ui/clips-tab.hpp>

#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/storage/clip-library-service.hpp>
#include <clipcoach/ui/components/clip-card.hpp>
#include <clipcoach/ui/components/empty-state.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/components/secondary-button.hpp>
#include <clipcoach/ui/components/status-card.hpp>
#include <clipcoach/ui/components/wheel-safe-controls.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/desktop-clip-action-service.hpp>
#include <clipcoach/ui/quick-clip-editor-dialog.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QProgressBar>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <utility>

namespace clipcoach::ui {
namespace {

QString formatDuration(int seconds)
{
	seconds = std::max(seconds, 0);
	const auto hours = seconds / 3600;
	const auto minutes = (seconds % 3600) / 60;
	const auto remainder = seconds % 60;
	return hours > 0 ? QStringLiteral("%1:%2:%3")
				   .arg(hours)
				   .arg(minutes, 2, 10, QLatin1Char('0'))
				   .arg(remainder, 2, 10, QLatin1Char('0'))
			 : QStringLiteral("%1:%2").arg(minutes).arg(remainder, 2, 10, QLatin1Char('0'));
}

} // namespace

ClipsTab::ClipsTab(TranslationFunction translator, storage::ClipLibraryService *libraryService, QString sessionId,
		   QWidget *parent, ClipActionService *actionService, ExportManager *exportManager,
		   SettingsManager *settingsManager, VerticalCanvasManager *verticalManager,
		   FeatureGateService *featureGates, CaptionGenerator captionGenerator)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  exportManager_(exportManager),
	  settingsManager_(settingsManager),
	  featureGates_(featureGates),
	  captionGenerator_(std::move(captionGenerator))
{
	setObjectName(QStringLiteral("clipsTab"));
	setAttribute(Qt::WA_StyledBackground, true);
	if (actionService == nullptr) {
		ownedActionService_ =
			std::make_unique<DesktopClipActionService>(exportManager, settingsManager, verticalManager);
		actionService = ownedActionService_.get();
	}
	controller_ =
		new ClipLibraryController(libraryService, actionService, sessionId.toStdString(), this, featureGates_);
	buildUi();
	bindUi();
	controller_->setStateCallback([this](ClipLibraryUiState state) { applyState(std::move(state)); });
}

ClipsTab::~ClipsTab() = default;

void ClipsTab::reload()
{
	controller_->reload();
}

void ClipsTab::refreshFeatureGates()
{
	const bool batchAllowed = featureGates_ != nullptr && featureGates_->isAllowed(Feature::BatchExport);
	const bool historyAllowed = featureGates_ != nullptr && featureGates_->isAllowed(Feature::FullHistory);
	if (batchExportButton_ != nullptr) {
		batchExportButton_->setEnabled(batchAllowed && !selectedClipIds_.empty());
		batchExportButton_->setToolTip(batchAllowed ? QString{} : text(strings::kErrorProRequired));
	}
	updateSelectionActions();
	if (historyScope_ != nullptr) {
		historyScope_->setEnabled(historyAllowed);
		if (!historyAllowed && historyScope_->currentIndex() != 0)
			historyScope_->setCurrentIndex(0);
	}
	if (controller_ != nullptr)
		controller_->reload();
}

void ClipsTab::setCaptionBusyCallback(CaptionBusyCallback callback)
{
	captionBusyCallback_ = std::move(callback);
}

QString ClipsTab::text(const char *key) const
{
	return translator_ ? translator_(key) : QString::fromUtf8(key);
}

void ClipsTab::buildUi()
{
	auto *root = new QVBoxLayout(this);
	root->setSizeConstraint(QLayout::SetNoConstraint);
	root->setContentsMargins(tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin);
	root->setSpacing(tokens::kSectionGap);

	auto *pageHeader = new QFrame(this);
	pageHeader->setObjectName(QStringLiteral("clipsPageHeader"));
	pageHeader->setProperty("clipsRole", QStringLiteral("pageHeader"));
	pageHeader->setProperty("pageRole", QStringLiteral("header"));
	auto *pageHeaderLayout = new QHBoxLayout(pageHeader);
	pageHeaderLayout->setContentsMargins(tokens::kCardPaddingHorizontal, tokens::kCardPaddingVertical,
					     tokens::kCardPaddingHorizontal, tokens::kCardPaddingVertical);
	pageHeaderLayout->setSpacing(tokens::kSpaceMd);
	auto *headerIcon = new QLabel(pageHeader);
	headerIcon->setObjectName(QStringLiteral("clipsHeaderIcon"));
	headerIcon->setProperty("pageRole", QStringLiteral("icon"));
	headerIcon->setAlignment(Qt::AlignCenter);
	headerIcon->setPixmap(style()->standardIcon(QStyle::SP_FileDialogContentsView).pixmap(24, 24));
	headerIcon->setFixedSize(tokens::kPageHeaderIconSize, tokens::kPageHeaderIconSize);
	auto *headerCopy = new QWidget(pageHeader);
	auto *headerCopyLayout = new QVBoxLayout(headerCopy);
	headerCopyLayout->setContentsMargins(0, 0, 0, 0);
	headerCopyLayout->setSpacing(tokens::kSpaceXs);
	auto *title = new QLabel(text(strings::kClipsTitle), headerCopy);
	title->setObjectName(QStringLiteral("PageTitle"));
	auto *subtitle = new QLabel(text(strings::kClipsSubtitle), headerCopy);
	subtitle->setObjectName(QStringLiteral("PageSubtitle"));
	subtitle->setWordWrap(true);
	headerCopyLayout->addWidget(title);
	headerCopyLayout->addWidget(subtitle);
	pageHeaderLayout->addWidget(headerIcon, 0, Qt::AlignTop);
	pageHeaderLayout->addWidget(headerCopy, 1);
	root->addWidget(pageHeader);

	overviewHost_ = new QWidget(this);
	overviewHost_->setObjectName(QStringLiteral("clipsOverviewHost"));
	overviewLayout_ = new QGridLayout(overviewHost_);
	overviewLayout_->setContentsMargins(0, 0, 0, 0);
	overviewLayout_->setSpacing(tokens::kSpaceMd);
	overviewLayout_->setObjectName(QStringLiteral("clipsOverviewLayout"));
	cardsHost_ = new QWidget(overviewHost_);
	cardsHost_->setObjectName(QStringLiteral("clipsSummaryCards"));
	cardsHost_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	metricsLayout_ = new QGridLayout(cardsHost_);
	metricsLayout_->setObjectName(QStringLiteral("clipsMetricsRow"));
	metricsLayout_->setContentsMargins(0, 0, 0, 0);
	metricsLayout_->setSpacing(tokens::kSpaceSm);
	totalCard_ = new StatusCard(text(strings::kClipsTotal), QStringLiteral("0"), false, this);
	totalCard_->setObjectName(QStringLiteral("clipsTotalCard"));
	favoriteCard_ = new StatusCard(text(strings::kClipsFilterFavorites), QStringLiteral("0"), false, this);
	favoriteCard_->setObjectName(QStringLiteral("clipsFavoritesCard"));
	verticalCard_ = new StatusCard(text(strings::kClipsFilterVertical), QStringLiteral("0"), false, this);
	verticalCard_->setObjectName(QStringLiteral("clipsVerticalCard"));
	pendingCard_ = new StatusCard(text(strings::kClipsFilterPending), QStringLiteral("0"), false, this);
	pendingCard_->setObjectName(QStringLiteral("clipsPendingCard"));
	capturedDurationCard_ =
		new StatusCard(text(strings::kClipsSessionDuration), QStringLiteral("0:00"), false, this);
	capturedDurationCard_->setObjectName(QStringLiteral("clipsCapturedDurationCard"));
	int summaryTone = 0;
	for (auto *card : {totalCard_, favoriteCard_, verticalCard_, pendingCard_, capturedDurationCard_}) {
		card->setProperty("cardRole", QStringLiteral("summary"));
		card->setProperty("summaryTone", summaryTone++);
		card->setMinimumHeight(tokens::kSummaryCardMinHeight);
		card->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	}

	filtersHost_ = new QFrame(overviewHost_);
	filtersHost_->setObjectName(QStringLiteral("clipsCompactFilters"));
	filtersHost_->setProperty("cardRole", QStringLiteral("filterPanel"));
	filtersHost_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	filtersLayout_ = new QGridLayout(filtersHost_);
	filtersLayout_->setObjectName(QStringLiteral("clipsFiltersGrid"));
	filtersLayout_->setContentsMargins(tokens::kSpaceSm, tokens::kSpaceSm, tokens::kSpaceSm, tokens::kSpaceSm);
	filtersLayout_->setSpacing(tokens::kSpaceXs);
	filterGroup_ = new QButtonGroup(this);
	filterGroup_->setExclusive(true);
	const std::pair<const char *, const char *> filterSpecs[] = {
		{"clipsFilterAll", strings::kClipsFilterAll},
		{"clipsFilterFavorites", strings::kClipsFilterFavorites},
		{"clipsFilterVertical", strings::kClipsFilterVertical},
		{"clipsFilterPending", strings::kClipsFilterPending},
	};
	for (int index = 0; index < 4; ++index) {
		auto *button = new QPushButton(text(filterSpecs[index].second), this);
		button->setObjectName(QString::fromLatin1(filterSpecs[index].first));
		button->setProperty("controlRole", QStringLiteral("filter"));
		button->setCheckable(true);
		button->setChecked(index == 0);
		button->setMinimumWidth(72);
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		if (index == 3)
			button->setToolTip(text(strings::kClipsFilterPendingHelp));
		filterGroup_->addButton(button, index);
		filterButtons_[static_cast<std::size_t>(index)] = button;
	}
	root->addWidget(overviewHost_);

	exportHost_ = new QFrame(this);
	exportHost_->setObjectName(QStringLiteral("clipsExportHost"));
	exportHost_->setProperty("clipsRole", QStringLiteral("toolbar"));
	exportLayout_ = new QGridLayout(exportHost_);
	exportLayout_->setObjectName(QStringLiteral("clipsExportGrid"));
	exportLayout_->setContentsMargins(0, tokens::kSpaceXs, 0, tokens::kSpaceXs);
	exportLayout_->setSpacing(tokens::kSpaceSm);
	searchBox_ = new QLineEdit(exportHost_);
	searchBox_->setObjectName(QStringLiteral("clipsSearchBox"));
	searchBox_->setPlaceholderText(text(strings::kClipsSearchPlaceholder));
	searchBox_->setClearButtonEnabled(true);
	searchBox_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	searchBox_->addAction(style()->standardIcon(QStyle::SP_FileDialogContentsView), QLineEdit::LeadingPosition);
	historyScope_ = new WheelSafeComboBox(exportHost_);
	historyScope_->setObjectName(QStringLiteral("clipsHistoryScope"));
	historyScope_->addItem(text(strings::kClipsScopeCurrent), false);
	historyScope_->addItem(text(strings::kClipsScopeHistory), true);
	historyScope_->setToolTip(text(strings::kClipsScopeHelp));
	historyScope_->setAccessibleName(text(strings::kClipsScopeHelp));
	historyScope_->setCurrentIndex(0);
	historyScope_->setEnabled(featureGates_ != nullptr && featureGates_->isAllowed(Feature::FullHistory));
	batchOrientation_ = new WheelSafeComboBox(this);
	batchOrientation_->setObjectName(QStringLiteral("batchExportOrientation"));
	batchOrientation_->addItem(text(strings::kOrientationHorizontal),
				   static_cast<int>(ExportOrientation::Horizontal));
	batchOrientation_->addItem(text(strings::kOrientationVertical), static_cast<int>(ExportOrientation::Vertical));
	batchOrientation_->addItem(text(strings::kOrientationBoth), static_cast<int>(ExportOrientation::Both));
	batchExportButton_ = new QPushButton(text(strings::kClipsExportSelection), this);
	batchExportButton_->setObjectName(QStringLiteral("batchExportButton"));
	batchExportButton_->setProperty("controlRole", QStringLiteral("secondary"));
	batchExportButton_->setProperty("toolbarAction", true);
	batchExportButton_->setFixedHeight(tokens::kFooterControlHeight);
	batchExportButton_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
	batchExportButton_->setEnabled(false);
	batchExportButton_->setToolTip(text(strings::kErrorProRequired));
	cancelExportButton_ = new QPushButton(text(strings::kClipsCancelExport), this);
	cancelExportButton_->setObjectName(QStringLiteral("cancelExportButton"));
	cancelExportButton_->setProperty("controlRole", QStringLiteral("secondary"));
	cancelExportButton_->setProperty("toolbarAction", true);
	cancelExportButton_->setFixedHeight(tokens::kFooterControlHeight);
	cancelExportButton_->setEnabled(false);
	batchDeleteButton_ = new QPushButton(text(strings::kClipsDeleteSelection), this);
	batchDeleteButton_->setObjectName(QStringLiteral("batchDeleteButton"));
	batchDeleteButton_->setProperty("controlRole", QStringLiteral("secondary"));
	batchDeleteButton_->setProperty("toolbarAction", true);
	batchDeleteButton_->setFixedHeight(tokens::kFooterControlHeight);
	batchDeleteButton_->setProperty("destructiveAction", true);
	batchDeleteButton_->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
	batchDeleteButton_->setEnabled(false);
	batchBadge_ = new ProBadge(text(strings::kPro), exportHost_);
	batchBadge_->setObjectName(QStringLiteral("batchExportProBadge"));
	sortCombo_ = new WheelSafeComboBox(exportHost_);
	sortCombo_->setObjectName(QStringLiteral("clipsSort"));
	sortCombo_->addItem(text(strings::kClipsSortDate), static_cast<int>(ClipSort::DateDescending));
	sortCombo_->addItem(text(strings::kClipsSortScore), static_cast<int>(ClipSort::ScoreDescending));
	root->addWidget(exportHost_);
	root->addWidget(filtersHost_);

	exportProgress_ = new QProgressBar(this);
	exportProgress_->setObjectName(QStringLiteral("exportProgressBar"));
	exportProgress_->setRange(0, 100);
	exportProgress_->hide();
	exportStatusLabel_ = new QLabel(this);
	exportStatusLabel_->setObjectName(QStringLiteral("exportStatusLabel"));
	exportStatusLabel_->setProperty("class", QStringLiteral("supporting"));
	exportStatusLabel_->hide();
	root->addWidget(exportProgress_);
	root->addWidget(exportStatusLabel_);

	loadingLabel_ = new QLabel(text(strings::kClipsLoading), this);
	loadingLabel_->setObjectName(QStringLiteral("clipsLoadingLabel"));
	loadingLabel_->setAlignment(Qt::AlignCenter);
	loadingLabel_->hide();
	errorLabel_ = new QLabel(this);
	errorLabel_->setObjectName(QStringLiteral("libraryErrorNotification"));
	errorLabel_->setProperty("notificationTone", QStringLiteral("error"));
	errorLabel_->setWordWrap(true);
	errorLabel_->hide();
	emptyState_ = new EmptyState(text(strings::kEmptyClipsTitle), text(strings::kEmptyClipsBody), this);

	auto *scroll = new QScrollArea(this);
	scroll->setObjectName(QStringLiteral("clipsListScroll"));
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setFrameShape(QFrame::NoFrame);
	listContainer_ = new QWidget(scroll);
	listContainer_->setObjectName(QStringLiteral("clipsListContainer"));
	listLayout_ = new QVBoxLayout(listContainer_);
	listLayout_->setContentsMargins(0, 0, tokens::kSpaceXs, 0);
	listLayout_->setSpacing(tokens::kSpaceSm);
	listLayout_->setAlignment(Qt::AlignTop);
	scroll->setWidget(listContainer_);

	root->addWidget(loadingLabel_);
	root->addWidget(errorLabel_);
	root->addWidget(emptyState_);
	root->addWidget(scroll, 1);

	auto *summary = new QFrame(this);
	summary->setObjectName(QStringLiteral("clipsSessionSummaryPanel"));
	summary->setProperty("cardRole", QStringLiteral("sessionSummary"));
	auto *summaryLayout = new QVBoxLayout(summary);
	summaryLayout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceSm, tokens::kSpaceMd, tokens::kSpaceSm);
	auto *summaryHeader = new QHBoxLayout();
	auto *summaryTitle = new QLabel(text(strings::kClipsSessionSummary), summary);
	summaryTitle->setObjectName(QStringLiteral("SectionLabel"));
	auto *summaryBadge = new ProBadge(text(strings::kPro), summary);
	summaryBadge->setObjectName(QStringLiteral("sessionRecapProBadge"));
	summaryHeader->addWidget(summaryTitle);
	summaryHeader->addStretch(1);
	summaryHeader->addWidget(summaryBadge);
	summaryLabel_ = new QLabel(summary);
	summaryLabel_->setObjectName(QStringLiteral("SupportingText"));
	summaryLabel_->setWordWrap(true);
	summaryLayout->addLayout(summaryHeader);
	summaryLayout->addWidget(summaryLabel_);
	root->addWidget(summary);
	refreshFeatureGates();
	updateResponsiveLayout();
}

void ClipsTab::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateResponsiveLayout();
}

void ClipsTab::updateResponsiveLayout()
{
	if (overviewLayout_ == nullptr || filtersLayout_ == nullptr || exportLayout_ == nullptr)
		return;

	const bool compact = width() < 720;
	const int metricColumns = width() >= 1180 ? 5 : (width() >= 760 ? 3 : 2);
	if (compact == compactLayout_ && metricColumns == metricColumnCount_ &&
	    overviewLayout_->indexOf(cardsHost_) >= 0)
		return;
	compactLayout_ = compact;
	metricColumnCount_ = metricColumns;
	overviewHost_->setProperty("layoutMode", compact ? QStringLiteral("compact") : QStringLiteral("wide"));

	overviewLayout_->removeWidget(cardsHost_);
	for (auto *card : {totalCard_, favoriteCard_, verticalCard_, pendingCard_, capturedDurationCard_})
		metricsLayout_->removeWidget(card);
	for (auto *button : filterButtons_)
		filtersLayout_->removeWidget(button);
	for (auto *widget : {static_cast<QWidget *>(searchBox_), static_cast<QWidget *>(historyScope_),
			     static_cast<QWidget *>(batchOrientation_), static_cast<QWidget *>(batchExportButton_),
			     batchBadge_, static_cast<QWidget *>(cancelExportButton_),
			     static_cast<QWidget *>(batchDeleteButton_), static_cast<QWidget *>(sortCombo_)})
		exportLayout_->removeWidget(widget);

	const std::array<StatusCard *, 5> metricCards{totalCard_, favoriteCard_, verticalCard_, pendingCard_,
						      capturedDurationCard_};
	for (int index = 0; index < static_cast<int>(metricCards.size()); ++index) {
		const int row = index / metricColumns;
		const int column = index % metricColumns;
		const int span = metricColumns == 2 && index == 4 ? 2 : 1;
		metricsLayout_->addWidget(metricCards[static_cast<std::size_t>(index)], row, column, 1, span);
	}
	const int metricRows = (static_cast<int>(metricCards.size()) + metricColumns - 1) / metricColumns;
	cardsHost_->setMinimumHeight(metricRows * 68 + std::max(0, metricRows - 1) * tokens::kSpaceSm);
	for (int column = 0; column < metricColumns; ++column)
		metricsLayout_->setColumnStretch(column, 1);
	overviewLayout_->addWidget(cardsHost_, 0, 0);

	if (compact) {
		for (int index = 0; index < 4; ++index)
			filtersLayout_->addWidget(filterButtons_[static_cast<std::size_t>(index)], index / 2,
						  index % 2);

		exportLayout_->addWidget(searchBox_, 0, 0, 1, 4);
		exportLayout_->addWidget(historyScope_, 1, 0, 1, 2);
		exportLayout_->addWidget(sortCombo_, 1, 2, 1, 2);
		exportLayout_->addWidget(batchOrientation_, 2, 0, 1, 2);
		exportLayout_->addWidget(batchExportButton_, 2, 2, 1, 2);
		exportLayout_->addWidget(batchBadge_, 3, 0);
		exportLayout_->addWidget(cancelExportButton_, 3, 1);
		exportLayout_->addWidget(batchDeleteButton_, 3, 2, 1, 2);
	} else {
		for (int index = 0; index < 4; ++index)
			filtersLayout_->addWidget(filterButtons_[static_cast<std::size_t>(index)], 0, index);

		exportLayout_->addWidget(searchBox_, 0, 0);
		exportLayout_->addWidget(historyScope_, 0, 1);
		exportLayout_->addWidget(batchOrientation_, 0, 2);
		exportLayout_->addWidget(sortCombo_, 0, 3);
		exportLayout_->setColumnStretch(0, 2);
		exportLayout_->setColumnStretch(4, 1);
		exportLayout_->addWidget(batchExportButton_, 0, 5);
		exportLayout_->addWidget(batchBadge_, 0, 6);
		exportLayout_->addWidget(cancelExportButton_, 0, 7);
		exportLayout_->addWidget(batchDeleteButton_, 0, 8);
	}
	for (auto *button : {batchExportButton_, cancelExportButton_, batchDeleteButton_})
		exportLayout_->setAlignment(button, Qt::AlignVCenter);
	overviewHost_->updateGeometry();
	filtersHost_->updateGeometry();
	exportHost_->updateGeometry();
}

void ClipsTab::bindUi()
{
	connect(filterGroup_, &QButtonGroup::idClicked, this,
		[this](int id) { controller_->setFilter(static_cast<ClipFilter>(id)); });
	auto *search = findChild<QLineEdit *>(QStringLiteral("clipsSearchBox"));
	connect(search, &QLineEdit::textChanged, this,
		[this](const QString &query) { controller_->setSearchQuery(query.toStdString()); });
	connect(historyScope_, &QComboBox::currentIndexChanged, this,
		[this](int) { controller_->setFullHistory(historyScope_->currentData().toBool()); });
	auto *sort = findChild<QComboBox *>(QStringLiteral("clipsSort"));
	connect(sort, &QComboBox::currentIndexChanged, this,
		[this, sort](int) { controller_->setSort(static_cast<ClipSort>(sort->currentData().toInt())); });
	connect(batchExportButton_, &QPushButton::clicked, this, [this] {
		const auto orientation = static_cast<ExportOrientation>(batchOrientation_->currentData().toInt());
		showActionResult(controller_->exportSelection(selectedClipIds_, orientation),
				 strings::kClipsActionError);
	});
	connect(cancelExportButton_, &QPushButton::clicked, this, [this] {
		if (exportManager_ == nullptr) {
			return;
		}
		for (const auto &job : exportManager_->jobs()) {
			if (job.state == ExportJobState::Exporting || job.state == ExportJobState::Pending) {
				std::string error;
				if (!exportManager_->cancel(job.id, &error)) {
					errorLabel_->setText(QString::fromStdString(error));
					errorLabel_->show();
				}
				break;
			}
		}
	});
	connect(batchDeleteButton_, &QPushButton::clicked, this, [this] { confirmAndDelete(selectedClipIds_); });
	exportPollTimer_ = new QTimer(this);
	exportPollTimer_->setInterval(250);
	connect(exportPollTimer_, &QTimer::timeout, this, [this] { refreshExportProgress(); });
	if (exportManager_ != nullptr) {
		exportPollTimer_->start();
	}
}

void ClipsTab::applyState(ClipLibraryUiState state)
{
	loadingLabel_->setVisible(state.loading);
	if (state.errorType != ClipLibraryErrorType::None) {
		errorLabel_->setText(text(state.errorType == ClipLibraryErrorType::Favorite
						  ? strings::kClipsFavoriteError
						  : strings::kClipsLoadError));
		errorLabel_->show();
	} else {
		errorLabel_->hide();
	}

	totalCard_->setValue(QString::number(state.summary.totalClips));
	favoriteCard_->setValue(QString::number(state.summary.favoriteClips));
	verticalCard_->setValue(QString::number(state.summary.verticalClips));
	pendingCard_->setValue(QString::number(state.summary.pendingClips));
	capturedDurationCard_->setValue(formatDuration(state.summary.capturedDurationSeconds));
	const bool recapAllowed = featureGates_ != nullptr && featureGates_->isAllowed(Feature::SessionRecap);
	summaryLabel_->setText(recapAllowed ? text(strings::kClipsSummaryFormat)
						      .arg(state.summary.totalClips)
						      .arg(state.summary.favoriteClips)
						      .arg(state.summary.verticalClips)
						      .arg(state.summary.pendingClips)
						      .arg(formatDuration(state.summary.capturedDurationSeconds))
					    : text(strings::kErrorProRequired));

	emptyState_->setVisible(!state.loading && state.clips.empty());
	startBatchedRender(std::move(state.clips));
}

void ClipsTab::startBatchedRender(std::vector<ClipMetadata> clips)
{
	++renderGeneration_;
	clearCards();
	pendingRender_ = std::move(clips);
	nextRenderIndex_ = 0;
	renderNextBatch(renderGeneration_);
}

void ClipsTab::renderNextBatch(std::uint64_t generation)
{
	if (generation != renderGeneration_) {
		return;
	}
	const auto end = std::min(nextRenderIndex_ + 20, pendingRender_.size());
	while (nextRenderIndex_ < end) {
		addClipCard(pendingRender_[nextRenderIndex_++]);
	}
	if (nextRenderIndex_ < pendingRender_.size()) {
		QTimer::singleShot(0, this, [this, generation] { renderNextBatch(generation); });
	}
}

void ClipsTab::addClipCard(const ClipMetadata &clip)
{
	const auto created = QDateTime::fromSecsSinceEpoch(
		std::chrono::duration_cast<std::chrono::seconds>(clip.createdAt.time_since_epoch()).count());
	ClipCardViewData data;
	data.clipId = QString::fromStdString(clip.id);
	data.title = QString::fromStdString(clip.title.empty() ? clip.fileName : clip.title);
	data.duration = formatDuration(clip.durationSeconds);
	data.dateTime = created.toString(QStringLiteral("dd MMM yyyy · HH:mm"));
	if (clip.triggerLabel == "remote_clipper") {
		data.trigger =
			clip.requestedBy.empty()
				? text(strings::kTriggerRemoteClipper)
				: text(strings::kTriggerRemoteClipperBy).arg(QString::fromStdString(clip.requestedBy));
	} else {
		data.trigger = clip.triggerLabel.empty() || clip.triggerType == TriggerType::Manual
				       ? triggerText(clip.triggerType)
				       : QString::fromStdString(clip.triggerLabel);
	}
	switch (clip.orientation) {
	case ClipOrientation::Horizontal:
		data.orientation = text(strings::kOrientationHorizontal);
		break;
	case ClipOrientation::Vertical:
		data.orientation = text(strings::kOrientationVertical);
		break;
	case ClipOrientation::Both:
		data.orientation = text(strings::kOrientationBoth);
		break;
	}
	data.thumbnailPath = QString::fromStdString(clip.thumbnailPath.u8string());
	data.favoriteTooltip = text(strings::kClipsFavorite);
	data.previewLabel = text(strings::kClipsPreview);
	data.editLabel = text(strings::kClipsQuickEditor);
	data.exportLabel = text(clip.orientation == ClipOrientation::Vertical ? strings::kClipsAlreadyVertical
									      : strings::kClipsExportVertical);
	data.captionLabel = text(strings::kClipsCaption);
	data.subtitlesLabel = text(strings::kClipsSubtitlesComingSoon);
	data.openFolderLabel = text(strings::kClipsOpenFolder);
	data.deleteLabel = text(strings::kClipsDelete);
	data.scoreLabel = text(strings::kClipsScoreLabel);
	data.processingLabel = text(strings::kClipsProcessing);
	data.score = clip.score;
	data.processing = clip.exportStatus == ExportStatus::Pending || clip.exportStatus == ExportStatus::Exporting;
	data.favorite = clip.isFavorite;
	data.selected = std::find(selectedClipIds_.begin(), selectedClipIds_.end(), clip.id) != selectedClipIds_.end();
	const bool captionAllowed = featureGates_ == nullptr || featureGates_->isAllowed(Feature::AiCaptions);
	data.captionAvailable = captionAllowed && (!clip.caption.empty() || static_cast<bool>(captionGenerator_));
	data.subtitlesAvailable = false;
	data.exportVerticalEnabled = clip.orientation != ClipOrientation::Vertical;

	const auto title = data.title;
	auto *card = new ClipCard(std::move(data), listContainer_);
	const auto id = clip.id;
	card->setFavoriteCallback([this, id](bool favorite) { controller_->setFavorite(id, favorite); });
	card->setPreviewCallback(
		[this, id] { showActionResult(controller_->preview(id), strings::kClipsActionError); });
	card->setEditCallback([this, clip] {
		auto *editor = new QuickClipEditorDialog(translator_, clip, exportManager_, settingsManager_, this);
		QStringList hashtags;
		for (const auto &hashtag : clip.hashtags)
			hashtags.push_back(QString::fromStdString(hashtag));
		if (!clip.caption.empty()) {
			const auto social = formatSocialCaption(QString::fromStdString(clip.caption), hashtags,
								QString::fromStdString(clip.aiSummary));
			editor->setCaption(social,
					   formatYouTubeShortsCaption(QString::fromStdString(clip.title), social));
		}
		QPointer<QuickClipEditorDialog> safeEditor(editor);
		editor->setCaptionRequest([this, clip, safeEditor] {
			if (safeEditor.isNull())
				return;
			safeEditor->setCaptionBusy(true);
			requestCaption(
				clip, nullptr,
				[safeEditor](const QString &social, const QString &shorts) {
					if (!safeEditor.isNull())
						safeEditor->setCaption(social, shorts);
				},
				[safeEditor](const CaptionGenerationProgress &progress) {
					if (!safeEditor.isNull())
						safeEditor->setCaptionProgress(progress);
				},
				[safeEditor](const QString &error) {
					if (!safeEditor.isNull())
						safeEditor->setCaptionError(error);
				});
		});
		editor->open();
	});
	card->setExportCallback(
		[this, id] { showActionResult(controller_->exportVertical(id), strings::kClipsExportFailed); });
	card->setCaptionCallback([this, clip, card] { requestCaption(clip, card); });
	card->setOpenFolderCallback(
		[this, id] { showActionResult(controller_->openFolder(id), strings::kClipsActionError); });
	card->setDeleteCallback([this, id, title] { confirmAndDelete({id}, title); });
	card->setSelectionCallback([this, id](bool selected) {
		const auto found = std::find(selectedClipIds_.begin(), selectedClipIds_.end(), id);
		if (selected && found == selectedClipIds_.end()) {
			selectedClipIds_.push_back(id);
		} else if (!selected && found != selectedClipIds_.end()) {
			selectedClipIds_.erase(found);
		}
		updateSelectionActions();
	});
	listLayout_->addWidget(card);
}

void ClipsTab::requestCaption(const ClipMetadata &clip, ClipCard *card, CaptionReadyCallback ready,
			      CaptionProgressReadyCallback progressReady, CaptionFailedCallback failed)
{
	const auto clipTitle = QString::fromStdString(clip.title.empty() ? clip.fileName : clip.title);
	if (!clip.caption.empty()) {
		errorLabel_->hide();
		QStringList hashtags;
		for (const auto &hashtag : clip.hashtags)
			hashtags.push_back(QString::fromStdString(hashtag));
		const auto socialCaption = formatSocialCaption(QString::fromStdString(clip.caption), hashtags,
							       QString::fromStdString(clip.aiSummary));
		const auto shorts = formatYouTubeShortsCaption(clipTitle, socialCaption);
		if (ready)
			ready(socialCaption, shorts);
		else
			showCaptionDialog(socialCaption, clipTitle, shorts);
		return;
	}
	if (featureGates_ != nullptr && !featureGates_->isAllowed(Feature::AiCaptions)) {
		const auto message = text(strings::kErrorProRequired);
		if (failed)
			failed(message);
		else if (ready)
			ready({}, {});
		errorLabel_->setText(message);
		errorLabel_->show();
		return;
	}
	if (!captionGenerator_) {
		const auto message = text(strings::kClipsCaptionFailed);
		if (failed)
			failed(message);
		else if (ready)
			ready({}, {});
		errorLabel_->setText(message);
		errorLabel_->show();
		return;
	}

	if (card != nullptr)
		card->setCaptionBusy(true, text(strings::kClipsCaptionGenerating));
	if (captionBusyCallback_)
		captionBusyCallback_(true, {1, text(strings::kClipsCaptionValidatingLicense), 10});
	errorLabel_->setProperty("notificationTone", QStringLiteral("info"));
	errorLabel_->style()->unpolish(errorLabel_);
	errorLabel_->style()->polish(errorLabel_);
	errorLabel_->setText(text(strings::kClipsCaptionGenerating));
	errorLabel_->show();
	QPointer<ClipsTab> self(this);
	QPointer<ClipCard> safeCard(card);
	captionGenerator_(
		clip,
		[self, progressReady](CaptionGenerationProgress progress) {
			if (!self.isNull() && self->captionBusyCallback_)
				self->captionBusyCallback_(true, progress);
			if (progressReady)
				progressReady(progress);
		},
		[self, safeCard, clipTitle, ready = std::move(ready),
		 failed = std::move(failed)](CaptionGenerationResult result) {
			if (self.isNull())
				return;
			if (self->captionBusyCallback_)
				self->captionBusyCallback_(false, {});
			if (!safeCard.isNull())
				safeCard->setCaptionBusy(false, self->text(strings::kClipsCaption));
			if (!result.success || result.caption.trimmed().isEmpty()) {
				const auto message = result.error.trimmed().isEmpty()
							     ? self->text(strings::kClipsCaptionFailed)
							     : result.error;
				if (failed)
					failed(message);
				else if (ready)
					ready({}, {});
				self->errorLabel_->setProperty("notificationTone", QStringLiteral("error"));
				self->errorLabel_->style()->unpolish(self->errorLabel_);
				self->errorLabel_->style()->polish(self->errorLabel_);
				self->errorLabel_->setText(message);
				self->errorLabel_->show();
				if (!failed)
					QMessageBox::warning(self, self->text(strings::kClipsCaptionFailed),
							     self->errorLabel_->text());
				return;
			}
			self->errorLabel_->hide();
			if (ready)
				ready(result.caption, result.youtubeShortsCaption);
			else
				self->showCaptionDialog(result.caption, clipTitle, result.youtubeShortsCaption);
			QTimer::singleShot(0, self, [self] {
				if (!self.isNull())
					self->reload();
			});
		});
}

void ClipsTab::showCaptionDialog(const QString &caption, const QString &clipTitle, const QString &youtubeShortsCaption)
{
	const auto seoSubject = clipTitle.trimmed().isEmpty() ? text(strings::kClipsCaptionDialogTitle)
							      : clipTitle.trimmed();
	const auto socialCaption =
		formatSocialCaption(caption, {}, text(strings::kClipsCaptionSeoFallback).arg(seoSubject));
	const auto shortsCaption = youtubeShortsCaption.trimmed().isEmpty()
					   ? formatYouTubeShortsCaption(clipTitle, socialCaption)
					   : formatYouTubeShortsCaption(youtubeShortsCaption, socialCaption);
	auto *dialog = new QDialog(this);
	dialog->setObjectName(QStringLiteral("clipCaptionDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setWindowTitle(text(strings::kClipsCaptionDialogTitle));
	dialog->setModal(false);
	dialog->resize(640, 660);

	auto *layout = new QVBoxLayout(dialog);
	layout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceLg, tokens::kSpaceLg, tokens::kSpaceLg);
	layout->setSpacing(tokens::kSpaceMd);
	auto *title = new QLabel(text(strings::kClipsCaptionDialogTitle), dialog);
	title->setObjectName(QStringLiteral("captionDialogTitle"));
	title->setProperty("textRole", QStringLiteral("sectionTitle"));
	layout->addWidget(title);
	if (!clipTitle.trimmed().isEmpty()) {
		auto *clipLabel = new QLabel(clipTitle, dialog);
		clipLabel->setProperty("textRole", QStringLiteral("muted"));
		clipLabel->setWordWrap(true);
		layout->addWidget(clipLabel);
	}
	auto *description = new QLabel(text(strings::kClipsCaptionDescription), dialog);
	description->setProperty("textRole", QStringLiteral("muted"));
	description->setWordWrap(true);
	layout->addWidget(description);
	auto *socialHeader = new QHBoxLayout();
	auto *socialLabel = new QLabel(text(strings::kClipsCaptionSocialLabel), dialog);
	socialLabel->setProperty("textRole", QStringLiteral("sectionTitle"));
	socialHeader->addWidget(socialLabel);
	socialHeader->addStretch();
	layout->addLayout(socialHeader);
	auto *editor = new QPlainTextEdit(socialCaption, dialog);
	editor->setObjectName(QStringLiteral("captionText"));
	editor->setReadOnly(true);
	editor->setMinimumHeight(150);
	layout->addWidget(editor, 1);
	auto *socialActions = new QHBoxLayout();
	socialActions->addStretch();
	auto *copySocial = new QPushButton(text(strings::kClipsCaptionCopySocial), dialog);
	copySocial->setObjectName(QStringLiteral("copyCaptionButton"));
	copySocial->setProperty("buttonRole", QStringLiteral("primary"));
	socialActions->addWidget(copySocial);
	layout->addLayout(socialActions);

	auto *shortsHeader = new QHBoxLayout();
	auto *shortsLabel = new QLabel(text(strings::kClipsCaptionYouTubeLabel), dialog);
	shortsLabel->setProperty("textRole", QStringLiteral("sectionTitle"));
	shortsHeader->addWidget(shortsLabel);
	shortsHeader->addStretch();
	auto *shortsCount = new QLabel(
		QStringLiteral("%1 / %2").arg(shortsCaption.toUcs4().size()).arg(kYouTubeShortsMaximumCharacters),
		dialog);
	shortsCount->setObjectName(QStringLiteral("youtubeShortsCharacterCount"));
	shortsCount->setProperty("textRole", QStringLiteral("muted"));
	shortsHeader->addWidget(shortsCount);
	layout->addLayout(shortsHeader);
	auto *shortsEditor = new QPlainTextEdit(shortsCaption, dialog);
	shortsEditor->setObjectName(QStringLiteral("youtubeShortsCaptionText"));
	shortsEditor->setReadOnly(true);
	shortsEditor->setMinimumHeight(90);
	layout->addWidget(shortsEditor);
	auto *shortsActions = new QHBoxLayout();
	shortsActions->addStretch();
	auto *copyShorts = new QPushButton(text(strings::kClipsCaptionCopyYouTube), dialog);
	copyShorts->setObjectName(QStringLiteral("copyYoutubeShortsCaptionButton"));
	copyShorts->setProperty("buttonRole", QStringLiteral("primary"));
	shortsActions->addWidget(copyShorts);
	layout->addLayout(shortsActions);

	auto *buttons = new QDialogButtonBox(dialog);
	auto *close = buttons->addButton(text(strings::kClipsCaptionClose), QDialogButtonBox::RejectRole);
	QObject::connect(copySocial, &QPushButton::clicked, dialog, [this, editor, copySocial] {
		QApplication::clipboard()->setText(editor->toPlainText());
		copySocial->setText(text(strings::kClipsCaptionCopied));
	});
	QObject::connect(copyShorts, &QPushButton::clicked, dialog, [this, shortsEditor, copyShorts] {
		QApplication::clipboard()->setText(shortsEditor->toPlainText());
		copyShorts->setText(text(strings::kClipsCaptionCopied));
	});
	QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::reject);
	layout->addWidget(buttons);
	dialog->open();
}

void ClipsTab::confirmAndDelete(std::vector<std::string> clipIds, const QString &clipTitle)
{
	if (clipIds.empty())
		return;
	const auto count = static_cast<int>(clipIds.size());
	const auto message =
		count == 1 ? text(strings::kClipsDeleteOneConfirm)
				     .arg(clipTitle.isEmpty() ? QString::fromStdString(clipIds.front()) : clipTitle)
			   : text(strings::kClipsDeleteManyConfirm).arg(count);
	const auto answer = QMessageBox::warning(this, text(strings::kClipsDeleteConfirmTitle), message,
						 QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
	if (answer != QMessageBox::Yes)
		return;

	batchDeleteButton_->setEnabled(false);
	const auto idsForCallback = clipIds;
	controller_->deleteClips(std::move(clipIds), [this, clipIds = idsForCallback](storage::StorageStatus status) {
		if (status.success) {
			for (const auto &id : clipIds) {
				selectedClipIds_.erase(std::remove(selectedClipIds_.begin(), selectedClipIds_.end(),
								   id),
						       selectedClipIds_.end());
			}
			errorLabel_->setProperty("notificationTone", QStringLiteral("success"));
			errorLabel_->style()->unpolish(errorLabel_);
			errorLabel_->style()->polish(errorLabel_);
			errorLabel_->setText(text(strings::kClipsDeleteSuccess).arg(static_cast<int>(clipIds.size())));
			errorLabel_->show();
			QTimer::singleShot(3500, errorLabel_, &QWidget::hide);
		} else {
			errorLabel_->setProperty("notificationTone", QStringLiteral("error"));
			errorLabel_->style()->unpolish(errorLabel_);
			errorLabel_->style()->polish(errorLabel_);
			errorLabel_->setText(
				text(strings::kClipsDeleteFailed).arg(QString::fromStdString(status.error)));
			errorLabel_->show();
		}
		updateSelectionActions();
	});
}

void ClipsTab::updateSelectionActions()
{
	const bool hasSelection = !selectedClipIds_.empty();
	if (batchDeleteButton_ != nullptr)
		batchDeleteButton_->setEnabled(hasSelection);
	if (batchExportButton_ != nullptr) {
		const bool batchAllowed = featureGates_ != nullptr && featureGates_->isAllowed(Feature::BatchExport);
		batchExportButton_->setEnabled(batchAllowed && hasSelection);
	}
}

void ClipsTab::refreshExportProgress()
{
	if (exportManager_ == nullptr) {
		return;
	}
	const auto jobs = exportManager_->jobs();
	const ExportJob *active = nullptr;
	const ExportJob *latest = nullptr;
	for (const auto &job : jobs) {
		if (latest == nullptr || job.updatedAt > latest->updatedAt) {
			latest = &job;
		}
		if (job.state == ExportJobState::Exporting || job.state == ExportJobState::Pending) {
			active = &job;
			break;
		}
	}
	const auto *visible = active != nullptr ? active : latest;
	if (visible == nullptr) {
		exportProgress_->hide();
		exportStatusLabel_->hide();
		cancelExportButton_->setEnabled(false);
		return;
	}
	exportProgress_->setValue(visible->progressPercent);
	exportProgress_->show();
	cancelExportButton_->setEnabled(active != nullptr);
	if (visible->state == ExportJobState::Error) {
		exportStatusLabel_->setText(
			text(strings::kClipsExportFailed).arg(QString::fromStdString(visible->error)));
	} else if (visible->state == ExportJobState::Done) {
		exportStatusLabel_->setText(
			text(strings::kClipsExportDone).arg(QString::fromStdString(visible->outputPath.u8string())));
	} else if (visible->state == ExportJobState::Cancelled) {
		exportStatusLabel_->setText(text(strings::kClipsExportCancelled));
	} else {
		exportStatusLabel_->setText(text(strings::kClipsExportProgress).arg(visible->progressPercent));
	}
	exportStatusLabel_->show();
	if ((visible->state == ExportJobState::Done || visible->state == ExportJobState::Error ||
	     visible->state == ExportJobState::Cancelled) &&
	    std::find(handledTerminalExportIds_.begin(), handledTerminalExportIds_.end(), visible->id) ==
		    handledTerminalExportIds_.end()) {
		handledTerminalExportIds_.push_back(visible->id);
		reload();
	}
}

void ClipsTab::clearCards()
{
	while (auto *item = listLayout_->takeAt(0)) {
		if (auto *widget = item->widget()) {
			widget->deleteLater();
		}
		delete item;
	}
}

void ClipsTab::showActionResult(const ClipActionResult &result, const char *errorKey)
{
	if (result.success) {
		errorLabel_->hide();
		return;
	}
	auto message = text(errorKey);
	if (message.contains(QStringLiteral("%1"))) {
		message = message.arg(QString::fromStdString(result.error));
	} else if (!result.error.empty()) {
		message += QStringLiteral(" ") + QString::fromStdString(result.error);
	}
	errorLabel_->setText(message);
	errorLabel_->show();
}

QString ClipsTab::triggerText(TriggerType trigger) const
{
	switch (trigger) {
	case TriggerType::Voice:
		return text(strings::kTriggerVoiceType);
	case TriggerType::AudioSpike:
		return text(strings::kTriggerAudioSpike);
	case TriggerType::Chat:
		return text(strings::kTriggerChat);
	case TriggerType::Scene:
		return text(strings::kTriggerSceneType);
	case TriggerType::Ai:
		return text(strings::kTriggerAi);
	case TriggerType::Manual:
		return text(strings::kTriggerManual);
	}
	return {};
}

} // namespace clipcoach::ui
