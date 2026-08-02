#include <clipcoach/ui/settings-tab.hpp>

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/licensing/license-manager.hpp>
#include <clipcoach/ui/components/context-help-button.hpp>
#include <clipcoach/ui/components/settings-section.hpp>
#include <clipcoach/ui/components/upgrade-banner.hpp>
#include <clipcoach/ui/components/wheel-safe-controls.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/integrations-panel.hpp>
#include <clipcoach/ui/pro-account-tab.hpp>
#include <clipcoach/ui/settings-controller.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QCheckBox>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFrame>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

#include <algorithm>
#include <chrono>
#include <utility>

namespace clipcoach::ui {
namespace {

QWidget *formRow(const QString &label, const QString &description, QWidget *control, QWidget *parent)
{
	auto *row = new QWidget(parent);
	row->setObjectName(QStringLiteral("SettingsFormRow"));
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceSm, tokens::kSpaceLg, tokens::kSpaceSm);
	layout->setSpacing(tokens::kSpaceMd);
	auto *copyColumn = new QHBoxLayout();
	copyColumn->setSpacing(tokens::kSpaceSm);
	auto *copy = new QLabel(label, row);
	copy->setObjectName(QStringLiteral("SettingsFieldLabel"));
	copy->setWordWrap(true);
	copyColumn->addWidget(copy);
	if (!description.isEmpty()) {
		auto *help = new ContextHelpButton(description, row);
		help->setObjectName(control->objectName() + QStringLiteral("InfoButton"));
		copyColumn->addWidget(help, 0, Qt::AlignVCenter);
		control->setToolTip(description);
	}
	copyColumn->addStretch(1);
	layout->addLayout(copyColumn, 1);
	if (qobject_cast<QCheckBox *>(control) == nullptr) {
		control->setMinimumWidth(118);
		control->setMaximumWidth(360);
	}
	layout->addWidget(control);
	return row;
}

QWidget *pathFormRow(const QString &label, const QString &description, QWidget *control, QWidget *parent)
{
	auto *row = new QWidget(parent);
	row->setObjectName(QStringLiteral("SettingsPathRow"));
	row->setProperty("settingsRole", QStringLiteral("pathRow"));

	auto *layout = new QVBoxLayout(row);
	layout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceSm, tokens::kSpaceLg, tokens::kSpaceSm);
	layout->setSpacing(tokens::kSpaceSm);

	auto *heading = new QHBoxLayout();
	heading->setContentsMargins(0, 0, 0, 0);
	heading->setSpacing(tokens::kSpaceSm);
	auto *copy = new QLabel(label, row);
	copy->setObjectName(QStringLiteral("SettingsFieldLabel"));
	copy->setWordWrap(true);
	heading->addWidget(copy);

	if (!description.isEmpty()) {
		auto *help = new ContextHelpButton(description, row);
		help->setObjectName(control->objectName() + QStringLiteral("InfoButton"));
		heading->addWidget(help, 0, Qt::AlignVCenter);
		control->setToolTip(description);
	}
	heading->addStretch(1);
	layout->addLayout(heading);

	control->setMinimumWidth(0);
	control->setMaximumWidth(QWIDGETSIZE_MAX);
	control->setSizePolicy(QSizePolicy::Expanding, control->sizePolicy().verticalPolicy());
	layout->addWidget(control);
	return row;
}

QWidget *remoteStatusTile(const QString &title, QLabel *&value, const QString &initial, const QString &objectName,
			  QWidget *parent)
{
	auto *tile = new QFrame(parent);
	tile->setObjectName(QStringLiteral("RemoteClipperStatusTile"));
	tile->setProperty("settingsRole", QStringLiteral("statusTile"));
	auto *layout = new QVBoxLayout(tile);
	layout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceMd);
	layout->setSpacing(tokens::kSpaceXs);
	auto *heading = new QLabel(title, tile);
	heading->setObjectName(QStringLiteral("RemoteClipperStatusTitle"));
	heading->setProperty("class", QStringLiteral("supporting"));
	value = new QLabel(initial, tile);
	value->setObjectName(objectName);
	value->setWordWrap(true);
	value->setProperty("class", QStringLiteral("statusValue"));
	layout->addWidget(heading);
	layout->addWidget(value);
	return tile;
}

class RemoteStatusGrid final : public QWidget {
public:
	explicit RemoteStatusGrid(QWidget *parent) : QWidget(parent), grid_(new QGridLayout(this))
	{
		setObjectName(QStringLiteral("remoteClipperStatusGrid"));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
		grid_->setContentsMargins(0, tokens::kSpaceSm, 0, 0);
		grid_->setHorizontalSpacing(tokens::kSpaceMd);
		grid_->setVerticalSpacing(tokens::kSpaceMd);
	}

	void addTile(QWidget *tile)
	{
		tiles_.push_back(tile);
		arrangeTiles();
	}

protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		arrangeTiles();
	}

private:
	void arrangeTiles()
	{
		const int columns = width() < 600 ? 1 : 3;
		if (columns == columns_ && grid_->count() == static_cast<int>(tiles_.size()))
			return;
		while (auto *item = grid_->takeAt(0))
			delete item;
		for (int column = 0; column < 3; ++column)
			grid_->setColumnStretch(column, 0);
		for (std::size_t index = 0; index < tiles_.size(); ++index)
			grid_->addWidget(tiles_[index], static_cast<int>(index) / columns,
					 static_cast<int>(index) % columns);
		for (int column = 0; column < columns; ++column)
			grid_->setColumnStretch(column, 1);
		columns_ = columns;
		updateGeometry();
	}

	QGridLayout *grid_{nullptr};
	std::vector<QWidget *> tiles_;
	int columns_{0};
};

QByteArray helpKey(const QString &objectName)
{
	return QStringLiteral("Settings.Help.%1").arg(objectName).toUtf8();
}

} // namespace

SettingsTab::SettingsTab(TranslationFunction translator, SettingsManager *manager, QWidget *parent,
			 integrations::ChatIntegrationManager *chatManager, licensing::LicenseManager *licenseManager,
			 VerticalObsBridge replayObsBridge)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  settingsManager_(manager),
	  chatManager_(chatManager),
	  licenseManager_(licenseManager),
	  replayObsBridge_(std::move(replayObsBridge))
{
	setObjectName(QStringLiteral("settingsTab"));
	setAttribute(Qt::WA_StyledBackground, true);
	controller_ = new SettingsController(manager, this);
	controller_->setErrorCallback([this](const std::string &) { showError(); });
	buildUi();
	bindUi();
	refreshReplayProfile();
	controller_->setAppliedCallback([this](const Settings &before, const Settings &after) {
		errorLabel_->hide();
		if (appliedCallback_) {
			appliedCallback_(before, after);
		}
	});
}

void SettingsTab::setAppliedCallback(AppliedCallback callback)
{
	appliedCallback_ = std::move(callback);
}

void SettingsTab::setProfileImportedCallback(ProfileImportedCallback callback)
{
	profileImportedCallback_ = std::move(callback);
}

void SettingsTab::setSetupRequestedCallback(SetupRequestedCallback callback)
{
	setupRequestedCallback_ = std::move(callback);
}

bool SettingsTab::exportProfileTo(const std::filesystem::path &path, std::string *error) const
{
	if (settingsManager_ == nullptr) {
		if (error != nullptr)
			*error = "settings storage is unavailable";
		return false;
	}
	return settingsManager_->exportProfile(path, error);
}

bool SettingsTab::importProfileFrom(const std::filesystem::path &path, std::string *error)
{
	if (settingsManager_ == nullptr) {
		if (error != nullptr)
			*error = "settings storage is unavailable";
		return false;
	}
	const auto before = settingsManager_->settings();
	if (!settingsManager_->importProfile(path, error))
		return false;
	profileImportChangedLanguage_ = before.language != settingsManager_->settings().language;
	if (appliedCallback_)
		appliedCallback_(before, settingsManager_->settings());
	return true;
}

QString SettingsTab::text(const char *key) const
{
	return translator_ ? translator_(key) : QString::fromUtf8(key);
}

void SettingsTab::buildUi()
{
	const auto &settings = controller_->settings();
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	auto *scroll = new QScrollArea(this);
	scroll->setObjectName(QStringLiteral("settingsScroll"));
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *content = new QWidget(scroll);
	content->setObjectName(QStringLiteral("settingsContent"));
	auto *layout = new QVBoxLayout(content);
	layout->setContentsMargins(tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin, tokens::kPageMargin);
	layout->setSpacing(tokens::kSectionGap);

	auto *pageHeader = new QFrame(content);
	pageHeader->setObjectName(QStringLiteral("settingsPageHeader"));
	pageHeader->setProperty("settingsRole", QStringLiteral("pageHeader"));
	pageHeader->setProperty("pageRole", QStringLiteral("header"));
	auto *pageHeaderLayout = new QHBoxLayout(pageHeader);
	pageHeaderLayout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceMd);
	pageHeaderLayout->setSpacing(tokens::kSpaceMd);
	auto *headerIcon = new QLabel(pageHeader);
	headerIcon->setObjectName(QStringLiteral("settingsPageIcon"));
	headerIcon->setProperty("pageRole", QStringLiteral("icon"));
	headerIcon->setAlignment(Qt::AlignCenter);
	headerIcon->setPixmap(style()->standardIcon(QStyle::SP_FileDialogDetailedView).pixmap(22, 22));
	headerIcon->setFixedSize(tokens::kPageHeaderIconSize, tokens::kPageHeaderIconSize);
	auto *headerCopy = new QVBoxLayout();
	headerCopy->setSpacing(tokens::kSpaceXs);
	auto *title = new QLabel(text(strings::kSettingsTitle), pageHeader);
	title->setObjectName(QStringLiteral("PageTitle"));
	auto *subtitle = new QLabel(text(strings::kSettingsSubtitle), pageHeader);
	subtitle->setObjectName(QStringLiteral("PageSubtitle"));
	subtitle->setWordWrap(true);
	headerCopy->addWidget(title);
	headerCopy->addWidget(subtitle);
	pageHeaderLayout->addWidget(headerIcon);
	pageHeaderLayout->addLayout(headerCopy, 1);
	layout->addWidget(pageHeader);
	widthHint_ = new QLabel(text(strings::kSettingsWidthHint), content);
	widthHint_->setObjectName(QStringLiteral("settingsWidthHint"));
	widthHint_->setProperty("notificationTone", QStringLiteral("info"));
	widthHint_->setWordWrap(true);
	widthHint_->hide();
	layout->addWidget(widthHint_);
	errorLabel_ = new QLabel(text(strings::kSettingsSaveError), content);
	errorLabel_->setObjectName(QStringLiteral("settingsErrorNotification"));
	errorLabel_->setProperty("notificationTone", QStringLiteral("error"));
	errorLabel_->setWordWrap(true);
	errorLabel_->hide();
	layout->addWidget(errorLabel_);

	sectionsContainer_ = new QWidget(content);
	sectionsContainer_->setObjectName(QStringLiteral("settingsSectionsContainer"));
	sectionsContainer_->setMinimumWidth(0);
	sectionsContainer_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	sectionsGrid_ = new QGridLayout(sectionsContainer_);
	sectionsGrid_->setContentsMargins(0, 0, 0, 0);
	sectionsGrid_->setHorizontalSpacing(tokens::kSpaceMd);
	sectionsGrid_->setVerticalSpacing(tokens::kSpaceMd);
	layout->addWidget(sectionsContainer_);

	auto *general = addSection(strings::kSettingsGeneral, QStringLiteral("settingsGeneralSection"));
	const int interfaceLanguage = settings.language == "en-US" ? 1 : settings.language == "es-ES" ? 2 : 0;
	addCombo(general, strings::kSettingsInterfaceLanguage, QStringLiteral("pluginLanguageCombo"),
		 {text(strings::kSettingsLanguageSystem), text(strings::kSettingsLanguageEnglish),
		  text(strings::kSettingsLanguageSpanish)},
		 interfaceLanguage);
	auto *languageRestart = new QLabel(text(strings::kSettingsLanguageRestart), general);
	languageRestart->setObjectName(QStringLiteral("pluginLanguageRestartHint"));
	languageRestart->setProperty("class", QStringLiteral("supporting"));
	languageRestart->setWordWrap(true);
	general->contentLayout()->addWidget(languageRestart);
	addToggle(general, strings::kSettingsStartWithObs, QStringLiteral("startWithObsCheck"), settings.startWithObs);
	addToggle(general, strings::kSettingsOpenAtStart, QStringLiteral("openDockAtStartupCheck"),
		  settings.openDockAtStartup);
	addToggle(general, strings::kSettingsAutoReplay, QStringLiteral("autoStartReplayCheck"),
		  settings.autoStartReplayBuffer);
	addToggle(general, strings::kSettingsConfirmDelete, QStringLiteral("confirmDeleteCheck"),
		  settings.confirmBeforeDelete);

	auto *profile = addSection(strings::kSettingsProfile, QStringLiteral("settingsProfileSection"));
	profile->setHelpText(text(strings::kSettingsProfileDescription));
	auto *profileDescription = new QLabel(text(strings::kSettingsProfileDescription), profile);
	profileDescription->setObjectName(QStringLiteral("settingsProfileDescription"));
	profileDescription->setWordWrap(true);
	profileDescription->setProperty("class", QStringLiteral("supporting"));
	profile->contentLayout()->addWidget(profileDescription);
	auto *profileActions = new QWidget(profile);
	profileActions->setObjectName(QStringLiteral("settingsProfileActions"));
	auto *profileActionsLayout = new QHBoxLayout(profileActions);
	profileActionsLayout->setContentsMargins(0, 0, 0, 0);
	profileActionsLayout->setSpacing(tokens::kSpaceSm);
	auto *exportProfile = new QPushButton(text(strings::kSettingsProfileExport), profileActions);
	exportProfile->setObjectName(QStringLiteral("exportSettingsProfileButton"));
	exportProfile->setProperty("controlRole", QStringLiteral("primary"));
	auto *importProfile = new QPushButton(text(strings::kSettingsProfileImport), profileActions);
	importProfile->setObjectName(QStringLiteral("importSettingsProfileButton"));
	importProfile->setProperty("controlRole", QStringLiteral("secondary"));
	profileActionsLayout->addWidget(exportProfile, 1);
	profileActionsLayout->addWidget(importProfile, 1);
	profile->contentLayout()->addWidget(profileActions);
	auto *openSetup = new QPushButton(text(strings::kSettingsSetupOpen), profile);
	openSetup->setObjectName(QStringLiteral("openInitialSetupButton"));
	openSetup->setProperty("controlRole", QStringLiteral("secondary"));
	profile->contentLayout()->addWidget(openSetup);
	connect(openSetup, &QPushButton::clicked, this, [this] {
		if (setupRequestedCallback_)
			setupRequestedCallback_();
	});
	profileStatus_ = new QLabel(profile);
	profileStatus_->setObjectName(QStringLiteral("settingsProfileStatus"));
	profileStatus_->setWordWrap(true);
	profileStatus_->hide();
	profile->contentLayout()->addWidget(profileStatus_);
	connect(exportProfile, &QPushButton::clicked, this, [this] {
		const auto suggested = QDir::homePath() + QStringLiteral("/clipxtudio-profile.json");
		auto path = QFileDialog::getSaveFileName(this, text(strings::kSettingsProfileExportDialog), suggested,
							 text(strings::kSettingsProfileJsonFilter));
		if (path.isEmpty())
			return;
		if (!path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
			path += QStringLiteral(".json");
		std::string error;
		const bool saved = exportProfileTo(std::filesystem::u8path(path.toUtf8().constData()), &error);
		profileStatus_->setProperty("notificationTone",
					    saved ? QStringLiteral("success") : QStringLiteral("error"));
		profileStatus_->setText(
			saved ? text(strings::kSettingsProfileExported).arg(path)
			      : text(strings::kSettingsProfileExportFailed).arg(QString::fromStdString(error)));
		profileStatus_->style()->unpolish(profileStatus_);
		profileStatus_->style()->polish(profileStatus_);
		profileStatus_->show();
	});
	connect(importProfile, &QPushButton::clicked, this, [this] {
		const auto path = QFileDialog::getOpenFileName(this, text(strings::kSettingsProfileImportDialog),
							       QDir::homePath(),
							       text(strings::kSettingsProfileJsonFilter));
		if (path.isEmpty())
			return;
		if (QMessageBox::question(this, text(strings::kSettingsProfileImportConfirmTitle),
					  text(strings::kSettingsProfileImportConfirmBody),
					  QMessageBox::Yes | QMessageBox::Cancel,
					  QMessageBox::Cancel) != QMessageBox::Yes)
			return;
		std::string error;
		const bool loaded = importProfileFrom(std::filesystem::u8path(path.toUtf8().constData()), &error);
		if (!loaded) {
			profileStatus_->setProperty("notificationTone", QStringLiteral("error"));
			profileStatus_->setText(
				text(strings::kSettingsProfileImportFailed).arg(QString::fromStdString(error)));
			profileStatus_->style()->unpolish(profileStatus_);
			profileStatus_->style()->polish(profileStatus_);
			profileStatus_->show();
			return;
		}
		QMessageBox::information(this, text(strings::kSettingsProfileImportedTitle),
					 text(strings::kSettingsProfileImportedBody));
		QTimer::singleShot(0, this, [this] {
			if (profileImportedCallback_ && !profileImportChangedLanguage_)
				profileImportedCallback_();
		});
	});

	auto *replay = addSection(strings::kVerticalReplayTitle, QStringLiteral("settingsReplayPerformanceSection"));
	replay->setHelpText(text("Settings.Section.Help.settingsReplayPerformanceSection") + QStringLiteral("\n\n") +
			    text(strings::kVerticalReplayPipelineValue));

	replayEncoder_ = new WheelSafeComboBox(replay);
	replayEncoder_->setObjectName(QStringLiteral("settingsReplayEncoderCombo"));
	replayEncoder_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
	replayEncoder_->setMinimumContentsLength(28);
	replayEncoder_->setMinimumWidth(280);
	replayEncoder_->setSizePolicy(QSizePolicy::Expanding, replayEncoder_->sizePolicy().verticalPolicy());
	replayEncoder_->view()->setTextElideMode(Qt::ElideNone);
	auto *replayEncoderRow = pathFormRow(text(strings::kVerticalReplayEncoder),
					     text("Settings.Help.settingsReplayEncoderCombo"), replayEncoder_, replay);
	replayEncoderRow->setObjectName(QStringLiteral("SettingsWideControlRow"));
	replayEncoderRow->setProperty("settingsRole", QStringLiteral("wideControlRow"));
	// pathFormRow expands path-like controls to the full card width. Restore a
	// useful floor afterwards so compact layouts cannot collapse the encoder
	// name into an unreadable abbreviation.
	replayEncoder_->setMinimumWidth(280);
	replay->contentLayout()->addWidget(replayEncoderRow);

	replayEnabled_ = new QCheckBox(replay);
	replayEnabled_->setObjectName(QStringLiteral("settingsReplayEnabledCheck"));
	replayEnabled_->setAccessibleName(text(strings::kVerticalReplayEnabled));
	replay->contentLayout()->addWidget(formRow(text(strings::kVerticalReplayEnabled),
						   text("Settings.Help.settingsReplayEnabledCheck"), replayEnabled_,
						   replay));

	auto *replayActions = new QWidget(replay);
	replayActions->setObjectName(QStringLiteral("settingsReplayActions"));
	auto *replayActionsLayout = new QHBoxLayout(replayActions);
	replayActionsLayout->setContentsMargins(0, 0, 0, 0);
	replayActionsLayout->setSpacing(tokens::kSpaceSm);
	applyReplayProfile_ = new QPushButton(text(strings::kVerticalReplayApply), replayActions);
	applyReplayProfile_->setObjectName(QStringLiteral("settingsReplayApplyButton"));
	applyReplayProfile_->setProperty("controlRole", QStringLiteral("primary"));
	replayActionsLayout->addWidget(applyReplayProfile_);
	replayActionsLayout->addStretch(1);
	replay->contentLayout()->addWidget(replayActions);

	replayProfileStatus_ = new QLabel(replay);
	replayProfileStatus_->setObjectName(QStringLiteral("settingsReplayProfileStatus"));
	replayProfileStatus_->setWordWrap(true);
	replayProfileStatus_->setProperty("notificationTone", QStringLiteral("info"));
	replay->contentLayout()->addWidget(replayProfileStatus_);

	auto *paths = addSection(strings::kSettingsPaths, QStringLiteral("settingsPathsSection"));
	addPath(paths, strings::kSettingsClipsFolder, QStringLiteral("clipDirectoryEdit"), settings.clipDirectory);
	addPath(paths, strings::kSettingsExportsFolder, QStringLiteral("exportDirectoryEdit"),
		settings.exportDirectory);
	addPath(paths, strings::kSettingsThumbnailsFolder, QStringLiteral("thumbnailDirectoryEdit"),
		settings.thumbnailDirectory);

	auto *names = addSection(strings::kSettingsFileNames, QStringLiteral("settingsFileNamesSection"));
	addToggle(names, strings::kSettingsIncludeDate, QStringLiteral("includeDateCheck"),
		  settings.includeDateInFileName);
	addToggle(names, strings::kSettingsIncludeScore, QStringLiteral("includeScoreCheck"),
		  settings.includeScoreInFileName);
	addToggle(names, strings::kSettingsIncludeTrigger, QStringLiteral("includeTriggerCheck"),
		  settings.includeTriggerInFileName);
	addToggle(names, strings::kSettingsIncludeOrientation, QStringLiteral("includeOrientationCheck"),
		  settings.includeOrientationInFileName);
	addText(names, strings::kSettingsNameTemplate, QStringLiteral("fileNameTemplateEdit"),
		QString::fromStdString(settings.fileNameTemplate));

	auto *notifications =
		addSection(strings::kSettingsNotificationsSection, QStringLiteral("settingsNotificationsSection"));
	addToggle(notifications, strings::kSettingsInsideObs, QStringLiteral("insideObsNotificationCheck"),
		  settings.notificationsEnabled);
	addCombo(notifications, strings::kSettingsDetail, QStringLiteral("notificationDetailCombo"),
		 {text(strings::kSettingsCompact), text(strings::kSettingsStandard), text(strings::kSettingsDetailed)},
		 static_cast<int>(settings.notificationDetail));

	auto *hotkeys = addSection(strings::kSettingsHotkeys, QStringLiteral("settingsHotkeysSection"));
	hotkeys->setHelpText(text(strings::kSettingsHotkeysManagedByObs));
	addHotkey(hotkeys, strings::kSettingsMarkMomentHotkey, QStringLiteral("markMomentHotkeyEdit"),
		  settings.markMomentHotkey);
	addHotkey(hotkeys, strings::kSettingsSave15Hotkey, QStringLiteral("save15HotkeyEdit"), settings.save15Hotkey);
	addHotkey(hotkeys, strings::kSettingsSave30Hotkey, QStringLiteral("save30HotkeyEdit"), settings.save30Hotkey);
	addHotkey(hotkeys, strings::kSettingsSave60Hotkey, QStringLiteral("save60HotkeyEdit"), settings.save60Hotkey);
	addHotkey(hotkeys, strings::kSettingsSaveVerticalHotkey, QStringLiteral("saveVerticalHotkeyEdit"),
		  settings.saveVerticalHotkey);
	addHotkey(hotkeys, strings::kSettingsOpenDesignerHotkey, QStringLiteral("openDesignerHotkeyEdit"),
		  settings.openVerticalDesignerHotkey);

	auto *exportSection = addSection(strings::kSettingsExport, QStringLiteral("settingsExportSection"));
	addCombo(exportSection, strings::kSettingsFormat, QStringLiteral("exportFormatCombo"),
		 {QStringLiteral("MP4"), QStringLiteral("MOV")}, static_cast<int>(settings.exportContainer));
	addCombo(exportSection, strings::kSettingsCodec, QStringLiteral("exportCodecCombo"),
		 {QStringLiteral("H.264"), QStringLiteral("HEVC"), QStringLiteral("AV1")},
		 static_cast<int>(settings.exportCodec));
	addCombo(exportSection, strings::kSettingsFps, QStringLiteral("exportFpsCombo"),
		 {QString::number(settings_constraints::kSupportedExportFps[0]),
		  QString::number(settings_constraints::kSupportedExportFps[1])},
		 settings.exportFps == 60 ? 1 : 0);
	addCombo(exportSection, strings::kSettingsQuality, QStringLiteral("exportQualityCombo"),
		 {text(strings::kSettingsLow), text(strings::kSettingsBalanced), text(strings::kSettingsHigh),
		  text(strings::kSettingsMaximum)},
		 settings.exportQuality == ExportQuality::Low ? 0 : static_cast<int>(settings.exportQuality) + 1);
	auto *verticalResolution = addCombo(exportSection, strings::kSettingsVerticalResolution,
					    QStringLiteral("verticalResolutionCombo"),
					    {QStringLiteral("1080p · %1 × %2")
						     .arg(settings_constraints::kPortrait1080Width)
						     .arg(settings_constraints::kPortrait1080Height),
					     QStringLiteral("2K · %1 × %2")
						     .arg(settings_constraints::kPortrait1440Width)
						     .arg(settings_constraints::kPortrait1440Height),
					     QStringLiteral("4K · %1 × %2")
						     .arg(settings_constraints::kPortrait2160Width)
						     .arg(settings_constraints::kPortrait2160Height),
					     QStringLiteral("8K · %1 × %2")
						     .arg(settings_constraints::kPortrait4320Width)
						     .arg(settings_constraints::kPortrait4320Height),
					     QStringLiteral("720p · %1 × %2")
						     .arg(settings_constraints::kPortrait720Width)
						     .arg(settings_constraints::kPortrait720Height),
					     text(strings::kVerticalCustom)},
					    0);
	static constexpr VerticalResolution resolutionValues[] = {
		VerticalResolution::Portrait1080, VerticalResolution::Portrait1440, VerticalResolution::Portrait2160,
		VerticalResolution::Portrait4320, VerticalResolution::Portrait720,  VerticalResolution::Custom};
	for (int index = 0; index < verticalResolution->count(); ++index)
		verticalResolution->setItemData(index, static_cast<int>(resolutionValues[index]));
	const auto selectedResolution = verticalResolution->findData(static_cast<int>(settings.verticalResolution));
	verticalResolution->setCurrentIndex(selectedResolution < 0 ? 0 : selectedResolution);
	verticalResolution->setProperty("lastValidValue", verticalResolution->currentIndex());

	auto *integrations = addSection(strings::kSettingsIntegrations, QStringLiteral("settingsIntegrationsSection"));
	integrations->contentLayout()->addWidget(new IntegrationsPanel(translator_, chatManager_, {}, integrations));

	auto *ai = addSection(strings::kSettingsAiAssistant, QStringLiteral("settingsAiSection"));
	auto *aiEnabled = addToggle(ai, strings::kSettingsAiEnabled, QStringLiteral("aiAssistantEnabledCheck"),
				    settings.aiAssistantEnabled);
	auto *aiConsent = addToggle(ai, strings::kSettingsAiConsent, QStringLiteral("aiPrivacyConsentCheck"),
				    settings.aiPrivacyConsent);
	auto *privacy = new QLabel(text(strings::kSettingsAiPrivacyNotice), ai);
	privacy->setObjectName(QStringLiteral("aiPrivacyNotice"));
	privacy->setWordWrap(true);
	privacy->setProperty("class", QStringLiteral("supporting"));
	ai->contentLayout()->addWidget(privacy);
	auto *aiLanguage = addCombo(ai, strings::kSettingsAiLanguage, QStringLiteral("aiLanguageCombo"),
				    {text(strings::kSettingsAiAuto), text(strings::kSettingsAiSpanish),
				     text(strings::kSettingsAiEnglish)},
				    settings.aiLanguage == "es"   ? 1
				    : settings.aiLanguage == "en" ? 2
								  : 0);
	const bool pro = licenseManager_ != nullptr && licenseManager_->snapshot().proEnabled();
	aiEnabled->setEnabled(pro);
	aiConsent->setEnabled(pro);
	aiLanguage->setEnabled(pro);
	if (!pro) {
		auto *banner = new UpgradeBanner(text(strings::kUpgradeTitle), text(strings::kUpgradeDescription),
						 text(strings::kUpgradeAction), text(strings::kPro), ai);
		banner->setObjectName(QStringLiteral("aiUpgradeBanner"));
		ai->contentLayout()->addWidget(banner);
	}

	auto *account = addSection(strings::kSettingsProAccount, QStringLiteral("settingsProAccountSection"));
	auto *remote = addSection(strings::kSettingsRemoteClipper, QStringLiteral("settingsRemoteClipperSection"));
	remote->setHelpText(text(strings::kSettingsRemoteDescription));
	addToggle(remote, strings::kSettingsRemoteAllowCommands, QStringLiteral("remoteCommandsEnabledCheck"),
		  settings.remoteCommandsEnabled);
	auto *statusGrid = new RemoteStatusGrid(remote);
	statusGrid->addTile(remoteStatusTile(text(strings::kSettingsRemoteConnectionTitle), remoteConnectionStatus_,
					     text(strings::kSettingsRemoteOffline),
					     QStringLiteral("remoteClipperConnectionStatus"), statusGrid));
	statusGrid->addTile(remoteStatusTile(text(strings::kSettingsRemoteSessionTitle), remoteSessionStatus_,
					     text(strings::kSettingsRemoteNoSession),
					     QStringLiteral("remoteClipperSessionStatus"), statusGrid));
	statusGrid->addTile(remoteStatusTile(text(strings::kSettingsRemoteHeartbeatTitle), remoteHeartbeatStatus_,
					     text(strings::kSettingsRemoteNoHeartbeat),
					     QStringLiteral("remoteClipperHeartbeatStatus"), statusGrid));
	remote->contentLayout()->addWidget(statusGrid);
	remoteAuthenticateButton_ = new QPushButton(text(strings::kSettingsRemoteAuthenticate), remote);
	remoteAuthenticateButton_->setObjectName(QStringLiteral("remoteClipperAuthenticateButton"));
	remoteAuthenticateButton_->setProperty("buttonRole", QStringLiteral("primary"));
	remoteAuthenticateButton_->setEnabled(licenseManager_ != nullptr);
	remoteAuthenticateButton_->hide();
	remote->contentLayout()->addWidget(remoteAuthenticateButton_);
	remoteAuthenticationMessage_ = new QLabel(remote);
	remoteAuthenticationMessage_->setObjectName(QStringLiteral("remoteClipperAuthenticationMessage"));
	remoteAuthenticationMessage_->setWordWrap(true);
	remoteAuthenticationMessage_->hide();
	remote->contentLayout()->addWidget(remoteAuthenticationMessage_);
	auto *remoteAccountButton = new QPushButton(text(strings::kSettingsRemoteOpenAccount), remote);
	remoteAccountButton->setObjectName(QStringLiteral("remoteClipperOpenAccountButton"));
	remoteAccountButton->setProperty("buttonRole", QStringLiteral("secondary"));
	remote->contentLayout()->addWidget(remoteAccountButton);
	remoteAccountButton->hide();
	connect(remoteAccountButton, &QPushButton::clicked, this, [this] {
		if (remoteClipperOpenRequestedCallback_)
			remoteClipperOpenRequestedCallback_();
	});
	QUrl membershipUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL));
	membershipUrl.setPath(QStringLiteral("/account"));
	membershipUrl.setFragment(QString());
	account->contentLayout()->addWidget(new ProAccountTab(translator_, licenseManager_, membershipUrl, account));

	auto *support = addSection(strings::kSettingsSupport, QStringLiteral("settingsSupportSection"));
	support->setHelpText(text(strings::kSettingsSupportDescription));

	auto supportUrl = [](const QString &type) {
		QUrl url(QStringLiteral(CLIPX_SERVICE_BASE_URL));
		url.setPath(QStringLiteral("/support"));
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("type"), type);
		query.addQueryItem(QStringLiteral("source"), QStringLiteral("plugin"));
		query.addQueryItem(QStringLiteral("app_version"), QStringLiteral(CLIPCOACH_VERSION));
		query.addQueryItem(QStringLiteral("os"), QSysInfo::prettyProductName());
		url.setQuery(query);
		url.setFragment(QStringLiteral("support-form"));
		return url;
	};
	auto *reportBug = new QPushButton(text(strings::kSettingsReportBug), support);
	reportBug->setObjectName(QStringLiteral("reportBugButton"));
	reportBug->setProperty("controlRole", QStringLiteral("primary"));
	connect(reportBug, &QPushButton::clicked, support,
		[supportUrl] { QDesktopServices::openUrl(supportUrl(QStringLiteral("bug"))); });
	auto *sendSuggestion = new QPushButton(text(strings::kSettingsSendSuggestion), support);
	sendSuggestion->setObjectName(QStringLiteral("sendSuggestionButton"));
	sendSuggestion->setProperty("controlRole", QStringLiteral("secondary"));
	connect(sendSuggestion, &QPushButton::clicked, support,
		[supportUrl] { QDesktopServices::openUrl(supportUrl(QStringLiteral("suggestion"))); });
	auto *supportActions = new QWidget(support);
	supportActions->setObjectName(QStringLiteral("settingsSupportActions"));
	auto *supportActionsLayout = new QHBoxLayout(supportActions);
	supportActionsLayout->setContentsMargins(0, 0, 0, 0);
	supportActionsLayout->setSpacing(tokens::kSpaceSm);
	supportActionsLayout->addWidget(reportBug, 1);
	supportActionsLayout->addWidget(sendSuggestion, 1);
	support->contentLayout()->addWidget(supportActions);

	layout->addStretch(1);
	scroll->setWidget(content);
	root->addWidget(scroll);
	updateResponsiveLayout();
}

void SettingsTab::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);
	updateResponsiveLayout();
}

void SettingsTab::updateResponsiveLayout()
{
	const bool compact = width() < 620;
	compactLayout_ = compact;
	if (auto *profileActions = findChild<QWidget *>(QStringLiteral("settingsProfileActions"))) {
		if (auto *actions = qobject_cast<QBoxLayout *>(profileActions->layout()))
			actions->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
	}
	if (widthHint_ != nullptr)
		widthHint_->setVisible(compact);

	const int availableWidth = sectionsContainer_ != nullptr ? qMax(1, sectionsContainer_->width())
								 : qMax(1, width());
	const int columns = availableWidth >= 1600 ? 4 : availableWidth >= 1080 ? 3 : availableWidth >= 640 ? 2 : 1;
	if (sectionsGrid_ != nullptr && columns != sectionColumns_) {
		while (auto *item = sectionsGrid_->takeAt(0))
			delete item;
		for (int column = 0; column < 4; ++column)
			sectionsGrid_->setColumnStretch(column, 0);
		for (std::size_t index = 0; index < sections_.size(); ++index) {
			sectionsGrid_->addWidget(sections_[index], static_cast<int>(index) / columns,
						 static_cast<int>(index) % columns);
		}
		for (int column = 0; column < columns; ++column)
			sectionsGrid_->setColumnStretch(column, 1);
		sectionColumns_ = columns;
	}
	const auto fieldHelpLabels = findChildren<QLabel *>();
	for (auto *help : fieldHelpLabels) {
		if (help->property("settingsRole").toString() == QStringLiteral("fieldHelp"))
			help->setVisible(columns <= 2);
	}

	const auto rows = findChildren<QWidget *>(QStringLiteral("SettingsFormRow"));
	for (auto *row : rows) {
		auto *box = qobject_cast<QBoxLayout *>(row->layout());
		if (box == nullptr)
			continue;
		box->setDirection(compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
		if (box->count() < 2)
			continue;
		auto *control = box->itemAt(1)->widget();
		if (control == nullptr || qobject_cast<QCheckBox *>(control) != nullptr)
			continue;
		control->setMinimumWidth(compact ? 0 : 118);
		control->setSizePolicy(QSizePolicy::Expanding, control->sizePolicy().verticalPolicy());
	}
}

void SettingsTab::refreshLicenseState()
{
	const bool pro = licenseManager_ != nullptr && licenseManager_->snapshot().proEnabled();
	for (const auto *name : {"aiAssistantEnabledCheck", "aiPrivacyConsentCheck", "aiLanguageCombo"}) {
		if (auto *control = findChild<QWidget *>(QString::fromLatin1(name)))
			control->setEnabled(pro);
	}
	if (auto *banner = findChild<QWidget *>(QStringLiteral("aiUpgradeBanner")))
		banner->setVisible(!pro);
}

void SettingsTab::setRemoteClipperStatus(const remote::RemoteClipperStatus &status)
{
	if (remoteConnectionStatus_ == nullptr || remoteSessionStatus_ == nullptr || remoteHeartbeatStatus_ == nullptr)
		return;
	QString connection = text(strings::kSettingsRemoteOffline);
	QString tone = QStringLiteral("warning");
	switch (status.connection) {
	case remote::RemoteConnectionState::Connected:
		connection = text(strings::kSettingsRemoteConnected);
		tone = QStringLiteral("success");
		break;
	case remote::RemoteConnectionState::Connecting:
		connection = text(strings::kSettingsRemoteConnecting);
		tone = QStringLiteral("info");
		break;
	case remote::RemoteConnectionState::Unauthorized:
		connection = status.errorCode == "REMOTE_ADDON_REQUIRED" ? text(strings::kSettingsRemoteAddonRequired)
			     : status.errorCode == "PRO_REQUIRED"        ? text(strings::kSettingsRemoteProRequired)
									 : text(strings::kSettingsRemoteUnauthorized);
		tone = QStringLiteral("error");
		break;
	case remote::RemoteConnectionState::Unavailable:
	case remote::RemoteConnectionState::Paused:
		connection = text(strings::kSettingsRemotePaused);
		break;
	case remote::RemoteConnectionState::Offline:
		break;
	}
	remoteConnectionStatus_->setText(QStringLiteral("%1\n%2").arg(
		connection, text(strings::kSettingsRemotePollInterval).arg(status.pollIntervalSeconds)));
	remoteConnectionStatus_->setProperty("notificationTone", tone);
	remoteConnectionStatus_->style()->unpolish(remoteConnectionStatus_);
	remoteConnectionStatus_->style()->polish(remoteConnectionStatus_);
	remoteSessionStatus_->setText(
		status.sessionId.empty()
			? text(strings::kSettingsRemoteNoSession)
			: text(strings::kSettingsRemoteSession).arg(QString::fromStdString(status.sessionId)));
	if (status.lastHeartbeatAt.has_value()) {
		const auto seconds =
			std::max<long long>(0, std::chrono::duration_cast<std::chrono::seconds>(
						       std::chrono::system_clock::now() - *status.lastHeartbeatAt)
						       .count());
		remoteHeartbeatStatus_->setText(text(strings::kSettingsRemoteLastHeartbeat).arg(seconds));
	} else {
		remoteHeartbeatStatus_->setText(text(strings::kSettingsRemoteNoHeartbeat));
	}
	const bool authorizationRequired = status.connection == remote::RemoteConnectionState::Unauthorized &&
					   status.errorCode != "REMOTE_ADDON_REQUIRED" &&
					   status.errorCode != "PRO_REQUIRED";
	if (remoteAuthenticateButton_ != nullptr) {
		remoteAuthenticateButton_->setVisible(false);
		remoteAuthenticateButton_->setEnabled(!remoteAuthenticationPending_ && licenseManager_ != nullptr);
	}
	if (!authorizationRequired && !remoteAuthenticationPending_ && remoteAuthenticationMessage_ != nullptr)
		remoteAuthenticationMessage_->hide();
}

void SettingsTab::setRemoteAuthenticationRequestedCallback(RemoteAuthenticationRequestedCallback callback)
{
	remoteAuthenticationRequestedCallback_ = std::move(callback);
}

void SettingsTab::setRemoteClipperOpenRequestedCallback(RemoteClipperOpenRequestedCallback callback)
{
	remoteClipperOpenRequestedCallback_ = std::move(callback);
}

void SettingsTab::setRemoteAuthenticationBusy(bool busy)
{
	remoteAuthenticationPending_ = busy;
	if (remoteAuthenticateButton_ == nullptr)
		return;
	remoteAuthenticateButton_->setText(
		text(busy ? strings::kSettingsRemoteAuthenticating : strings::kSettingsRemoteAuthenticate));
	remoteAuthenticateButton_->setEnabled(!busy && licenseManager_ != nullptr);
	if (busy)
		remoteAuthenticateButton_->hide();
}

void SettingsTab::setRemoteCommandsEnabled(bool enabled)
{
	if (auto *control = findChild<QCheckBox *>(QStringLiteral("remoteCommandsEnabledCheck"))) {
		const QSignalBlocker blocker(control);
		control->setChecked(enabled);
	}
}

void SettingsTab::bindUi()
{
	auto check = [this](const char *name, const std::function<void(Settings &, bool)> &setter) {
		auto *control = findChild<QCheckBox *>(QString::fromLatin1(name));
		connect(control, &QCheckBox::toggled, this, [this, control, setter](bool value) {
			if (!controller_->update([&](Settings &settings) { setter(settings, value); })) {
				const QSignalBlocker blocker(control);
				control->setChecked(!value);
			}
		});
	};
	check("startWithObsCheck", [](Settings &s, bool v) { s.startWithObs = v; });
	check("openDockAtStartupCheck", [](Settings &s, bool v) { s.openDockAtStartup = v; });
	check("autoStartReplayCheck", [](Settings &s, bool v) { s.autoStartReplayBuffer = v; });
	check("remoteCommandsEnabledCheck", [](Settings &s, bool v) { s.remoteCommandsEnabled = v; });
	connect(remoteAuthenticateButton_, &QPushButton::clicked, this, [this] {
		if (remoteAuthenticationRequestedCallback_)
			remoteAuthenticationRequestedCallback_();
	});
	check("confirmDeleteCheck", [](Settings &s, bool v) { s.confirmBeforeDelete = v; });
	check("includeDateCheck", [](Settings &s, bool v) { s.includeDateInFileName = v; });
	check("includeScoreCheck", [](Settings &s, bool v) { s.includeScoreInFileName = v; });
	check("includeTriggerCheck", [](Settings &s, bool v) { s.includeTriggerInFileName = v; });
	check("includeOrientationCheck", [](Settings &s, bool v) { s.includeOrientationInFileName = v; });
	check("insideObsNotificationCheck", [](Settings &s, bool v) { s.notificationsEnabled = v; });
	check("aiAssistantEnabledCheck", [](Settings &s, bool v) { s.aiAssistantEnabled = v; });
	check("aiPrivacyConsentCheck", [](Settings &s, bool v) { s.aiPrivacyConsent = v; });

	auto combo = [this](const char *name, const std::function<void(Settings &, int)> &setter) {
		auto *control = findChild<QComboBox *>(QString::fromLatin1(name));
		connect(control, &QComboBox::currentIndexChanged, this, [this, control, setter](int value) {
			if (controller_->update([&](Settings &settings) { setter(settings, value); })) {
				control->setProperty("lastValidValue", value);
			} else {
				const QSignalBlocker blocker(control);
				control->setCurrentIndex(control->property("lastValidValue").toInt());
			}
		});
	};
	combo("pluginLanguageCombo", [](Settings &s, int v) {
		static constexpr const char *values[] = {"system", "en-US", "es-ES"};
		s.language = values[v];
	});
	combo("notificationDetailCombo",
	      [](Settings &s, int v) { s.notificationDetail = static_cast<NotificationDetail>(v); });
	combo("exportFormatCombo", [](Settings &s, int v) { s.exportContainer = static_cast<ExportContainer>(v); });
	combo("exportCodecCombo", [](Settings &s, int v) { s.exportCodec = static_cast<ExportCodec>(v); });
	combo("exportFpsCombo", [](Settings &s, int v) { s.exportFps = settings_constraints::kSupportedExportFps[v]; });
	combo("exportQualityCombo", [](Settings &s, int v) {
		static constexpr ExportQuality values[] = {ExportQuality::Low, ExportQuality::Medium,
							   ExportQuality::High, ExportQuality::Maximum};
		s.exportQuality = values[v];
	});
	auto *verticalResolution = findChild<QComboBox *>(QStringLiteral("verticalResolutionCombo"));
	connect(verticalResolution, &QComboBox::currentIndexChanged, this, [this, verticalResolution](int index) {
		const auto value = static_cast<VerticalResolution>(verticalResolution->itemData(index).toInt());
		if (controller_->update([&](Settings &settings) {
			    settings.verticalResolution = value;
			    if (value != VerticalResolution::Custom) {
				    const auto dimensions = verticalResolutionDimensions(value);
				    settings.verticalWidth = dimensions.width;
				    settings.verticalHeight = dimensions.height;
			    }
		    })) {
			verticalResolution->setProperty("lastValidValue", index);
		} else {
			const QSignalBlocker blocker(verticalResolution);
			verticalResolution->setCurrentIndex(verticalResolution->property("lastValidValue").toInt());
		}
	});
	combo("aiLanguageCombo", [](Settings &s, int v) {
		static constexpr const char *values[] = {"auto", "es", "en"};
		s.aiLanguage = values[v];
	});

	auto *templateEdit = findChild<QLineEdit *>(QStringLiteral("fileNameTemplateEdit"));
	connect(templateEdit, &QLineEdit::editingFinished, this, [this, templateEdit] {
		const auto before = QString::fromStdString(controller_->settings().fileNameTemplate);
		if (!controller_->update([&](Settings &settings) {
			    settings.fileNameTemplate = templateEdit->text().toStdString();
		    })) {
			templateEdit->setText(before);
		}
	});

	struct PathBinding {
		const char *name;
		std::function<void(Settings &, const std::filesystem::path &)> setter;
	};
	for (const auto &binding :
	     std::initializer_list<PathBinding>{{"clipDirectoryEdit",
						 [](Settings &s, const auto &p) {
							 s.clipDirectory = p;
						 }},
						{"exportDirectoryEdit",
						 [](Settings &s, const auto &p) {
							 s.exportDirectory = p;
						 }},
						{"thumbnailDirectoryEdit", [](Settings &s, const auto &p) {
							 s.thumbnailDirectory = p;
						 }}}) {
		auto *field = findChild<QLineEdit *>(QString::fromLatin1(binding.name));
		auto *change = findChild<QPushButton *>(QString::fromLatin1(binding.name) + QStringLiteral("Change"));
		auto *open = findChild<QPushButton *>(QString::fromLatin1(binding.name) + QStringLiteral("Open"));
		connect(change, &QPushButton::clicked, this,
			[this, field, setter = binding.setter] { chooseDirectory(field, setter); });
		connect(open, &QPushButton::clicked, this, [this, field] { openDirectory(field); });
	}

	struct HotkeyBinding {
		const char *name;
		std::string Settings::*member;
	};
	for (const auto &binding :
	     std::initializer_list<HotkeyBinding>{{"markMomentHotkeyEdit", &Settings::markMomentHotkey},
						  {"save15HotkeyEdit", &Settings::save15Hotkey},
						  {"save30HotkeyEdit", &Settings::save30Hotkey},
						  {"save60HotkeyEdit", &Settings::save60Hotkey},
						  {"saveVerticalHotkeyEdit", &Settings::saveVerticalHotkey},
						  {"openDesignerHotkeyEdit", &Settings::openVerticalDesignerHotkey}}) {
		auto *field = findChild<QKeySequenceEdit *>(QString::fromLatin1(binding.name));
		connect(field, &QKeySequenceEdit::editingFinished, this, [this, field, member = binding.member] {
			if (controller_->update([&](Settings &settings) {
				    settings.*member =
					    field->keySequence().toString(QKeySequence::PortableText).toStdString();
			    })) {
				field->setProperty("lastValidValue",
						   field->keySequence().toString(QKeySequence::PortableText));
			} else {
				const QSignalBlocker blocker(field);
				field->setKeySequence(QKeySequence(field->property("lastValidValue").toString()));
			}
		});
	}

	connect(applyReplayProfile_, &QPushButton::clicked, this, [this] {
		if (!replayObsBridge_.applyReplayProfile || replayEncoder_ == nullptr || replayEnabled_ == nullptr ||
		    replayProfileStatus_ == nullptr)
			return;

		const auto result = replayObsBridge_.applyReplayProfile(
			replayEncoder_->currentData().toString().toStdString(), replayEnabled_->isChecked());
		if (!result.success) {
			replayProfileStatus_->setProperty("notificationTone", QStringLiteral("error"));
			replayProfileStatus_->setText(QString::fromStdString(result.message));
			replayProfileStatus_->style()->unpolish(replayProfileStatus_);
			replayProfileStatus_->style()->polish(replayProfileStatus_);
			return;
		}

		refreshReplayProfile();
		replayProfileStatus_->setProperty("notificationTone", QStringLiteral("success"));
		replayProfileStatus_->setText(result.restartRequired ? text(strings::kVerticalReplayRestart)
								     : QString::fromStdString(result.message));
		replayProfileStatus_->style()->unpolish(replayProfileStatus_);
		replayProfileStatus_->style()->polish(replayProfileStatus_);
		if (result.restartRequired)
			showReplayRestartDialog();
	});
}

void SettingsTab::refreshReplayProfile()
{
	if (replayEncoder_ == nullptr || replayEnabled_ == nullptr || applyReplayProfile_ == nullptr ||
	    replayProfileStatus_ == nullptr)
		return;

	const bool available = static_cast<bool>(replayObsBridge_.replayEncoders) &&
			       static_cast<bool>(replayObsBridge_.replayProfile) &&
			       static_cast<bool>(replayObsBridge_.applyReplayProfile);
	replayEncoder_->setEnabled(available);
	replayEnabled_->setEnabled(available);
	applyReplayProfile_->setEnabled(available);
	if (!available) {
		replayProfileStatus_->setProperty("notificationTone", QStringLiteral("info"));
		replayProfileStatus_->setText(text(strings::kVerticalReplayUnavailable));
		replayProfileStatus_->style()->unpolish(replayProfileStatus_);
		replayProfileStatus_->style()->polish(replayProfileStatus_);
		return;
	}

	const auto profile = replayObsBridge_.replayProfile();
	const QSignalBlocker blockEncoder(replayEncoder_);
	const QSignalBlocker blockEnabled(replayEnabled_);
	replayEncoder_->clear();
	for (const auto &option : replayObsBridge_.replayEncoders()) {
		const auto label = QString::fromStdString(option.displayName) +
				   (option.hardware ? QStringLiteral(" - GPU") : QStringLiteral(" - CPU"));
		replayEncoder_->addItem(label, QString::fromStdString(option.id));
	}
	auto index = replayEncoder_->findData(QString::fromStdString(profile.encoderId));
	if (index < 0 && !profile.encoderId.empty()) {
		replayEncoder_->addItem(QString::fromStdString(profile.encoderDisplayName),
					QString::fromStdString(profile.encoderId));
		index = replayEncoder_->count() - 1;
	}
	int popupWidth = replayEncoder_->minimumWidth();
	const QFontMetrics popupMetrics(replayEncoder_->view()->font());
	for (int item = 0; item < replayEncoder_->count(); ++item) {
		const auto label = replayEncoder_->itemText(item);
		replayEncoder_->setItemData(item, label, Qt::ToolTipRole);
		popupWidth = std::max(popupWidth, popupMetrics.horizontalAdvance(label) + 48);
	}
	replayEncoder_->view()->setMinimumWidth(popupWidth);
	replayEncoder_->setCurrentIndex(index < 0 ? 0 : index);
	replayEnabled_->setChecked(profile.replayBufferEnabled);
	replayProfileStatus_->setProperty("notificationTone", profile.hardwareEncoder ? QStringLiteral("success")
										      : QStringLiteral("warning"));
	replayProfileStatus_->setText(
		(profile.hardwareEncoder ? text(strings::kVerticalReplayHardware) : text(strings::kVerticalReplayCpu))
			.arg(QString::fromStdString(profile.outputMode),
			     QString::fromStdString(profile.encoderDisplayName)));
	replayProfileStatus_->style()->unpolish(replayProfileStatus_);
	replayProfileStatus_->style()->polish(replayProfileStatus_);
}

void SettingsTab::showReplayRestartDialog()
{
	auto *dialog = new QMessageBox(QMessageBox::Question, text(strings::kVerticalReplayRestartTitle),
				       text(strings::kVerticalReplayRestartBody), QMessageBox::NoButton, this);
	dialog->setObjectName(QStringLiteral("settingsReplayRestartDialog"));
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	auto *restartButton = dialog->addButton(text(strings::kVerticalReplayRestartNow), QMessageBox::AcceptRole);
	restartButton->setObjectName(QStringLiteral("settingsReplayRestartNowButton"));
	dialog->addButton(text(strings::kVerticalReplayRestartLater), QMessageBox::RejectRole);
	restartButton->setEnabled(static_cast<bool>(replayObsBridge_.restartObs));
	connect(restartButton, &QPushButton::clicked, this, [this] {
		std::string error;
		if (!replayObsBridge_.restartObs || !replayObsBridge_.restartObs(&error)) {
			replayProfileStatus_->setProperty("notificationTone", QStringLiteral("error"));
			replayProfileStatus_->setText(QString::fromStdString(error));
		} else {
			replayProfileStatus_->setProperty("notificationTone", QStringLiteral("success"));
			replayProfileStatus_->setText(text(strings::kVerticalReplayRestartScheduled));
		}
		replayProfileStatus_->style()->unpolish(replayProfileStatus_);
		replayProfileStatus_->style()->polish(replayProfileStatus_);
	});
	dialog->setWindowModality(Qt::WindowModal);
	dialog->show();
}

QCheckBox *SettingsTab::addToggle(SettingsSection *section, const char *labelKey, const QString &objectName,
				  bool checked)
{
	auto *control = new QCheckBox(section);
	control->setObjectName(objectName);
	control->setChecked(checked);
	control->setProperty("lastValidValue", checked);
	control->setAccessibleName(text(labelKey));
	const auto key = helpKey(objectName);
	section->contentLayout()->addWidget(formRow(text(labelKey), text(key.constData()), control, section));
	return control;
}

QSpinBox *SettingsTab::addSpin(SettingsSection *section, const char *labelKey, const QString &objectName, int value,
			       int minimum, int maximum)
{
	auto *control = new WheelSafeSpinBox(section);
	control->setObjectName(objectName);
	control->setRange(minimum, maximum);
	control->setValue(value);
	control->setProperty("lastValidValue", value);
	control->setSuffix(text(strings::kSettingsSecondsSuffix));
	const auto key = helpKey(objectName);
	section->contentLayout()->addWidget(formRow(text(labelKey), text(key.constData()), control, section));
	return control;
}

QComboBox *SettingsTab::addCombo(SettingsSection *section, const char *labelKey, const QString &objectName,
				 const QStringList &options, int current)
{
	auto *control = new WheelSafeComboBox(section);
	control->setObjectName(objectName);
	control->addItems(options);
	control->setCurrentIndex(current);
	control->setProperty("lastValidValue", current);
	const auto key = helpKey(objectName);
	section->contentLayout()->addWidget(formRow(text(labelKey), text(key.constData()), control, section));
	return control;
}

QLineEdit *SettingsTab::addPath(SettingsSection *section, const char *labelKey, const QString &objectName,
				const std::filesystem::path &value)
{
	auto *container = new QWidget(section);
	container->setObjectName(objectName + QStringLiteral("Container"));
	auto *layout = new QHBoxLayout(container);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(tokens::kSpaceXs);
	auto *field = new QLineEdit(container);
	field->setObjectName(objectName);
	field->setReadOnly(true);
	field->setPlaceholderText(text(strings::kSettingsAutomaticPath));
	field->setText(QString::fromStdString(value.u8string()));
	auto *change = new QPushButton(text(strings::kSettingsChange), container);
	change->setObjectName(objectName + QStringLiteral("Change"));
	change->setProperty("controlRole", QStringLiteral("compact"));
	change->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
	change->setToolTip(text(strings::kSettingsChange));
	auto *open = new QPushButton(text(strings::kSettingsOpenFolder), container);
	open->setObjectName(objectName + QStringLiteral("Open"));
	open->setProperty("controlRole", QStringLiteral("compact"));
	open->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
	open->setToolTip(text(strings::kSettingsOpenFolder));
	open->setEnabled(!value.empty());
	layout->addWidget(field, 1);
	layout->addWidget(change);
	layout->addWidget(open);
	const auto key = helpKey(objectName);
	section->contentLayout()->addWidget(pathFormRow(text(labelKey), text(key.constData()), container, section));
	return field;
}

QLineEdit *SettingsTab::addText(SettingsSection *section, const char *labelKey, const QString &objectName,
				const QString &value)
{
	auto *control = new QLineEdit(value, section);
	control->setObjectName(objectName);
	control->setMaxLength(settings_constraints::kMaxFileNameTemplateLength);
	const auto key = helpKey(objectName);
	section->contentLayout()->addWidget(formRow(text(labelKey), text(key.constData()), control, section));
	return control;
}

QKeySequenceEdit *SettingsTab::addHotkey(SettingsSection *section, const char *labelKey, const QString &objectName,
					 const std::string &value)
{
	auto *control = new QKeySequenceEdit(QKeySequence(QString::fromStdString(value)), section);
	control->setObjectName(objectName);
	control->setProperty("lastValidValue", control->keySequence().toString(QKeySequence::PortableText));
	control->setToolTip(text(strings::kSettingsHotkeysManagedByObs));
	const auto key = helpKey(objectName);
	section->contentLayout()->addWidget(formRow(text(labelKey), text(key.constData()), control, section));
	return control;
}

SettingsSection *SettingsTab::addSection(const char *titleKey, const QString &objectName)
{
	const auto sectionNumber = static_cast<int>(sections_.size()) + 1;
	auto *section = new SettingsSection(QStringLiteral("%1. %2").arg(sectionNumber).arg(text(titleKey)),
					    sectionsContainer_ != nullptr ? sectionsContainer_ : this);
	section->setObjectName(objectName);
	section->setProperty("settingsRole", QStringLiteral("section"));
	section->setProperty("sectionTone", sectionNumber % 3 == 1   ? QStringLiteral("purple")
					    : sectionNumber % 3 == 2 ? QStringLiteral("cyan")
								     : QStringLiteral("blue"));
	section->setMinimumWidth(0);
	section->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	const auto sectionHelpKey = QStringLiteral("Settings.Section.Help.%1").arg(objectName).toUtf8();
	section->setHelpText(text(sectionHelpKey.constData()));
	sections_.push_back(section);
	return section;
}

void SettingsTab::chooseDirectory(QLineEdit *field,
				  const std::function<void(Settings &, const std::filesystem::path &)> &setter)
{
	const auto selected =
		QFileDialog::getExistingDirectory(this, text(strings::kSettingsChooseFolder), field->text());
	if (selected.isEmpty()) {
		return;
	}
	const auto path = std::filesystem::u8path(selected.toStdString());
	if (controller_->update([&](Settings &settings) { setter(settings, path); })) {
		field->setText(selected);
		auto *open = findChild<QPushButton *>(field->objectName() + QStringLiteral("Open"));
		open->setEnabled(true);
	}
}

void SettingsTab::openDirectory(QLineEdit *field)
{
	if (field->text().isEmpty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(field->text()))) {
		showError();
	}
}

void SettingsTab::showError()
{
	errorLabel_->show();
}

} // namespace clipcoach::ui
