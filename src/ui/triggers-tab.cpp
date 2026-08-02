#include <clipcoach/ui/triggers-tab.hpp>

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/trigger-engine.hpp>
#include <clipcoach/core/voice-trigger-controller.hpp>
#include <clipcoach/ui/components/context-help-button.hpp>
#include <clipcoach/ui/components/settings-section.hpp>
#include <clipcoach/ui/components/toggle-row.hpp>
#include <clipcoach/ui/components/upgrade-banner.hpp>
#include <clipcoach/ui/components/wheel-safe-controls.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QCheckBox>
#include <QBoxLayout>
#include <QComboBox>
#include <QDateTime>
#include <QFrame>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <sstream>

namespace clipcoach::ui {
namespace {

std::vector<std::string> splitCsv(const std::string &value)
{
	std::vector<std::string> result;
	std::stringstream stream(value);
	std::string item;
	while (std::getline(stream, item, ',')) {
		const auto first = item.find_first_not_of(" \t");
		const auto last = item.find_last_not_of(" \t");
		if (first != std::string::npos)
			result.push_back(item.substr(first, last - first + 1));
	}
	return result;
}

std::string joinVoicePhrases(const QListWidget *list)
{
	std::string result;
	for (int index = 0; index < list->count(); ++index) {
		if (!result.empty())
			result.push_back(',');
		result += list->item(index)->text().toStdString();
	}
	return result;
}

std::vector<std::string> checkedScenes(const QListWidget *list)
{
	std::vector<std::string> result;
	for (int index = 0; index < list->count(); ++index) {
		const auto *item = list->item(index);
		if (item->checkState() == Qt::Checked)
			result.push_back(item->data(Qt::UserRole).toString().toStdString());
	}
	return result;
}

std::string joinCsv(const std::vector<std::string> &values)
{
	std::string result;
	for (const auto &value : values) {
		if (!result.empty())
			result.push_back(',');
		result += value;
	}
	return result;
}

void clearLayout(QVBoxLayout *layout)
{
	while (auto *item = layout->takeAt(0)) {
		delete item->widget();
		delete item;
	}
}

QWidget *fieldRow(const QString &title, const QString &description,
		  QWidget *editor, QWidget *parent)
{
	auto *row = new QWidget(parent);
	row->setObjectName(QStringLiteral("triggerFieldRow"));
	row->setProperty("triggerRole", QStringLiteral("field"));
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(tokens::kSpaceSm, tokens::kSpaceSm,
				   tokens::kSpaceSm, tokens::kSpaceSm);
	layout->setSpacing(tokens::kSpaceMd);

	auto *copy = new QHBoxLayout();
	copy->setContentsMargins(0, 0, 0, 0);
	copy->setSpacing(tokens::kSpaceSm);
	auto *label = new QLabel(title, row);
	label->setObjectName(QStringLiteral("triggerFieldTitle"));
	copy->addWidget(label);
	if (!description.isEmpty()) {
		auto *help = new ContextHelpButton(description, row);
		help->setObjectName(editor->objectName() +
				    QStringLiteral("InfoButton"));
		copy->addWidget(help, 0, Qt::AlignVCenter);
		editor->setToolTip(description);
	}
	copy->addStretch(1);
	layout->addLayout(copy, 1);

	editor->setMinimumWidth(170);
	editor->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	layout->addWidget(editor, 0, Qt::AlignVCenter);
	return row;
}

} // namespace

TriggersTab::TriggersTab(TranslationFunction translator, TriggerEngine *engine,
			 SettingsManager *settingsManager, QWidget *parent,
			 VoiceTriggerController *voiceController,
			 SceneProvider sceneProvider)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  engine_(engine),
	  settingsManager_(settingsManager),
	  voiceController_(voiceController),
	  sceneProvider_(std::move(sceneProvider))
{
	setObjectName(QStringLiteral("triggersTab"));
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);

	auto *content = new QWidget(this);
	auto *layout = new QVBoxLayout(content);
	layout->setContentsMargins(tokens::kPageMargin, tokens::kPageMargin,
				   tokens::kPageMargin, tokens::kPageMargin);
	layout->setSpacing(tokens::kSectionGap);
	layout->setAlignment(Qt::AlignTop);

	auto *pageHeader = new QFrame(content);
	pageHeader->setObjectName(QStringLiteral("triggersPageHeader"));
	pageHeader->setProperty("triggerRole", QStringLiteral("pageHeader"));
	pageHeader->setProperty("pageRole", QStringLiteral("header"));
	auto *pageHeaderLayout = new QHBoxLayout(pageHeader);
	pageHeaderLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd,
					     tokens::kSpaceLg, tokens::kSpaceMd);
	pageHeaderLayout->setSpacing(tokens::kSpaceMd);
	auto *headerIcon = new QLabel(pageHeader);
	headerIcon->setObjectName(QStringLiteral("triggersPageIcon"));
	headerIcon->setProperty("pageRole", QStringLiteral("icon"));
	headerIcon->setAlignment(Qt::AlignCenter);
	headerIcon->setPixmap(style()->standardIcon(QStyle::SP_BrowserReload)
				      .pixmap(22, 22));
	headerIcon->setFixedSize(tokens::kPageHeaderIconSize,
				 tokens::kPageHeaderIconSize);
	auto *headingCopy = new QVBoxLayout();
	headingCopy->setContentsMargins(0, 0, 0, 0);
	headingCopy->setSpacing(tokens::kSpaceXs);
	auto *title = new QLabel(text(strings::kTriggersTitle), pageHeader);
	title->setObjectName(QStringLiteral("PageTitle"));
	auto *subtitle = new QLabel(text(strings::kTriggersSubtitle), pageHeader);
	subtitle->setObjectName(QStringLiteral("PageSubtitle"));
	subtitle->setWordWrap(true);
	headingCopy->addWidget(title);
	headingCopy->addWidget(subtitle);
	pageHeaderLayout->addWidget(headerIcon, 0, Qt::AlignVCenter);
	pageHeaderLayout->addLayout(headingCopy, 1);
	timingSummary_ = new QLabel(pageHeader);
	timingSummary_->setObjectName(QStringLiteral("triggerTimingSummary"));
	timingSummary_->setProperty("triggerRole", QStringLiteral("estimateBadge"));
	timingSummary_->setAlignment(Qt::AlignCenter);
	timingSummary_->setMinimumWidth(180);
	pageHeaderLayout->addWidget(timingSummary_, 0, Qt::AlignVCenter);
	layout->addWidget(pageHeader);

	auto *leftColumn = new QWidget(content);
	leftColumn->setObjectName(QStringLiteral("triggersLeftColumn"));
	auto *leftLayout = new QVBoxLayout(leftColumn);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->setSpacing(tokens::kSpaceMd);
	leftLayout->setAlignment(Qt::AlignTop);
	auto *rightColumn = new QWidget(content);
	rightColumn->setObjectName(QStringLiteral("triggersRightColumn"));
	auto *rightLayout = new QVBoxLayout(rightColumn);
	rightLayout->setContentsMargins(0, 0, 0, 0);
	rightLayout->setSpacing(tokens::kSpaceMd);
	rightLayout->setAlignment(Qt::AlignTop);

	auto *triggerSection =
		new SettingsSection(text(strings::kTriggersSignals), content);
	triggerSection->setObjectName(QStringLiteral("triggerSignalsSection"));
	triggerSection->setProperty("triggerRole", QStringLiteral("card"));
	manual_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerManual),
		text(strings::kTriggerManualDescription),
		QStringLiteral("triggerManualToggle"), true, false);
	voice_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerVoice),
		text(strings::kTriggerVoiceDescription),
		QStringLiteral("triggerVoiceToggle"), false, true);
	audioSpike_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerAudio),
		text(strings::kTriggerAudioDescription),
		QStringLiteral("triggerAudioSpikeToggle"), false, true);
	chatPulse_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerChatPulse),
		text(strings::kTriggerChatPulseDescription),
		QStringLiteral("triggerChatPulseToggle"), false, true);
	scene_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerScene),
		text(strings::kTriggerSceneDescription),
		QStringLiteral("triggerSceneToggle"), false, true);
	keyword_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerKeyword),
		text(strings::kTriggerKeywordDescription),
		QStringLiteral("triggerKeywordToggle"), false, true);
	aiHook_ = addTriggerToggle(
		triggerSection->contentLayout(), text(strings::kTriggerAiHook),
		text(strings::kTriggerAiHookDescription),
		QStringLiteral("triggerAiHookToggle"), false, true);
	leftLayout->addWidget(triggerSection);

	auto *configuration =
		new SettingsSection(text(strings::kTriggersConfiguration), content);
	configuration->setObjectName(QStringLiteral("triggerConfigurationSection"));
	configuration->setProperty("triggerRole", QStringLiteral("card"));
	configuration->setHelpText(
		text(strings::kTriggerConfigurationDescription));
	preRoll_ = new WheelSafeSpinBox(configuration);
	preRoll_->setObjectName(QStringLiteral("triggerPreRoll"));
	preRoll_->setRange(0, 120);
	preRoll_->setSuffix(QStringLiteral(" s"));
	postRoll_ = new WheelSafeSpinBox(configuration);
	postRoll_->setObjectName(QStringLiteral("triggerPostRoll"));
	postRoll_->setRange(0, 120);
	postRoll_->setSuffix(QStringLiteral(" s"));
	sensitivity_ = new WheelSafeSpinBox(configuration);
	sensitivity_->setObjectName(QStringLiteral("triggerSensitivity"));
	sensitivity_->setRange(0, 100);
	sensitivity_->setSuffix(QStringLiteral("%"));
	cooldown_ = new WheelSafeSpinBox(configuration);
	cooldown_->setObjectName(QStringLiteral("triggerCooldown"));
	cooldown_->setRange(0, 3600);
	cooldown_->setSuffix(QStringLiteral(" s"));
	action_ = new WheelSafeComboBox(configuration);
	action_->setObjectName(QStringLiteral("triggerAction"));
	action_->addItem(text(strings::kTriggerActionSave),
			 static_cast<int>(TriggerAction::SaveClip));
	action_->addItem(text(strings::kTriggerActionMark),
			 static_cast<int>(TriggerAction::MarkMoment));
	action_->addItem(text(strings::kTriggerActionSaveVertical),
			 static_cast<int>(TriggerAction::SaveVerticalClip));
	action_->addItem(text(strings::kTriggerActionSaveBoth),
			 static_cast<int>(TriggerAction::SaveBoth));
	action_->addItem(text(strings::kTriggerActionRecommended),
			 static_cast<int>(TriggerAction::AddToRecommended));
	keywords_ = new QLineEdit(configuration);
	keywords_->setObjectName(QStringLiteral("triggerKeywords"));
	keywords_->setPlaceholderText(text(strings::kTriggerKeywordsHint));
	scenes_ = new QListWidget(configuration);
	scenes_->setObjectName(QStringLiteral("triggerScenes"));
	scenes_->setMinimumHeight(112);
	scenes_->setMaximumHeight(150);
	scenes_->setSelectionMode(QAbstractItemView::NoSelection);
	configuration->contentLayout()->addWidget(fieldRow(
		text(strings::kSettingsPreRoll), text(strings::kTriggerPreRollHelp),
		preRoll_, configuration));
	configuration->contentLayout()->addWidget(fieldRow(
		text(strings::kSettingsPostRoll), text(strings::kTriggerPostRollHelp),
		postRoll_, configuration));
	configuration->contentLayout()->addWidget(fieldRow(
		text(strings::kTriggerSensitivity),
		text(strings::kTriggerSensitivityHelp), sensitivity_,
		configuration));
	configuration->contentLayout()->addWidget(fieldRow(
		text(strings::kTriggerCooldown), text(strings::kTriggerCooldownHelp),
		cooldown_, configuration));
	configuration->contentLayout()->addWidget(fieldRow(
		text(strings::kTriggerAction), text(strings::kTriggerActionHelp),
		action_, configuration));
	saveError_ = new QLabel(configuration);
	saveError_->setObjectName(QStringLiteral("triggerSaveError"));
	saveError_->setProperty("class", QStringLiteral("error"));
	saveError_->setWordWrap(true);
	saveError_->hide();
	configuration->contentLayout()->addWidget(saveError_);
	rightLayout->addWidget(configuration);

	auto *sceneSection =
		new SettingsSection(text(strings::kTriggerScene), content);
	sceneSection->setObjectName(QStringLiteral("triggerScenesSection"));
	sceneSection->setProperty("triggerRole", QStringLiteral("card"));
	sceneSection->setHelpText(text(strings::kTriggerSceneHelp));
	sceneSection->contentLayout()->addWidget(scenes_);
	auto *keywordHeading = new QHBoxLayout();
	keywordHeading->setContentsMargins(0, 0, 0, 0);
	keywordHeading->setSpacing(tokens::kSpaceSm);
	auto *keywordLabel = new QLabel(text(strings::kTriggerKeyword), sceneSection);
	keywordLabel->setObjectName(QStringLiteral("triggerKeywordFieldTitle"));
	keywordHeading->addWidget(keywordLabel);
	auto *keywordHelp = new ContextHelpButton(
		text(strings::kTriggerKeywordDescription), sceneSection);
	keywordHelp->setObjectName(QStringLiteral("triggerKeywordsInfoButton"));
	keywordHeading->addWidget(keywordHelp, 0, Qt::AlignVCenter);
	keywordHeading->addStretch(1);
	sceneSection->contentLayout()->addLayout(keywordHeading);
	sceneSection->contentLayout()->addWidget(keywords_);
	leftLayout->addWidget(sceneSection);

	auto *voiceSection =
		new SettingsSection(text(strings::kVoicePhrases), content);
	voiceSection->setObjectName(QStringLiteral("triggerVoiceSection"));
	voiceSection->setProperty("triggerRole", QStringLiteral("card"));
	voiceSection->setHelpText(text(strings::kVoicePhrasesDescription));
	voiceBody_ = new QBoxLayout(QBoxLayout::LeftToRight);
	voiceBody_->setContentsMargins(0, 0, 0, 0);
	voiceBody_->setSpacing(tokens::kSpaceLg);

	auto *phraseEditor = new QWidget(voiceSection);
	phraseEditor->setObjectName(QStringLiteral("voicePhraseEditor"));
	phraseEditor->setProperty("voiceRole", QStringLiteral("group"));
	auto *phraseColumn = new QVBoxLayout(phraseEditor);
	phraseColumn->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd,
					tokens::kSpaceMd, tokens::kSpaceMd);
	phraseColumn->setSpacing(tokens::kSpaceSm);
	voicePhrases_ = new QListWidget(voiceSection);
	voicePhrases_->setObjectName(QStringLiteral("voicePhraseList"));
	voicePhrases_->setMinimumHeight(188);
	voicePhrases_->setSelectionMode(
		QAbstractItemView::SingleSelection);
	voicePhraseInput_ = new QLineEdit(voiceSection);
	voicePhraseInput_->setObjectName(
		QStringLiteral("voicePhraseInput"));
	voicePhraseInput_->setPlaceholderText(
		text(strings::kVoicePhraseInputHint));
	voicePhraseInput_->setFixedHeight(tokens::kLargeControlHeight);
	addVoicePhrase_ = new QPushButton(text(strings::kVoicePhraseAdd),
					 voiceSection);
	addVoicePhrase_->setObjectName(
		QStringLiteral("addVoicePhraseButton"));
	removeVoicePhrase_ =
		new QPushButton(text(strings::kVoicePhraseRemove),
				voiceSection);
	removeVoicePhrase_->setObjectName(
		QStringLiteral("removeVoicePhraseButton"));
	addVoicePhrase_->setProperty("controlRole", QStringLiteral("primary"));
	removeVoicePhrase_->setProperty("controlRole", QStringLiteral("secondary"));
	addVoicePhrase_->setFixedHeight(tokens::kLargeControlHeight);
	removeVoicePhrase_->setFixedHeight(tokens::kLargeControlHeight);
	addVoicePhrase_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	removeVoicePhrase_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
	auto *phraseActions = new QHBoxLayout();
	phraseActions->setContentsMargins(0, 0, 0, 0);
	phraseActions->setSpacing(tokens::kSpaceSm);
	phraseActions->addWidget(addVoicePhrase_, 1);
	phraseActions->addWidget(removeVoicePhrase_, 1);
	phraseColumn->addWidget(voicePhrases_);
	phraseColumn->addWidget(voicePhraseInput_);
	phraseColumn->addLayout(phraseActions);

	auto *voiceControls = new QWidget(voiceSection);
	voiceControls->setObjectName(QStringLiteral("voiceRecognitionControls"));
	voiceControls->setProperty("voiceRole", QStringLiteral("group"));
	auto *voiceControlsLayout = new QVBoxLayout(voiceControls);
	voiceControlsLayout->setContentsMargins(
		tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd,
		tokens::kSpaceMd);
	voiceControlsLayout->setSpacing(tokens::kSpaceMd);
	auto *voiceForm = new QFormLayout();
	voiceForm->setContentsMargins(0, 0, 0, 0);
	voiceForm->setHorizontalSpacing(tokens::kSpaceLg);
	voiceForm->setVerticalSpacing(tokens::kSpaceMd);
	voiceForm->setFieldGrowthPolicy(
		QFormLayout::AllNonFixedFieldsGrow);
	voiceForm->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
	voiceLanguage_ = new WheelSafeComboBox(voiceSection);
	voiceLanguage_->setObjectName(
		QStringLiteral("voiceLanguage"));
	voiceLanguage_->addItem(text(strings::kVoiceLanguageAuto), "auto");
	voiceLanguage_->addItem(text(strings::kVoiceLanguageSpanish), "es");
	voiceLanguage_->addItem(text(strings::kVoiceLanguageEnglish), "en");
	voiceAudioSource_ = new WheelSafeComboBox(voiceSection);
	voiceAudioSource_->setObjectName(QStringLiteral("voiceAudioSource"));
	voiceAudioSource_->addItem(text(strings::kVoiceAudioSourceAuto), QString{});
	if (voiceController_ != nullptr) {
		for (const auto &source : voiceController_->availableAudioSources())
			voiceAudioSource_->addItem(QString::fromStdString(source),
						 QString::fromStdString(source));
	}
	voiceSpeechMode_ = new WheelSafeComboBox(voiceSection);
	voiceSpeechMode_->setObjectName(
		QStringLiteral("voiceSpeechMode"));
	voiceSpeechMode_->addItem(
		text(strings::kVoiceModeLocal),
		static_cast<int>(SpeechProcessingMode::Local));
	voiceForm->addRow(text(strings::kVoiceAudioSource), voiceAudioSource_);
	voiceForm->addRow(text(strings::kVoiceLanguage), voiceLanguage_);
	voiceForm->addRow(text(strings::kVoiceProcessingMode),
			  voiceSpeechMode_);
	voiceControlsLayout->addLayout(voiceForm);
	voiceStatus_ = new QLabel(text(strings::kVoiceStatusDisabled), voiceControls);
	voiceStatus_->setObjectName(QStringLiteral("voiceRuntimeStatus"));
	voiceStatus_->setWordWrap(true);
	voiceStatus_->setProperty("notificationTone", QStringLiteral("info"));
	voiceStatus_->setMargin(tokens::kSpaceMd);
	voiceStatus_->setMinimumHeight(58);
	voiceControlsLayout->addWidget(voiceStatus_);
	voiceBody_->addWidget(phraseEditor, 5);
	voiceBody_->addWidget(voiceControls, 7);
	voiceSection->contentLayout()->addLayout(voiceBody_);

	const auto voiceControlsEnabled =
		engine_ != nullptr && engine_->proUnlocked();
	for (auto *widget :
	     std::vector<QWidget *>{voicePhrases_, voicePhraseInput_,
				    addVoicePhrase_, removeVoicePhrase_,
				    voiceAudioSource_, voiceLanguage_,
				    voiceSpeechMode_})
		widget->setEnabled(voiceControlsEnabled);
	auto *events =
		new SettingsSection(text(strings::kTriggersRecentEvents), content);
	events->setObjectName(QStringLiteral("triggerRecentEventsSection"));
	events->setProperty("triggerRole", QStringLiteral("card"));
	events->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
	events->setMinimumHeight(112);
	eventsLayout_ = events->contentLayout();
	eventsLayout_->setContentsMargins(
		tokens::kSpaceXl, tokens::kSpaceLg,
		tokens::kSpaceXl, tokens::kSpaceLg);
	eventsLayout_->setSpacing(tokens::kSpaceMd);
	if (engine_ == nullptr || !engine_->proUnlocked()) {
		upgradeBanner_ = new UpgradeBanner(
			text(strings::kUpgradeTitle),
			text(strings::kUpgradeDescription),
			text(strings::kUpgradeAction), text(strings::kPro),
			content);
		rightLayout->addWidget(upgradeBanner_);
	}
	leftLayout->addStretch(1);
	rightLayout->addStretch(1);

	mainColumns_ = new QBoxLayout(QBoxLayout::LeftToRight);
	mainColumns_->setContentsMargins(0, 0, 0, 0);
	mainColumns_->setSpacing(tokens::kSpaceMd);
	mainColumns_->addWidget(leftColumn, 1);
	mainColumns_->addWidget(rightColumn, 1);
	layout->addLayout(mainColumns_);
	layout->addWidget(events);
	layout->addWidget(voiceSection);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);
	root->addWidget(scroll);

	refreshSceneOptions();
	load();
	updateTimingSummary();
	if (voiceController_ != nullptr) {
		QPointer<TriggersTab> self(this);
		voiceController_->setStatusCallback([self](const VoiceRuntimeStatus &status) {
			if (!self.isNull())
				self->updateVoiceStatus(status);
		});
		updateVoiceStatus(voiceController_->status());
	}
	for (auto *control :
	     {manual_, voice_, audioSpike_, chatPulse_, scene_, keyword_,
	      aiHook_})
		connect(control, &QCheckBox::toggled, this,
			[this] { applyAndSave(); });
	for (auto *control : {preRoll_, postRoll_, sensitivity_, cooldown_})
		connect(control, &QSpinBox::editingFinished, this,
			[this] { applyAndSave(); });
	connect(preRoll_, &QSpinBox::valueChanged, this,
		[this] { updateTimingSummary(); });
	connect(postRoll_, &QSpinBox::valueChanged, this,
		[this] { updateTimingSummary(); });
	connect(action_, &QComboBox::currentIndexChanged, this,
		[this] { applyAndSave(); });
	connect(keywords_, &QLineEdit::editingFinished, this,
		[this] { applyAndSave(); });
	connect(scenes_, &QListWidget::itemChanged, this,
		[this](QListWidgetItem *) { applyAndSave(); });
	connect(addVoicePhrase_, &QPushButton::clicked, this, [this] {
		const auto phrase = voicePhraseInput_->text().trimmed();
		if (phrase.isEmpty() || phrase.contains(','))
			return;
		const auto duplicate = voicePhrases_->findItems(
			phrase, Qt::MatchFixedString);
		if (!duplicate.empty())
			return;
		voicePhrases_->addItem(phrase);
		voicePhraseInput_->clear();
		applyAndSave();
	});
	connect(removeVoicePhrase_, &QPushButton::clicked, this, [this] {
		if (voicePhrases_->count() <= 1)
			return;
		delete voicePhrases_->takeItem(
			voicePhrases_->currentRow());
		applyAndSave();
	});
	connect(voiceLanguage_, &QComboBox::currentIndexChanged, this,
		[this] { applyAndSave(); });
	connect(voiceAudioSource_, &QComboBox::currentIndexChanged, this,
		[this] { applyAndSave(); });
	connect(voiceSpeechMode_, &QComboBox::currentIndexChanged, this,
		[this] { applyAndSave(); });

	auto *timer = new QTimer(this);
	timer->setInterval(100);
	connect(timer, &QTimer::timeout, this,
		[this] { refreshRecentEvents(); });
	timer->start();
	if (sceneProvider_) {
		auto *sceneTimer = new QTimer(this);
		sceneTimer->setInterval(2000);
		connect(sceneTimer, &QTimer::timeout, this,
			[this] { refreshSceneOptions(); });
		sceneTimer->start();
	}
	refreshRecentEvents();
}

void TriggersTab::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	if (mainColumns_ != nullptr) {
		mainColumns_->setDirection(width() < 900
						   ? QBoxLayout::TopToBottom
						   : QBoxLayout::LeftToRight);
	}
	if (voiceBody_ != nullptr) {
		voiceBody_->setDirection(width() < 820
					 ? QBoxLayout::TopToBottom
					 : QBoxLayout::LeftToRight);
	}
}

void TriggersTab::refreshLicenseState()
{
	const bool pro = engine_ != nullptr && engine_->proUnlocked();
	for (auto *toggle : {voice_, audioSpike_, chatPulse_, scene_, keyword_, aiHook_}) {
		toggle->setEnabled(pro);
		toggle->setToolTip(pro ? QString{} : text(strings::kTriggerProRequired));
	}
	for (auto *widget :
	     std::vector<QWidget *>{voicePhrases_, voicePhraseInput_, addVoicePhrase_, removeVoicePhrase_,
				    voiceAudioSource_, voiceLanguage_, voiceSpeechMode_})
		widget->setEnabled(pro);
	if (upgradeBanner_ != nullptr)
		upgradeBanner_->setVisible(!pro);
	load();
}

void TriggersTab::refreshConfiguration()
{
	load();
}

void TriggersTab::setTimingChangedCallback(TimingChangedCallback callback)
{
	timingChangedCallback_ = std::move(callback);
}

QString TriggersTab::text(const char *key) const
{
	return translator_ ? translator_(key) : QString::fromUtf8(key);
}

QCheckBox *TriggersTab::addTriggerToggle(
	QVBoxLayout *layout, const QString &title, const QString &description,
	const QString &objectName, bool checked, bool pro)
{
	QString glyph = QStringLiteral("◆");
	if (objectName.contains(QStringLiteral("Manual")))
		glyph = QStringLiteral("✋");
	else if (objectName.contains(QStringLiteral("Voice")))
		glyph = QStringLiteral("◉");
	else if (objectName.contains(QStringLiteral("Audio")))
		glyph = QStringLiteral("≋");
	else if (objectName.contains(QStringLiteral("Chat")))
		glyph = QStringLiteral("▣");
	else if (objectName.contains(QStringLiteral("Scene")))
		glyph = QStringLiteral("▤");
	else if (objectName.contains(QStringLiteral("Keyword")))
		glyph = QStringLiteral("#");
	else if (objectName.contains(QStringLiteral("Ai")))
		glyph = QStringLiteral("✦");
	auto *row = new ToggleRow(glyph + QStringLiteral("  ") + title,
				  description, checked, this);
	row->setHelpButtonObjectName(objectName +
				     QStringLiteral("InfoButton"));
	row->setProperty("triggerRole", QStringLiteral("signal"));
	if (auto *signalLayout = qobject_cast<QHBoxLayout *>(row->layout());
	    signalLayout != nullptr) {
		signalLayout->setContentsMargins(
			tokens::kCardPaddingHorizontal, tokens::kSpaceSm,
			tokens::kCardPaddingHorizontal, tokens::kSpaceSm);
	}
	row->setMinimumHeight(44);
	row->setMaximumHeight(56);
	auto *toggle = row->findChild<QCheckBox *>();
	toggle->setObjectName(objectName);
	if (pro && (engine_ == nullptr || !engine_->proUnlocked())) {
		toggle->setChecked(false);
		toggle->setEnabled(false);
		toggle->setToolTip(text(strings::kTriggerProRequired));
	}
	layout->addWidget(row);
	return toggle;
}

void TriggersTab::load()
{
	const auto settings = settingsManager_ != nullptr
				      ? settingsManager_->settings()
				      : Settings::defaults();
	const QSignalBlocker manualBlock(manual_);
	const QSignalBlocker voiceBlock(voice_);
	const QSignalBlocker audioBlock(audioSpike_);
	const QSignalBlocker chatBlock(chatPulse_);
	const QSignalBlocker sceneBlock(scene_);
	const QSignalBlocker keywordBlock(keyword_);
	const QSignalBlocker aiBlock(aiHook_);
	const QSignalBlocker preRollBlock(preRoll_);
	const QSignalBlocker postRollBlock(postRoll_);
	const QSignalBlocker sensitivityBlock(sensitivity_);
	const QSignalBlocker cooldownBlock(cooldown_);
	const QSignalBlocker actionBlock(action_);
	const QSignalBlocker keywordsBlock(keywords_);
	const QSignalBlocker scenesBlock(scenes_);
	const QSignalBlocker voiceLanguageBlock(voiceLanguage_);
	const QSignalBlocker voiceAudioSourceBlock(voiceAudioSource_);
	const QSignalBlocker voiceModeBlock(voiceSpeechMode_);
	manual_->setChecked(settings.triggerManualEnabled);
	voice_->setChecked(voice_->isEnabled() && settings.triggerVoiceEnabled);
	audioSpike_->setChecked(audioSpike_->isEnabled() &&
			       settings.triggerAudioSpikeEnabled);
	chatPulse_->setChecked(chatPulse_->isEnabled() &&
			      settings.triggerChatPulseEnabled);
	scene_->setChecked(scene_->isEnabled() && settings.triggerSceneEnabled);
	keyword_->setChecked(keyword_->isEnabled() &&
			    settings.triggerKeywordEnabled);
	aiHook_->setChecked(aiHook_->isEnabled() &&
			   settings.triggerAiHookEnabled);
	preRoll_->setValue(settings.preRollSeconds);
	postRoll_->setValue(settings.postRollSeconds);
	sensitivity_->setValue(settings.triggerSensitivity);
	cooldown_->setValue(settings.triggerCooldownSeconds);
	action_->setCurrentIndex(std::max(
		0, action_->findData(static_cast<int>(settings.triggerAction))));
	keywords_->setText(
		QString::fromStdString(settings.triggerKeywordsCsv));
	const auto configuredScenes = splitCsv(settings.triggerScenesCsv);
	for (const auto &configuredScene : configuredScenes) {
		const auto value = QString::fromStdString(configuredScene);
		auto matches = scenes_->findItems(value, Qt::MatchExactly);
		QListWidgetItem *item = matches.empty() ? nullptr : matches.front();
		if (item == nullptr) {
			item = new QListWidgetItem(value, scenes_);
			item->setData(Qt::UserRole, value);
			item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		}
		item->setCheckState(Qt::Checked);
	}
	voicePhrases_->clear();
	for (const auto &phrase :
	     splitCsv(settings.voiceTriggerPhrasesCsv))
		voicePhrases_->addItem(QString::fromStdString(phrase));
	voiceLanguage_->setCurrentIndex(std::max(
		0, voiceLanguage_->findData(
			   QString::fromStdString(
				   settings.voiceTriggerLanguage))));
	auto sourceIndex = voiceAudioSource_->findData(
		QString::fromStdString(settings.voiceAudioSourceName));
	if (sourceIndex < 0 && !settings.voiceAudioSourceName.empty()) {
		voiceAudioSource_->addItem(
			QString::fromStdString(settings.voiceAudioSourceName),
			QString::fromStdString(settings.voiceAudioSourceName));
		sourceIndex = voiceAudioSource_->count() - 1;
	}
	voiceAudioSource_->setCurrentIndex(std::max(0, sourceIndex));
	voiceSpeechMode_->setCurrentIndex(0);
	applyEngineConfiguration();
	updateTimingSummary();
}

void TriggersTab::applyAndSave()
{
	if (settingsManager_ == nullptr) {
		applyEngineConfiguration();
		return;
	}
	auto settings = settingsManager_->settings();
	settings.triggerManualEnabled = manual_->isChecked();
	settings.triggerVoiceEnabled = voice_->isChecked();
	settings.triggerAudioSpikeEnabled = audioSpike_->isChecked();
	settings.triggerChatPulseEnabled = chatPulse_->isChecked();
	settings.triggerSceneEnabled = scene_->isChecked();
	settings.triggerKeywordEnabled = keyword_->isChecked();
	settings.triggerAiHookEnabled = aiHook_->isChecked();
	settings.preRollSeconds = preRoll_->value();
	settings.postRollSeconds = postRoll_->value();
	settings.triggerSensitivity = sensitivity_->value();
	settings.triggerCooldownSeconds = cooldown_->value();
	settings.triggerAction = static_cast<TriggerAction>(
		action_->currentData().toInt());
	settings.triggerKeywordsCsv = keywords_->text().toStdString();
	settings.triggerScenesCsv = joinCsv(checkedScenes(scenes_));
	settings.voiceTriggerPhrasesCsv =
		joinVoicePhrases(voicePhrases_);
	settings.voiceTriggerLanguage =
		voiceLanguage_->currentData().toString().toStdString();
	settings.voiceAudioSourceName =
		voiceAudioSource_->currentData().toString().toStdString();
	// This build ships a complete local OBS-audio recognizer only. Do not
	// expose or persist a cloud mode until a real authenticated transport is
	// available.
	settings.voiceSpeechMode = SpeechProcessingMode::Local;
	settings.voiceCloudConsent = false;
	std::string error;
	if (!settingsManager_->save(settings, &error)) {
		saveError_->setText(QString::fromStdString(error));
		saveError_->show();
		load();
		return;
	}
	saveError_->hide();
	applyEngineConfiguration();
	if (timingChangedCallback_)
		timingChangedCallback_(settings.preRollSeconds +
				       settings.postRollSeconds);
}

void TriggersTab::applyEngineConfiguration()
{
	if (engine_ == nullptr)
		return;
	const auto configure = [this](SmartTriggerType type, bool enabled) {
		TriggerConfiguration config;
		config.enabled = enabled;
		config.preRollSeconds = preRoll_->value();
		config.postRollSeconds = postRoll_->value();
		config.sensitivity = sensitivity_->value();
		config.action = static_cast<TriggerAction>(
			action_->currentData().toInt());
		config.keywords = splitCsv(keywords_->text().toStdString());
		config.scenes = checkedScenes(scenes_);
		config.cooldown = std::chrono::seconds(cooldown_->value());
		std::string ignored;
		(void)engine_->setConfiguration(type, config, &ignored);
	};
	configure(SmartTriggerType::Manual, manual_->isChecked());
	configure(SmartTriggerType::Voice, voice_->isChecked());
	configure(SmartTriggerType::AudioSpike, audioSpike_->isChecked());
	configure(SmartTriggerType::ChatPulse, chatPulse_->isChecked());
	configure(SmartTriggerType::Scene, scene_->isChecked());
	configure(SmartTriggerType::Keyword, keyword_->isChecked());
	configure(SmartTriggerType::FutureAiHook, aiHook_->isChecked());
	if (voiceController_ != nullptr && settingsManager_ != nullptr)
		voiceController_->applySettings(settingsManager_->settings());
}

void TriggersTab::refreshSceneOptions()
{
	if (!sceneProvider_)
		return;
	const auto selected = checkedScenes(scenes_);
	const QSignalBlocker blocker(scenes_);
	scenes_->clear();
	for (const auto &scene : sceneProvider_()) {
		if (scene.empty())
			continue;
		const auto value = QString::fromStdString(scene);
		auto *item = new QListWidgetItem(value, scenes_);
		item->setData(Qt::UserRole, value);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(
			std::find(selected.begin(), selected.end(), scene) != selected.end()
				? Qt::Checked
				: Qt::Unchecked);
	}
}

void TriggersTab::updateTimingSummary()
{
	if (timingSummary_ == nullptr)
		return;
	timingSummary_->setText(
		text(strings::kTriggerTimingSummary)
			.arg(preRoll_->value())
			.arg(postRoll_->value())
			.arg(preRoll_->value() + postRoll_->value()));
}

void TriggersTab::updateVoiceStatus(const VoiceRuntimeStatus &status)
{
	if (voiceStatus_ == nullptr)
		return;
	const char *key = strings::kVoiceStatusError;
	QString tone = QStringLiteral("error");
	switch (status.state) {
	case VoiceRuntimeState::Disabled:
		key = strings::kVoiceStatusDisabled;
		tone = QStringLiteral("info");
		break;
	case VoiceRuntimeState::Starting:
		key = strings::kVoiceStatusStarting;
		tone = QStringLiteral("info");
		break;
	case VoiceRuntimeState::WaitingForAudio:
		key = strings::kVoiceStatusWaitingForAudio;
		tone = QStringLiteral("info");
		break;
	case VoiceRuntimeState::Listening:
		key = strings::kVoiceStatusListeningBilingual;
		tone = QStringLiteral("success");
		break;
	case VoiceRuntimeState::ModelUnavailable:
		key = strings::kVoiceStatusLanguageMissing;
		break;
	case VoiceRuntimeState::SourceUnavailable:
		key = strings::kVoiceStatusMicrophoneError;
		break;
	case VoiceRuntimeState::Error:
		break;
	}
	QString message = text(key);
	if (!status.sourceName.empty())
		message += QStringLiteral(" · ") +
			   QString::fromStdString(status.sourceName);
	if (!status.lastTranscript.empty())
		message += QStringLiteral("\n") +
			   text(strings::kVoiceStatusLastHeard)
				   .arg(QString::fromStdString(status.lastTranscript));
	voiceStatus_->setText(message);
	voiceStatus_->setToolTip(QString::fromStdString(status.message));
	voiceStatus_->setProperty("notificationTone", tone);
	voiceStatus_->style()->unpolish(voiceStatus_);
	voiceStatus_->style()->polish(voiceStatus_);
}

void TriggersTab::refreshRecentEvents()
{
	clearLayout(eventsLayout_);
	const auto events =
		engine_ != nullptr ? engine_->recentEvents(20)
				   : std::vector<TriggerEvent>{};
	if (events.empty()) {
		auto *empty =
			new QLabel(text(strings::kTriggersNoEvents), this);
		empty->setObjectName(QStringLiteral("triggerEventsEmpty"));
		empty->setProperty("class", QStringLiteral("supporting"));
		eventsLayout_->addWidget(empty);
		return;
	}
	for (const auto &event : events) {
		const auto time = QDateTime::fromMSecsSinceEpoch(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				event.occurredAt.time_since_epoch())
				.count());
		auto *label = new QLabel(
			text(strings::kTriggerEventFormat)
				.arg(QString::fromUtf8(
					triggerTypeName(event.primaryType)))
				.arg(event.score)
				.arg(time.toString(QStringLiteral("HH:mm:ss"))),
			this);
		label->setObjectName(QStringLiteral("triggerEventItem"));
		label->setProperty("triggerRole", QStringLiteral("event"));
		label->setWordWrap(true);
		label->setMinimumHeight(38);
		label->setMargin(tokens::kSpaceMd);
		eventsLayout_->addWidget(label);
	}
}

} // namespace clipcoach::ui
