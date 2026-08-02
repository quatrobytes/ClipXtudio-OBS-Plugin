#include <clipcoach/ui/vertical-tab.hpp>

#include <clipcoach/core/settings.hpp>
#include <clipcoach/ui/components/primary-button.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/components/settings-section.hpp>
#include <clipcoach/ui/components/wheel-safe-controls.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/ui-strings.hpp>
#include <clipcoach/ui/vertical-preview-widget.hpp>

#include <QBoxLayout>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>

namespace clipcoach::ui {
VerticalTab::VerticalTab(TranslationFunction translator, VerticalCanvasManager *manager, VerticalObsBridge obsBridge,
			 QWidget *parent)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  manager_(manager),
	  obsBridge_(std::move(obsBridge))
{
	setObjectName(QStringLiteral("verticalTab"));
	build();
	bind();
	refresh();
}

QString VerticalTab::text(const char *key) const
{
	return translator_ ? translator_(key) : QString::fromUtf8(key);
}

void VerticalTab::build()
{
	constexpr int kHeaderActionHeight = 44;

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);
	auto *scroll = new QScrollArea(this);
	scroll->setObjectName(QStringLiteral("verticalCanvasScroll"));
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *content = new QWidget(scroll);
	content->setObjectName(QStringLiteral("verticalCanvasContent"));
	content->setMinimumWidth(0);
	content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	auto *layout = new QVBoxLayout(content);
	layout->setContentsMargins(tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin);
	layout->setSpacing(tokens::kSectionGap);
	layout->setAlignment(Qt::AlignTop);

	pageHeader_ = new QFrame(content);
	pageHeader_->setObjectName(QStringLiteral("verticalPageHeader"));
	pageHeader_->setProperty("verticalRole", QStringLiteral("header"));
	pageHeader_->setProperty("pageRole", QStringLiteral("header"));
	headerLayout_ = new QBoxLayout(QBoxLayout::LeftToRight, pageHeader_);
	headerLayout_->setContentsMargins(0, tokens::kSpaceSm, 0, tokens::kSpaceSm);
	headerLayout_->setSpacing(tokens::kSpaceXl);
	auto *headerIdentity = new QWidget(pageHeader_);
	headerIdentity->setObjectName(QStringLiteral("verticalHeaderIdentity"));
	auto *identityLayout = new QHBoxLayout(headerIdentity);
	identityLayout->setContentsMargins(0, 0, 0, 0);
	identityLayout->setSpacing(tokens::kSpaceLg);
	auto *headerIcon = new QLabel(QStringLiteral("▯"), pageHeader_);
	headerIcon->setObjectName(QStringLiteral("verticalHeaderIcon"));
	headerIcon->setProperty("pageRole", QStringLiteral("icon"));
	headerIcon->setAlignment(Qt::AlignCenter);
	headerIcon->setFixedSize(48, 48);
	auto *headerCopy = new QVBoxLayout();
	headerCopy->setSpacing(tokens::kSpaceXs);
	auto *title = new QLabel(text(strings::kVerticalTitle), pageHeader_);
	title->setObjectName(QStringLiteral("PageTitle"));
	auto *subtitle = new QLabel(text(strings::kVerticalSubtitle), pageHeader_);
	subtitle->setWordWrap(true);
	subtitle->setObjectName(QStringLiteral("PageSubtitle"));
	headerCopy->addWidget(title);
	headerCopy->addWidget(subtitle);
	identityLayout->addWidget(headerIcon);
	identityLayout->addLayout(headerCopy, 1);

	auto *headerActionBlock = new QWidget(pageHeader_);
	headerActionBlock->setObjectName(QStringLiteral("verticalHeaderActionBlock"));
	auto *headerActionBlockLayout = new QVBoxLayout(headerActionBlock);
	headerActionBlockLayout->setContentsMargins(0, 0, 0, 0);
	headerActionBlockLayout->setSpacing(tokens::kSpaceSm);
	headerActionsLayout_ = new QBoxLayout(QBoxLayout::LeftToRight);
	headerActionsLayout_->setContentsMargins(0, 0, 0, 0);
	headerActionsLayout_->setSpacing(tokens::kSpaceMd);
	startReplayButton_ = new PrimaryButton(text(strings::kVerticalStartReplay), pageHeader_);
	startReplayButton_->setObjectName(QStringLiteral("verticalStartReplayButton"));
	startReplayButton_->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	startReplayButton_->setFixedWidth(194);
	startReplayButton_->setFixedHeight(kHeaderActionHeight);
	saveClipButton_ = new QPushButton(text(strings::kVerticalSaveClip), pageHeader_);
	saveClipButton_->setObjectName(QStringLiteral("verticalSaveClipButton"));
	saveClipButton_->setProperty("controlRole", QStringLiteral("secondary"));
	saveClipButton_->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
	saveClipButton_->setFixedWidth(142);
	saveClipButton_->setFixedHeight(kHeaderActionHeight);
	createSceneButton_ = new QPushButton(text(strings::kVerticalCreateScene), pageHeader_);
	createSceneButton_->setObjectName(QStringLiteral("verticalCreateSceneButton"));
	createSceneButton_->setProperty("controlRole", QStringLiteral("secondary"));
	createSceneButton_->setIcon(style()->standardIcon(QStyle::SP_FileDialogNewFolder));
	createSceneButton_->setFixedWidth(184);
	createSceneButton_->setFixedHeight(kHeaderActionHeight);
	verticalStateBadge_ = new QLabel(text(strings::kVerticalInactiveState), pageHeader_);
	verticalStateBadge_->setObjectName(QStringLiteral("verticalActiveBadge"));
	verticalStateBadge_->setAlignment(Qt::AlignCenter);
	verticalStateBadge_->setFixedWidth(132);
	verticalStateBadge_->setFixedHeight(kHeaderActionHeight);
	headerActionsLayout_->addWidget(startReplayButton_, 0, Qt::AlignVCenter);
	headerActionsLayout_->addWidget(saveClipButton_, 0, Qt::AlignVCenter);
	headerActionsLayout_->addWidget(createSceneButton_, 0, Qt::AlignVCenter);
	headerActionsLayout_->addWidget(verticalStateBadge_, 0, Qt::AlignVCenter);

	auto *saveStateRow = new QWidget(headerActionBlock);
	saveStateRow->setObjectName(QStringLiteral("verticalSaveStateRow"));
	auto *saveStateLayout = new QHBoxLayout(saveStateRow);
	saveStateLayout->setContentsMargins(0, 0, tokens::kSpaceSm, 0);
	saveStateLayout->setSpacing(tokens::kSpaceSm);
	saveStateLayout->addStretch(1);
	auto *savedIcon = new QLabel(QString(QChar(0x2713)), saveStateRow);
	savedIcon->setObjectName(QStringLiteral("verticalSavedIcon"));
	savedIcon->setAlignment(Qt::AlignCenter);
	auto *autoSaved = new QLabel(text(strings::kVerticalAutoSaved), saveStateRow);
	autoSaved->setObjectName(QStringLiteral("verticalAutoSavedLabel"));
	auto *separator = new QLabel(QStringLiteral("|"), saveStateRow);
	separator->setObjectName(QStringLiteral("verticalSavedSeparator"));
	auto *savedNow = new QLabel(text(strings::kVerticalSavedNow), saveStateRow);
	savedNow->setObjectName(QStringLiteral("verticalSavedNowLabel"));
	saveStateLayout->addWidget(savedIcon);
	saveStateLayout->addWidget(autoSaved);
	saveStateLayout->addWidget(separator);
	saveStateLayout->addWidget(savedNow);

	headerActionBlockLayout->addLayout(headerActionsLayout_);
	headerActionBlockLayout->addWidget(saveStateRow);
	headerLayout_->addWidget(headerIdentity, 1, Qt::AlignVCenter);
	headerLayout_->addWidget(headerActionBlock, 0, Qt::AlignRight | Qt::AlignVCenter);
	layout->addWidget(pageHeader_);
	widthHint_ = new QLabel(text(strings::kVerticalWidthHint), content);
	widthHint_->setObjectName(QStringLiteral("verticalWidthHint"));
	widthHint_->setProperty("notificationTone", QStringLiteral("info"));
	widthHint_->setWordWrap(true);
	widthHint_->hide();
	layout->addWidget(widthHint_);

	auto *workflow = new SettingsSection(text(strings::kVerticalWorkflowTitle), content);
	workflow->setObjectName(QStringLiteral("verticalWorkflowHelp"));
	auto *workflowBody = new QLabel(text(strings::kVerticalWorkflowBody), workflow);
	workflowBody->setObjectName(QStringLiteral("verticalWorkflowDescription"));
	workflowBody->setProperty("class", QStringLiteral("supporting"));
	workflowBody->setWordWrap(true);
	workflow->contentLayout()->addWidget(workflowBody);
	workflow->hide();
	layout->addWidget(workflow);

	bodyLayout_ = new QBoxLayout(QBoxLayout::LeftToRight);
	bodyLayout_->setSpacing(tokens::kSpaceLg);
	previewColumn_ = new QWidget(content);
	previewColumn_->setObjectName(QStringLiteral("verticalPreviewColumn"));
	previewColumn_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	previewColumn_->setMinimumWidth(0);
	auto *previewColumnLayout = new QVBoxLayout(previewColumn_);
	previewColumnLayout->setContentsMargins(0, 0, 0, 0);
	previewColumnLayout->setSpacing(tokens::kSpaceMd);

	compactActionBar_ = new QFrame(previewColumn_);
	compactActionBar_->setObjectName(QStringLiteral("verticalCompactActionBar"));
	auto *compactActions = new QHBoxLayout(compactActionBar_);
	compactActions->setContentsMargins(tokens::kSpaceSm, tokens::kSpaceSm, tokens::kSpaceSm, tokens::kSpaceSm);
	compactActions->setSpacing(tokens::kSpaceSm);
	const auto compactButton = [this](const QString &name, QStyle::StandardPixmap icon, const QString &toolTip) {
		auto *button = new QPushButton(compactActionBar_);
		button->setObjectName(name);
		button->setProperty("controlRole", QStringLiteral("compactIcon"));
		button->setIcon(style()->standardIcon(icon));
		button->setToolTip(toolTip);
		button->setAccessibleName(toolTip);
		button->setFixedSize(40, 40);
		return button;
	};
	compactReplayButton_ = compactButton(QStringLiteral("verticalCompactReplayButton"), QStyle::SP_MediaPlay,
					     text(strings::kVerticalStartReplay));
	compactSaveButton_ = compactButton(QStringLiteral("verticalCompactSaveButton"), QStyle::SP_DialogSaveButton,
					   text(strings::kVerticalSaveClip));
	compactCreateSceneButton_ = compactButton(QStringLiteral("verticalCompactCreateSceneButton"),
						  QStyle::SP_FileDialogNewFolder, text(strings::kVerticalCreateScene));
	compactActions->addWidget(compactReplayButton_);
	compactActions->addWidget(compactSaveButton_);
	compactActions->addWidget(compactCreateSceneButton_);
	compactActions->addStretch(1);
	compactActionBar_->hide();
	if (obsBridge_.createPreview) {
		preview_ = obsBridge_.createPreview(previewColumn_);
	}
	if (preview_ == nullptr) {
		fallbackPreview_ = new VerticalPreviewWidget(previewColumn_);
		preview_ = fallbackPreview_;
	}
	preview_->setObjectName(QStringLiteral("verticalCanvasPreview"));
	preview_->setMinimumSize(234, 416);
	preview_->setMaximumSize(315, 560);
	preview_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
	auto *previewCard = new QFrame(previewColumn_);
	previewCard->setObjectName(QStringLiteral("verticalPreviewCard"));
	previewCard->setProperty("verticalRole", QStringLiteral("card"));
	auto *previewCardLayout = new QVBoxLayout(previewCard);
	previewCardLayout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd);
	previewCardLayout->setSpacing(tokens::kSpaceSm);
	auto *previewHeading = new QHBoxLayout();
	auto *previewTitle = new QLabel(text(strings::kVerticalPreviewTitle), previewCard);
	previewTitle->setObjectName(QStringLiteral("verticalPreviewTitle"));
	auto *previewRatio = new QLabel(text(strings::kVerticalPreviewRatio), previewCard);
	previewRatio->setObjectName(QStringLiteral("verticalPreviewRatio"));
	previewRatio->setAlignment(Qt::AlignCenter);
	previewHeading->addWidget(previewTitle);
	previewHeading->addStretch(1);
	previewOnlyButton_ = new QPushButton(previewCard);
	previewOnlyButton_->setObjectName(QStringLiteral("verticalPreviewOnlyButton"));
	previewOnlyButton_->setProperty("controlRole", QStringLiteral("compactIcon"));
	previewOnlyButton_->setCheckable(true);
	previewOnlyButton_->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
	previewOnlyButton_->setToolTip(text(strings::kVerticalPreviewOnly));
	previewOnlyButton_->setAccessibleName(text(strings::kVerticalPreviewOnly));
	previewOnlyButton_->setFixedSize(34, 30);
	previewHeading->addWidget(previewOnlyButton_);
	previewHeading->addWidget(previewRatio);
	previewCardLayout->addLayout(previewHeading);
	previewCardLayout->addWidget(preview_, 0, Qt::AlignTop | Qt::AlignHCenter);
	previewColumnLayout->addWidget(previewCard);
	previewColumnLayout->addWidget(compactActionBar_);

	bodyLayout_->addWidget(previewColumn_, 4);

	auto *controls = new SettingsSection(text(strings::kVerticalSettings), content);
	controls->setObjectName(QStringLiteral("verticalLegacySettingsSection"));
	controls->setMinimumWidth(0);
	// Keep the localized extended explanation available to accessibility/tests
	// without repeating the legacy technical paragraph in the visual hierarchy.
	auto *settingsDescription = new QLabel(text(strings::kVerticalSettingsDescription), controls);
	settingsDescription->setObjectName(QStringLiteral("verticalSettingsDescription"));
	settingsDescription->setAccessibleName(text(strings::kVerticalSettingsDescription));
	settingsDescription->hide();
	controls->contentLayout()->addWidget(settingsDescription);

	auto *form = new QFormLayout();
	form->setContentsMargins(0, 0, 0, 0);
	form->setHorizontalSpacing(tokens::kSpaceLg);
	form->setVerticalSpacing(tokens::kSpaceMd);
	form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	form->setRowWrapPolicy(QFormLayout::WrapLongRows);
	form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);

	const auto addFormSection = [form, controls](const QString &titleText, const QString &descriptionText,
						     const QString &objectName) {
		auto *section = new QWidget(controls);
		section->setObjectName(objectName);
		section->setProperty("verticalRole", QStringLiteral("formSection"));
		auto *sectionLayout = new QVBoxLayout(section);
		sectionLayout->setContentsMargins(0, tokens::kSpaceSm, 0, tokens::kSpaceXs);
		sectionLayout->setSpacing(tokens::kSpaceXs);
		auto *heading = new QLabel(titleText, section);
		heading->setProperty("verticalRole", QStringLiteral("sectionTitle"));
		auto *description = new QLabel(descriptionText, section);
		description->setProperty("class", QStringLiteral("supporting"));
		description->setWordWrap(true);
		sectionLayout->addWidget(heading);
		sectionLayout->addWidget(description);
		form->addRow(section);
	};

	addFormSection(text(strings::kVerticalCompositionTitle), text(strings::kVerticalCompositionDescription),
		       QStringLiteral("verticalCompositionHeading"));
	sceneCombo_ = new WheelSafeComboBox(controls);
	sceneCombo_->setObjectName(QStringLiteral("verticalObsSceneCombo"));
	sceneCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	sceneCombo_->setMinimumContentsLength(12);
	form->addRow(text(strings::kVerticalObsScene), sceneCombo_);
	form->labelForField(sceneCombo_)->setObjectName(QStringLiteral("SettingsFieldLabel"));

	sourceCombo_ = new WheelSafeComboBox(controls);
	sourceCombo_->setObjectName(QStringLiteral("verticalObsSourceCombo"));
	sourceCombo_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	sourceCombo_->setMinimumContentsLength(12);
	form->addRow(text(strings::kVerticalObsSource), sourceCombo_);
	form->labelForField(sourceCombo_)->setObjectName(QStringLiteral("SettingsFieldLabel"));

	outputMode_ = new WheelSafeComboBox(controls);
	outputMode_->setObjectName(QStringLiteral("verticalOutputModeCombo"));
	outputMode_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	outputMode_->setMinimumContentsLength(12);
	outputMode_->addItems(
		{text(strings::kSettingsHorizontal), text(strings::kSettingsVertical), text(strings::kSettingsBoth)});
	form->addRow(text(strings::kVerticalOutputMode), outputMode_);
	form->labelForField(outputMode_)->setObjectName(QStringLiteral("SettingsFieldLabel"));

	addFormSection(text(strings::kVerticalCanvasTitle), text(strings::kVerticalCanvasDescription),
		       QStringLiteral("verticalCanvasHeading"));
	resolution_ = new WheelSafeComboBox(controls);
	resolution_->setObjectName(QStringLiteral("verticalCanvasResolutionCombo"));
	resolution_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	resolution_->setMinimumContentsLength(12);
	resolution_->addItem(QStringLiteral("1080p · 1080 × 1920"), static_cast<int>(VerticalResolution::Portrait1080));
	resolution_->addItem(QStringLiteral("2K · 1440 × 2560"), static_cast<int>(VerticalResolution::Portrait1440));
	resolution_->addItem(QStringLiteral("4K · 2160 × 3840"), static_cast<int>(VerticalResolution::Portrait2160));
	resolution_->addItem(QStringLiteral("8K · 4320 × 7680"), static_cast<int>(VerticalResolution::Portrait4320));
	resolution_->addItem(QStringLiteral("720p · 720 × 1280"), static_cast<int>(VerticalResolution::Portrait720));
	resolution_->addItem(text(strings::kVerticalCustom), static_cast<int>(VerticalResolution::Custom));
	form->addRow(text(strings::kVerticalResolution), resolution_);
	form->labelForField(resolution_)->setObjectName(QStringLiteral("SettingsFieldLabel"));

	auto *custom = new QWidget(controls);
	auto *customLayout = new QHBoxLayout(custom);
	customLayout->setContentsMargins(0, 0, 0, 0);
	customWidth_ = new WheelSafeSpinBox(custom);
	customWidth_->setObjectName(QStringLiteral("verticalCustomWidth"));
	customWidth_->setRange(360, settings_constraints::kPortrait4320Width);
	customWidth_->setSingleStep(9);
	customHeight_ = new WheelSafeSpinBox(custom);
	customHeight_->setObjectName(QStringLiteral("verticalCustomHeight"));
	customHeight_->setRange(640, settings_constraints::kPortrait4320Height);
	customHeight_->setSingleStep(16);
	customLayout->addWidget(customWidth_);
	customLayout->addWidget(new QLabel(QStringLiteral("×"), custom));
	customLayout->addWidget(customHeight_);
	form->addRow(text(strings::kVerticalCustomSize), custom);
	form->labelForField(custom)->setObjectName(QStringLiteral("SettingsFieldLabel"));

	zoom_ = new WheelSafeSpinBox(controls);
	zoom_->setObjectName(QStringLiteral("verticalZoom"));
	zoom_->setRange(100, 300);
	zoom_->setSuffix(QStringLiteral("%"));
	zoom_->setToolTip(text(strings::kVerticalControlsHelp));
	form->addRow(text(strings::kVerticalZoom), zoom_);

	addFormSection(text(strings::kVerticalPositionTitle), text(strings::kVerticalPositionDescription),
		       QStringLiteral("verticalPositionHeading"));
	panX_ = new WheelSafeSpinBox(controls);
	panX_->setObjectName(QStringLiteral("verticalPanX"));
	panX_->setRange(-100, 100);
	panX_->setSuffix(QStringLiteral("%"));
	panX_->setToolTip(text(strings::kVerticalControlsHelp));
	form->addRow(text(strings::kVerticalPanX), panX_);
	panY_ = new WheelSafeSpinBox(controls);
	panY_->setObjectName(QStringLiteral("verticalPanY"));
	panY_->setRange(-100, 100);
	panY_->setSuffix(QStringLiteral("%"));
	panY_->setToolTip(text(strings::kVerticalControlsHelp));
	form->addRow(text(strings::kVerticalPanY), panY_);

	controls->contentLayout()->addLayout(form);
	controls->hide();

	// Present the existing, already-bound controls in purpose-specific cards.
	// Reparenting keeps every manager binding intact while matching the visual
	// hierarchy of the product reference.
	auto *controlsColumn = new QWidget(content);
	controlsColumn->setObjectName(QStringLiteral("verticalCanvasSettingsSection"));
	controlsColumn->setMinimumWidth(0);
	controlsSection_ = controlsColumn;
	auto *controlsColumnLayout = new QVBoxLayout(controlsColumn);
	controlsColumnLayout->setContentsMargins(0, 0, 0, 0);
	controlsColumnLayout->setSpacing(tokens::kSpaceMd);

	const auto createCard = [controlsColumn](const QString &titleText, const QString &objectName) {
		auto *card = new QFrame(controlsColumn);
		card->setObjectName(objectName);
		card->setProperty("verticalRole", QStringLiteral("controlCard"));
		auto *cardRoot = new QVBoxLayout(card);
		cardRoot->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceMd);
		cardRoot->setSpacing(tokens::kSpaceMd);
		auto *titleLabel = new QLabel(titleText, card);
		titleLabel->setProperty("verticalRole", QStringLiteral("sectionTitle"));
		cardRoot->addWidget(titleLabel);
		return card;
	};
	const auto cardRoot = [](QFrame *card) {
		return qobject_cast<QVBoxLayout *>(card->layout());
	};
	const auto field = [](const QString &labelText, QWidget *control, QWidget *parent) {
		auto *holder = new QWidget(parent);
		auto *holderLayout = new QVBoxLayout(holder);
		holderLayout->setContentsMargins(0, 0, 0, 0);
		holderLayout->setSpacing(tokens::kSpaceXs);
		auto *label = new QLabel(labelText, holder);
		label->setObjectName(QStringLiteral("SettingsFieldLabel"));
		holderLayout->addWidget(label);
		holderLayout->addWidget(control);
		return holder;
	};

	auto *compositionCard = createCard(QStringLiteral("◈  ") + text(strings::kVerticalCompositionTitle),
					   QStringLiteral("verticalCompositionCard"));
	auto *compositionGrid = new QGridLayout();
	compositionGrid->setContentsMargins(0, 0, 0, 0);
	compositionGrid->setHorizontalSpacing(tokens::kSpaceLg);
	compositionGrid->setVerticalSpacing(tokens::kSpaceMd);
	compositionGrid->setColumnStretch(0, 1);
	compositionGrid->setColumnStretch(1, 1);
	compositionGrid->addWidget(field(text(strings::kVerticalObsScene), sceneCombo_, compositionCard), 0, 0);
	compositionGrid->addWidget(field(text(strings::kVerticalObsSource), sourceCombo_, compositionCard), 0, 1);
	compositionGrid->addWidget(field(text(strings::kVerticalOutputMode), outputMode_, compositionCard), 1, 0);
	cardRoot(compositionCard)->addLayout(compositionGrid);
	compositionSection_ = compositionCard;
	previewColumnLayout->addWidget(compositionCard);

	auto *canvasCard = createCard(QStringLiteral("▱  ") + text(strings::kVerticalCanvasTitle),
				      QStringLiteral("verticalCanvasCard"));
	auto *canvasGrid = new QGridLayout();
	canvasGrid->setContentsMargins(0, 0, 0, 0);
	canvasGrid->setHorizontalSpacing(tokens::kSpaceLg);
	canvasGrid->setVerticalSpacing(tokens::kSpaceMd);
	canvasGrid->setColumnStretch(0, 1);
	canvasGrid->setColumnStretch(1, 1);
	canvasGrid->addWidget(field(text(strings::kVerticalResolution), resolution_, canvasCard), 0, 0);
	canvasGrid->addWidget(field(text(strings::kVerticalCustomSize), custom, canvasCard), 0, 1);
	cardRoot(canvasCard)->addLayout(canvasGrid);
	auto *zoomRow = new QWidget(canvasCard);
	auto *zoomRowLayout = new QHBoxLayout(zoomRow);
	zoomRowLayout->setContentsMargins(0, 0, 0, 0);
	zoomRowLayout->setSpacing(tokens::kSpaceMd);
	auto *zoomLabel = new QLabel(text(strings::kVerticalZoom), zoomRow);
	zoomLabel->setObjectName(QStringLiteral("SettingsFieldLabel"));
	zoomSlider_ = new WheelSafeSlider(Qt::Horizontal, zoomRow);
	zoomSlider_->setObjectName(QStringLiteral("verticalZoomSlider"));
	zoomSlider_->setRange(100, 300);
	zoom_->setFixedWidth(88);
	zoomRowLayout->addWidget(zoomLabel);
	zoomRowLayout->addWidget(zoomSlider_, 1);
	zoomRowLayout->addWidget(zoom_);
	cardRoot(canvasCard)->addWidget(zoomRow);

	auto *positionCard = createCard(QStringLiteral("✥  ") + text(strings::kVerticalPositionTitle),
					QStringLiteral("verticalPositionCard"));
	const auto addPositionRow = [this, positionCard, cardRoot](const QString &labelText, QSlider **slider,
								   QSpinBox *spin, const QString &objectName) {
		auto *row = new QWidget(positionCard);
		auto *rowLayout = new QHBoxLayout(row);
		rowLayout->setContentsMargins(0, 0, 0, 0);
		rowLayout->setSpacing(tokens::kSpaceMd);
		auto *label = new QLabel(labelText, row);
		label->setObjectName(QStringLiteral("SettingsFieldLabel"));
		label->setMinimumWidth(130);
		*slider = new WheelSafeSlider(Qt::Horizontal, row);
		(*slider)->setObjectName(objectName);
		(*slider)->setRange(-100, 100);
		spin->setFixedWidth(88);
		rowLayout->addWidget(label);
		rowLayout->addWidget(*slider, 1);
		rowLayout->addWidget(spin);
		cardRoot(positionCard)->addWidget(row);
	};
	addPositionRow(text(strings::kVerticalPanX), &panXSlider_, panX_, QStringLiteral("verticalPanXSlider"));
	addPositionRow(text(strings::kVerticalPanY), &panYSlider_, panY_, QStringLiteral("verticalPanYSlider"));

	auto *framingContainer = new QWidget(controlsColumn);
	framingContainer->setObjectName(QStringLiteral("verticalFramingControls"));
	framingLayout_ = new QBoxLayout(QBoxLayout::LeftToRight, framingContainer);
	framingLayout_->setContentsMargins(0, 0, 0, 0);
	framingLayout_->setSpacing(tokens::kSpaceMd);
	framingLayout_->addWidget(canvasCard, 1);
	framingLayout_->addWidget(positionCard, 1);
	controlsColumnLayout->addWidget(framingContainer);
	controlsColumnLayout->addStretch(1);

	bodyLayout_->addWidget(controlsColumn, 7);
	layout->addLayout(bodyLayout_);

	message_ = new QLabel(content);
	message_->setObjectName(QStringLiteral("verticalCanvasMessage"));
	message_->setWordWrap(true);
	message_->setProperty("class", QStringLiteral("supporting"));
	layout->addWidget(message_);
	layout->addStretch(1);
	scroll->setWidget(content);
	root->addWidget(scroll);
	updateResponsiveLayout();
}

void VerticalTab::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateResponsiveLayout();
}

void VerticalTab::updateResponsiveLayout()
{
	if (bodyLayout_ == nullptr)
		return;
	const bool compact = width() < 920;
	compactLayout_ = compact;

	if (pageHeader_ != nullptr)
		pageHeader_->setVisible(!compact && !previewOnly_);
	if (compactActionBar_ != nullptr)
		compactActionBar_->setVisible(compact && !previewOnly_);
	if (previewOnlyButton_ != nullptr)
		previewOnlyButton_->setVisible(true);
	if (compositionSection_ != nullptr)
		compositionSection_->setVisible(!previewOnly_);
	if (controlsSection_ != nullptr)
		controlsSection_->setVisible(!previewOnly_);
	if (framingLayout_ != nullptr)
		framingLayout_->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
	if (widthHint_ != nullptr)
		widthHint_->hide();

	bodyLayout_->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
	bodyLayout_->setStretch(0, compact ? 0 : 4);
	bodyLayout_->setStretch(1, compact ? 0 : 7);

	if (preview_ != nullptr) {
		const int rawWidth = previewOnly_ ? std::clamp(width() - 96, 315, 540)
				     : compact    ? std::clamp(width() - 80, 234, 315)
						  : std::clamp(static_cast<int>(width() * 0.22), 252, 306);
		const int desiredWidth = std::max(234, (rawWidth / 9) * 9);
		preview_->setFixedSize(desiredWidth, (desiredWidth / 9) * 16);
	}
	syncCompactActions();
}

void VerticalTab::setPreviewOnly(bool enabled)
{
	previewOnly_ = enabled;
	if (previewOnlyButton_ != nullptr) {
		if (previewOnlyButton_->isChecked() != enabled) {
			const QSignalBlocker blocker(previewOnlyButton_);
			previewOnlyButton_->setChecked(enabled);
		}
		previewOnlyButton_->setIcon(style()->standardIcon(enabled ? QStyle::SP_TitleBarNormalButton
									  : QStyle::SP_TitleBarMaxButton));
		const auto label = text(enabled ? strings::kVerticalPreviewRestore : strings::kVerticalPreviewOnly);
		previewOnlyButton_->setToolTip(label);
		previewOnlyButton_->setAccessibleName(label);
	}
	if (message_ != nullptr)
		message_->setVisible(!enabled && !message_->text().isEmpty());
	updateResponsiveLayout();
}

void VerticalTab::syncCompactActions()
{
	if (compactReplayButton_ != nullptr && startReplayButton_ != nullptr) {
		compactReplayButton_->setEnabled(startReplayButton_->isEnabled());
		compactReplayButton_->setIcon(
			style()->standardIcon(replayActive_ ? QStyle::SP_MediaStop : QStyle::SP_MediaPlay));
	}
	if (compactSaveButton_ != nullptr && saveClipButton_ != nullptr)
		compactSaveButton_->setEnabled(saveClipButton_->isEnabled());
	if (compactCreateSceneButton_ != nullptr && createSceneButton_ != nullptr)
		compactCreateSceneButton_->setEnabled(createSceneButton_->isEnabled());
}

void VerticalTab::bind()
{
	connect(startReplayButton_, &QPushButton::clicked, this, [this] {
		if (startReplayCallback_)
			startReplayCallback_();
	});
	connect(saveClipButton_, &QPushButton::clicked, this, [this] {
		if (saveClipCallback_)
			saveClipCallback_();
	});
	connect(compactReplayButton_, &QPushButton::clicked, startReplayButton_, &QPushButton::click);
	connect(compactSaveButton_, &QPushButton::clicked, saveClipButton_, &QPushButton::click);
	connect(compactCreateSceneButton_, &QPushButton::clicked, createSceneButton_, &QPushButton::click);
	connect(previewOnlyButton_, &QPushButton::toggled, this, &VerticalTab::setPreviewOnly);
	if (obsBridge_.bindPreviewInteraction && preview_ != nullptr) {
		obsBridge_.bindPreviewInteraction(preview_, [this](int zoom, int panX, int panY) {
			if (manager_ == nullptr)
				return;
			std::string error;
			if (!manager_->setFraming(zoom, panX, panY, &error)) {
				showError(error);
				return;
			}
			const QSignalBlocker blockZoom(zoom_);
			const QSignalBlocker blockPanX(panX_);
			const QSignalBlocker blockPanY(panY_);
			zoom_->setValue(zoom);
			panX_->setValue(panX);
			panY_->setValue(panY);
			if (zoomSlider_ != nullptr)
				zoomSlider_->setValue(zoom);
			if (panXSlider_ != nullptr)
				panXSlider_->setValue(panX);
			if (panYSlider_ != nullptr)
				panYSlider_->setValue(panY);
			updateLivePreview();
		});
	}
	connect(sceneCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
		if (refreshing_)
			return;
		refreshObsSources();
		if (manager_ != nullptr) {
			std::string error;
			if (!manager_->setObsSelection(sceneCombo_->currentData().toString().toStdString(),
						       sourceCombo_->currentData().toString().toStdString(), &error))
				showError(error);
		}
		updateLivePreview();
	});
	connect(sourceCombo_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
		if (refreshing_)
			return;
		if (manager_ != nullptr) {
			std::string error;
			if (!manager_->setObsSelection(sceneCombo_->currentData().toString().toStdString(),
						       sourceCombo_->currentData().toString().toStdString(), &error))
				showError(error);
		}
		updateLivePreview();
	});
	connect(createSceneButton_, &QPushButton::clicked, this, [this] {
		if (!obsBridge_.createVerticalScene)
			return;
		std::string error;
		const auto scene = sceneCombo_->currentData().toString().toStdString();
		const auto source = sourceCombo_->currentData().toString().toStdString();
		if (!obsBridge_.createVerticalScene(scene, source, manager_->settings(), &error)) {
			showError(error);
			return;
		}
		refreshing_ = true;
		refreshObsScenes();
		refreshing_ = false;
		const auto created = sceneCombo_->findData(QStringLiteral("ClipXtudio Vertical"));
		if (created >= 0)
			sceneCombo_->setCurrentIndex(created);
	});
	const auto framingChanged = [this](int) {
		if (refreshing_ || manager_ == nullptr)
			return;
		std::string error;
		if (!manager_->setFraming(zoom_->value(), panX_->value(), panY_->value(), &error))
			showError(error);
		updateLivePreview();
	};
	if (zoomSlider_ != nullptr) {
		connect(zoomSlider_, &QSlider::valueChanged, zoom_, &QSpinBox::setValue);
		connect(zoom_, qOverload<int>(&QSpinBox::valueChanged), zoomSlider_, &QSlider::setValue);
	}
	if (panXSlider_ != nullptr) {
		connect(panXSlider_, &QSlider::valueChanged, panX_, &QSpinBox::setValue);
		connect(panX_, qOverload<int>(&QSpinBox::valueChanged), panXSlider_, &QSlider::setValue);
	}
	if (panYSlider_ != nullptr) {
		connect(panYSlider_, &QSlider::valueChanged, panY_, &QSpinBox::setValue);
		connect(panY_, qOverload<int>(&QSpinBox::valueChanged), panYSlider_, &QSlider::setValue);
	}
	connect(zoom_, qOverload<int>(&QSpinBox::valueChanged), this, framingChanged);
	connect(panX_, qOverload<int>(&QSpinBox::valueChanged), this, framingChanged);
	connect(panY_, qOverload<int>(&QSpinBox::valueChanged), this, framingChanged);
	connect(outputMode_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (refreshing_ || manager_ == nullptr) {
			return;
		}
		std::string error;
		if (!manager_->setOutputMode(static_cast<CaptureOutputMode>(index), &error)) {
			showError(error);
		}
		refresh();
	});
	connect(resolution_, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
		if (!refreshing_) {
			applyResolution();
		}
	});
	connect(customWidth_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
		if (!refreshing_ &&
		    resolution_->currentData().toInt() == static_cast<int>(VerticalResolution::Custom)) {
			applyResolution();
		}
	});
	connect(customHeight_, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
		if (!refreshing_ &&
		    resolution_->currentData().toInt() == static_cast<int>(VerticalResolution::Custom)) {
			applyResolution();
		}
	});
}

void VerticalTab::applyResolution()
{
	if (manager_ == nullptr) {
		return;
	}
	std::string error;
	const auto preset = static_cast<VerticalResolution>(resolution_->currentData().toInt());
	if (!manager_->setResolution(preset, customWidth_->value(), customHeight_->value(), &error)) {
		showError(error);
		return;
	}
	refresh();
}

void VerticalTab::refresh()
{
	if (manager_ == nullptr) {
		if (controlsSection_ != nullptr)
			controlsSection_->setEnabled(false);
		if (createSceneButton_ != nullptr)
			createSceneButton_->setEnabled(false);
		message_->setText(text(strings::kVerticalUnavailable));
		return;
	}
	if (controlsSection_ != nullptr)
		controlsSection_->setEnabled(true);
	refreshing_ = true;
	refreshObsScenes();
	const auto sceneIndex = sceneCombo_->findData(QString::fromStdString(manager_->selectedSceneName()));
	if (sceneIndex >= 0)
		sceneCombo_->setCurrentIndex(sceneIndex);
	refreshObsSources();
	const auto sourceIndex = sourceCombo_->findData(QString::fromStdString(manager_->selectedSourceName()));
	if (sourceIndex >= 0)
		sourceCombo_->setCurrentIndex(sourceIndex);
	const auto state = manager_->settings();
	zoom_->setValue(state.zoomPercent);
	panX_->setValue(state.panXPercent);
	panY_->setValue(state.panYPercent);
	outputMode_->setCurrentIndex(static_cast<int>(state.outputMode));
	const auto resolutionIndex = resolution_->findData(static_cast<int>(state.resolution));
	resolution_->setCurrentIndex(resolutionIndex < 0 ? 0 : resolutionIndex);
	customWidth_->setValue(state.width);
	customHeight_->setValue(state.height);
	customWidth_->setEnabled(state.resolution == VerticalResolution::Custom);
	customHeight_->setEnabled(state.resolution == VerticalResolution::Custom);
	if (fallbackPreview_ != nullptr)
		fallbackPreview_->setCanvasSettings(state);
	updateLivePreview();
	message_->setText(manager_->proUnlocked() ? text(strings::kVerticalProActive)
						  : text(strings::kVerticalFreeMode));
	if (verticalStateBadge_ != nullptr) {
		verticalStateBadge_->setText(text(strings::kVerticalActiveState));
		verticalStateBadge_->setProperty("verticalActive", true);
		verticalStateBadge_->style()->unpolish(verticalStateBadge_);
		verticalStateBadge_->style()->polish(verticalStateBadge_);
	}
	refreshing_ = false;
	notifyCanvasChanged();
}

void VerticalTab::refreshObsSceneOptions()
{
	if (manager_ == nullptr || sceneCombo_ == nullptr || sourceCombo_ == nullptr)
		return;
	const bool wasRefreshing = refreshing_;
	refreshing_ = true;
	refreshObsScenes();
	refreshObsSources();
	refreshing_ = wasRefreshing;
	updateLivePreview();
}

void VerticalTab::setCanvasChangedCallback(CanvasChangedCallback callback)
{
	canvasChangedCallback_ = std::move(callback);
	notifyCanvasChanged();
}

void VerticalTab::setCaptureActions(ActionCallback startReplay, ActionCallback saveClip)
{
	startReplayCallback_ = std::move(startReplay);
	saveClipCallback_ = std::move(saveClip);
	setReplayState(replayActive_, replayTransition_);
}

void VerticalTab::setReplayState(bool active, bool transition)
{
	replayActive_ = active;
	replayTransition_ = transition;
	if (startReplayButton_ != nullptr) {
		startReplayButton_->setEnabled(static_cast<bool>(startReplayCallback_) && !active && !transition);
	}
	if (saveClipButton_ != nullptr) {
		saveClipButton_->setEnabled(static_cast<bool>(saveClipCallback_) && active && !transition);
	}
	syncCompactActions();
	// The badge describes the configured vertical canvas, not the Replay
	// Buffer transport. Replay state is already represented by the action
	// buttons and the global OBS status card.
}

void VerticalTab::notifyCanvasChanged()
{
	if (manager_ != nullptr && canvasChangedCallback_)
		canvasChangedCallback_(manager_->settings());
}

void VerticalTab::refreshObsScenes()
{
	const auto selected = sceneCombo_->currentData().toString();
	sceneCombo_->clear();
	sceneCombo_->addItem(text(strings::kVerticalSelectScene), QString());
	if (obsBridge_.scenes) {
		for (const auto &scene : obsBridge_.scenes()) {
			sceneCombo_->addItem(QString::fromStdString(scene), QString::fromStdString(scene));
		}
	}
	auto index = selected.isEmpty() ? -1 : sceneCombo_->findData(selected);
	if (index < 0 && manager_ != nullptr)
		index = sceneCombo_->findData(QString::fromStdString(manager_->selectedSceneName()));
	if (index < 0 && obsBridge_.activeScene) {
		const auto active = obsBridge_.activeScene();
		index = sceneCombo_->findData(QString::fromStdString(active));
	}
	if (index < 0 && obsBridge_.sourcesForScene) {
		for (int candidate = 1; candidate < sceneCombo_->count(); ++candidate) {
			if (!obsBridge_.sourcesForScene(sceneCombo_->itemData(candidate).toString().toStdString())
				     .empty()) {
				index = candidate;
				break;
			}
		}
	}
	sceneCombo_->setCurrentIndex(index < 0 ? 0 : index);
	const bool available = obsBridge_.available() && sceneCombo_->count() > 1;
	sceneCombo_->setEnabled(available);
	sourceCombo_->setEnabled(available);
	createSceneButton_->setEnabled(available && static_cast<bool>(obsBridge_.createVerticalScene));
	syncCompactActions();
}

void VerticalTab::refreshObsSources()
{
	const auto selected = sourceCombo_->currentData().toString();
	sourceCombo_->clear();
	sourceCombo_->addItem(text(strings::kVerticalWholeScene), QString());
	const auto scene = sceneCombo_->currentData().toString().toStdString();
	if (!scene.empty() && obsBridge_.sourcesForScene) {
		for (const auto &source : obsBridge_.sourcesForScene(scene))
			sourceCombo_->addItem(QString::fromStdString(source), QString::fromStdString(source));
	}
	auto index = sourceCombo_->findData(selected);
	if (index < 0 && manager_ != nullptr)
		index = sourceCombo_->findData(QString::fromStdString(manager_->selectedSourceName()));
	sourceCombo_->setCurrentIndex(index < 0 ? 0 : index);
}

void VerticalTab::updateLivePreview()
{
	if (!obsBridge_.updatePreview || preview_ == nullptr || manager_ == nullptr)
		return;
	const auto source = sourceCombo_->currentData().toString();
	const auto scene = sceneCombo_->currentData().toString();
	obsBridge_.updatePreview(preview_, (source.isEmpty() ? scene : source).toStdString(), manager_->settings());
}

void VerticalTab::showError(const std::string &error)
{
	message_->setText(QString::fromStdString(error));
}

} // namespace clipcoach::ui
