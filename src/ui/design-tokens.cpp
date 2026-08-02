#include <clipcoach/ui/design-tokens.hpp>

#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QTimer>

namespace clipcoach::ui::tokens {

QString styleSheet()
{
	return QStringLiteral(R"QSS(
QWidget#ClipXtudioMainDockContent {
	background: #0C1016;
	color: #F4F7FB;
	font-size: 13px;
}
QDialog#quickClipEditorDialog {
	background: #0C1016;
	color: #F4F7FB;
	font-size: 13px;
}
QDialog#quickClipEditorDialog QLabel {
	color: #E8EDF5;
}
QDialog#quickClipEditorDialog QLabel[textRole="muted"] {
	color: #9FB0C7;
}
QDialog#quickClipEditorDialog QFrame[cardRole="editor"] {
	background: #101722;
	border: 1px solid #2A3950;
	border-radius: 12px;
}
QDialog#quickClipEditorDialog QLineEdit,
QDialog#quickClipEditorDialog QPlainTextEdit,
QDialog#quickClipEditorDialog QComboBox {
	background: #111925;
	color: #F4F7FB;
	border: 1px solid #304057;
	border-radius: 8px;
	padding: 8px;
}
QDialog#quickClipEditorDialog QPushButton {
	background: #172131;
	color: #F4F7FB;
	border: 1px solid #3A4A61;
	border-radius: 8px;
	padding: 8px 12px;
	font-weight: 700;
}
QDialog#quickClipEditorDialog QPushButton[buttonRole="primary"],
QDialog#quickClipEditorDialog QPushButton[controlRole="primary"] {
	background: #7C3AED;
	border-color: #9B67F2;
	color: #FFFFFF;
}
QDialog#quickClipEditorDialog QProgressBar {
	background: #263448;
	border: 0;
	border-radius: 5px;
	height: 10px;
}
QDialog#quickClipEditorDialog QProgressBar::chunk {
	background: #8B5CF6;
	border-radius: 5px;
}
QWizard#clipXtudioInitialSetup, QWizard#clipXtudioInitialSetup QWizardPage {
	background: #0C1016;
	color: #F4F7FB;
}
QWizard#clipXtudioInitialSetup QWidget {
	background-color: #0C1016;
	color: #F4F7FB;
}
QWizard#clipXtudioInitialSetup QLabel,
QWizard#clipXtudioInitialSetup QCheckBox {
	color: #E8EDF5;
}
QWizard#clipXtudioInitialSetup QLineEdit,
QWizard#clipXtudioInitialSetup QComboBox,
QWizard#clipXtudioInitialSetup QPlainTextEdit {
	background: #111925;
	color: #F4F7FB;
	border: 1px solid #304057;
	border-radius: 8px;
	padding: 8px;
}
QWizard#clipXtudioInitialSetup QPushButton[controlRole="primary"] {
	background: #7C3AED;
	color: #FFFFFF;
	border: 1px solid #9B67F2;
	border-radius: 8px;
	padding: 8px 16px;
	font-weight: 700;
}
QWizard#clipXtudioInitialSetup QPushButton[controlRole="secondary"] {
	background: #151E2B;
	color: #F4F7FB;
	border: 1px solid #394A61;
	border-radius: 8px;
	padding: 8px 16px;
	font-weight: 700;
}
QWidget#clipsTab, QWidget#clipsListContainer {
	background: #0C1016;
	color: #F4F7FB;
}
QWidget#settingsTab, QWidget#settingsContent {
	background: #0C1016;
	color: #F4F7FB;
}
QFrame[pageRole="header"] {
	background: #101722;
	border: 1px solid #29374A;
	border-radius: 12px;
}
QLabel[pageRole="icon"] {
	background: #28194A;
	color: #C084FC;
	border: 1px solid #513080;
	border-radius: 10px;
	font-size: 22px;
	font-weight: 700;
	min-width: 44px;
	max-width: 44px;
	min-height: 44px;
	max-height: 44px;
}
QWidget#settingsSectionsContainer {
	background: transparent;
}
QWidget#triggersTab, QWidget#triggersLeftColumn, QWidget#triggersRightColumn {
	background: #0C1016;
	color: #F4F7FB;
}
QFrame[triggerRole="card"] {
	background: #111925;
	border: 1px solid #2A374A;
	border-radius: 12px;
}
QWidget[triggerRole="signal"] {
	background: #101722;
	border: 1px solid #263448;
	border-radius: 8px;
}
QWidget[triggerRole="signal"] QLabel#SectionLabel {
	font-size: 12px;
	font-weight: 700;
}
QWidget[triggerRole="signal"] QLabel#SupportingText {
	color: #97A6BA;
	font-size: 10px;
}
QWidget[triggerRole="signal"]:hover {
	background: #172131;
	border-color: #65449A;
}
QWidget[triggerRole="field"] {
	background: #101722;
	border: 1px solid #263448;
	border-radius: 8px;
}
QLabel[triggerRole="event"] {
	background: #0F1621;
	color: #E8EDF5;
	border: 1px solid #263448;
	border-radius: 8px;
	padding: 4px 8px;
}
QLabel#triggerFieldTitle, QLabel#triggerKeywordFieldTitle {
	color: #F4F7FB;
	font-size: 12px;
	font-weight: 700;
}
QLabel#triggerFieldHelp {
	color: #97A6BA;
	font-size: 10px;
}
QLabel#triggerTimingSummary[triggerRole="estimateBadge"] {
	background: #2B1C49;
	color: #C9A8FF;
	border: 1px solid #6941A5;
	border-radius: 8px;
	padding: 9px 12px;
	font-weight: 800;
}
QFrame#triggerVoiceSection QListWidget,
QFrame#triggerScenesSection QListWidget {
	background: #0F1621;
	color: #E8EDF5;
	border: 1px solid #304057;
	border-radius: 8px;
	padding: 5px;
}
QFrame#triggerVoiceSection QListWidget::item,
QFrame#triggerScenesSection QListWidget::item {
	padding: 6px 8px;
	border-bottom: 1px solid #253246;
}
QFrame#triggerVoiceSection QListWidget::item:selected {
	background: #33205C;
	color: #FFFFFF;
}
QFrame#triggerVoiceSection QWidget[voiceRole="group"] {
	background: #0F1621;
	border: 1px solid #263448;
	border-radius: 9px;
}
QFrame#triggerVoiceSection QLineEdit#voicePhraseInput,
QFrame#triggerVoiceSection QPushButton#addVoicePhraseButton,
QFrame#triggerVoiceSection QPushButton#removeVoicePhraseButton {
	min-height: 42px;
}
QLabel#voiceRuntimeStatus {
	padding: 12px 16px;
}
QWidget#DockHeader {
	background: #0F141C;
	border-bottom: 1px solid #283345;
}
QWidget#GlobalFooter {
	background: #0F141C;
	border-top: 1px solid #283345;
}
QLabel#footerDeveloperCredits {
	color: #A4AEBD;
}
QLabel#headerStatusBadge {
	background: #12351F;
	color: #86EFAC;
	border: 1px solid #1C6334;
	border-radius: 6px;
	padding: 2px 8px;
	font-weight: 700;
}
QLabel#headerStatusBadge[notificationTone="error"] {
	background: #3A171B;
	color: #FCA5A5;
	border-color: #7F2931;
}
QLabel#headerCapturePendingDot,
QLabel#clipPendingDot {
	background: #EF4444;
	border: 1px solid #FCA5A5;
	border-radius: 5px;
	min-width: 10px;
	max-width: 10px;
	min-height: 10px;
	max-height: 10px;
}
QLabel#headerCapturePendingDot[pulseOn="false"],
QLabel#clipPendingDot[pulseOn="false"] {
	background: #7F1D1D;
	border-color: #991B1B;
}
QPushButton#footerReplayBufferButton,
QPushButton#footerSaveClipButton,
QPushButton#footerCheckForUpdatesButton {
	min-height: 32px;
	max-height: 32px;
	padding-top: 0;
	padding-bottom: 0;
}
QPushButton#headerRemoteAuthenticateButton[authenticated="true"],
QPushButton#headerRemoteOpenButton[remoteState="online"] {
	background: #12351F;
	color: #86EFAC;
	border: 1px solid #1C6334;
}
QPushButton#headerRemoteAuthenticateButton[authenticated="true"]:hover,
QPushButton#headerRemoteOpenButton[remoteState="online"]:hover {
	background: #194B2B;
	border-color: #2A8A49;
}
QPushButton#headerRemoteOpenButton[remoteState="offline"] {
	background: #3A171B;
	color: #FCA5A5;
	border: 1px solid #7F2931;
}
QPushButton#headerRemoteOpenButton[remoteState="offline"]:hover {
	background: #512026;
	border-color: #A83A45;
}
QLabel#updateAvailableIndicator {
	background: #EF4444;
	border: 1px solid #FCA5A5;
	border-radius: 5px;
	min-width: 10px;
	max-width: 10px;
	min-height: 10px;
	max-height: 10px;
}
QLabel#updateAvailableIndicator[pulseOn="false"] {
	background: #7F1D1D;
	border-color: #991B1B;
}
QLabel#verticalDirectEditHelp {
	background: #151F2D;
	color: #BFD0E8;
	border: 1px solid #30415A;
	border-radius: 7px;
	padding: 8px 10px;
}
QLabel#BrandMark {
	background: transparent;
	border: none;
	min-width: 22px;
	max-width: 22px;
	min-height: 22px;
	max-height: 22px;
}
QLabel#BrandLabel {
	color: #F4F7FB;
	font-size: 15px;
	font-weight: 700;
}
QLabel#PageTitle {
	color: #F4F7FB;
	font-size: 18px;
	font-weight: 700;
}
QLabel#PageSubtitle, QLabel#SupportingText {
	color: #A4AEBD;
	font-size: 11px;
}
QLabel[class="supporting"] {
	color: #A4AEBD;
}
QToolButton[uiRole="contextHelp"] {
	background: #12365A;
	color: #62BEFF;
	border: 1px solid #376B95;
	border-radius: 10px;
	padding: 0;
	min-width: 20px;
	max-width: 20px;
	min-height: 20px;
	max-height: 20px;
	font-size: 13px;
	font-weight: 800;
}
QToolButton[uiRole="contextHelp"]:hover,
QToolButton[uiRole="contextHelp"]:focus,
QToolButton[uiRole="contextHelp"]:checked {
	background: #248FE8;
	color: #F4F7FB;
	border-color: #6AC5FF;
}
QToolButton[uiRole="contextHelp"]:pressed {
	background: #1674C1;
	color: #FFFFFF;
}
QFrame#ContextHelpPopup {
	background: #111A27;
	border: 1px solid #328ED6;
	border-radius: 8px;
}
QLabel#ContextHelpPopupText {
	background: transparent;
	border: none;
	color: #F4F7FB;
	font-size: 12px;
}
QToolTip {
	background: #171E29;
	color: #F4F7FB;
	border: 1px solid #7C3AED;
	border-radius: 5px;
	padding: 7px 9px;
}
QLabel#SectionLabel {
	color: #F4F7FB;
	font-size: 12px;
	font-weight: 700;
}
QFrame#StatusCard, QFrame#SettingsSection, QFrame#ClipCard,
QFrame[cardRole="summary"], QFrame[cardRole="sessionSummary"],
QFrame[cardRole="filterPanel"],
QFrame[settingsRole="section"] {
	background: #171E29;
	border: 1px solid #283345;
	border-radius: 12px;
}
QFrame[settingsRole="section"][sectionTone="purple"] {
	border-top: 2px solid #7C3AED;
}
QFrame[settingsRole="section"][sectionTone="cyan"] {
	border-top: 2px solid #0891B2;
}
QFrame[settingsRole="section"][sectionTone="blue"] {
	border-top: 2px solid #3B82F6;
}
QFrame[settingsRole="section"] QLabel#SectionLabel {
	color: #F4F7FB;
	font-size: 12px;
	font-weight: 800;
}
QFrame[settingsRole="section"] QLabel[class="supporting"] {
	color: #98A6B8;
	font-size: 10px;
}
QFrame#StatusCard:hover, QFrame#ClipCard:hover, QFrame[cardRole="summary"]:hover {
	background: #202938;
	border-color: #36445A;
}
)QSS") + QStringLiteral(R"QSS(
QFrame#clipsPageHeader {
	background: #101722;
	border: 1px solid #29374A;
	border-radius: 12px;
}
QLabel#clipsHeaderIcon {
	background: #241B45;
	color: #C4A7FF;
	border: 1px solid #4E387D;
	border-radius: 10px;
}
QFrame#StatusCard[cardRole="summary"] {
	background: #121A26;
	border: 1px solid #27364A;
	border-radius: 11px;
}
QFrame#StatusCard[cardRole="summary"][summaryTone="0"] { border-top: 2px solid #8B5CF6; }
QFrame#StatusCard[cardRole="summary"][summaryTone="1"] { border-top: 2px solid #F5B942; }
QFrame#StatusCard[cardRole="summary"][summaryTone="2"] { border-top: 2px solid #22D3EE; }
QFrame#StatusCard[cardRole="summary"][summaryTone="3"] { border-top: 2px solid #F59E0B; }
QFrame#StatusCard[cardRole="summary"][summaryTone="4"] { border-top: 2px solid #A78BFA; }
QFrame#clipsExportHost, QFrame#clipsCompactFilters {
	background: #101722;
	border: 1px solid #263348;
	border-radius: 10px;
}
QFrame#ClipCard {
	background: #121A26;
	border: 1px solid #27364A;
	border-radius: 10px;
}
QFrame#ClipCard:hover {
	background: #172131;
	border-color: #5A4389;
}
QWidget#clipActions {
	background: transparent;
}
QLabel#clipOrientationBadge {
	background: #10323C;
	color: #67E8F9;
	border: 1px solid #155E75;
	border-radius: 5px;
	padding: 2px 6px;
	font-size: 9px;
	font-weight: 700;
}
QPushButton[clipAction="true"] {
	background: #101823;
	border: 1px solid #324057;
	border-radius: 7px;
}
QPushButton[clipAction="true"]:hover {
	background: #202C3D;
	border-color: #6D52A4;
}
QPushButton[clipAction="true"][destructiveAction="true"] {
	background: #2A151C;
	border-color: #6F2633;
}
QPushButton[clipAction="true"][destructiveAction="true"]:hover {
	background: #421C27;
	border-color: #FF6B78;
}
QFrame[captureRole="card"] {
	background: #121A26;
	border: 1px solid #27364A;
	border-radius: 12px;
}
QFrame[captureRole="action"] {
	background: #0F1723;
	border: 1px solid #2C3A50;
	border-radius: 11px;
}
QFrame[captureRole="action"]:hover {
	background: #151E2C;
	border-color: #5B3F8D;
}
QFrame[captureRole="actionFeatured"] {
	background: #35206D;
	border: 1px solid #8B5CF6;
	border-radius: 11px;
}
QFrame[captureRole="flowStep"] {
	background: #0F1723;
	border: 1px solid #26354A;
	border-radius: 10px;
}
QFrame#verticalPreviewCard, QFrame[verticalRole="card"] {
	background: #101722;
	border: 1px solid #2A374A;
	border-radius: 12px;
}
QFrame#verticalPageHeader {
	background: transparent;
	border: none;
	border-radius: 0;
}
QFrame#verticalCompactActionBar {
	background: #101722;
	border: 1px solid #2A374A;
	border-radius: 10px;
}
QPushButton[controlRole="compactIcon"] {
	min-width: 30px;
	max-width: 40px;
	min-height: 30px;
	max-height: 40px;
	padding: 0;
	background: #141D2A;
	border: 1px solid #344258;
	border-radius: 8px;
}
QPushButton[controlRole="compactIcon"]:hover {
	background: #251A3D;
	border-color: #8B5CF6;
}
QPushButton[controlRole="compactIcon"]:checked {
	background: #35206D;
	border-color: #A05CF8;
}
QWidget#verticalHeaderIdentity,
QWidget#verticalHeaderActionBlock,
QWidget#verticalSaveStateRow {
	background: transparent;
}
QWidget#verticalHeaderActionBlock QPushButton {
	font-weight: 600;
}
QPushButton#verticalStartReplayButton {
	background: #8B46F5;
	border-color: #A05CF8;
}
QPushButton#verticalStartReplayButton:hover {
	background: #9A58F7;
}
QLabel#verticalAutoSavedLabel,
QLabel#verticalSavedNowLabel,
QLabel#verticalSavedSeparator {
	color: #7F8A9B;
	font-size: 11px;
}
QLabel#verticalSavedIcon {
	color: #8491A3;
	border: 1px solid #637084;
	border-radius: 7px;
	min-width: 14px;
	max-width: 14px;
	min-height: 14px;
	max-height: 14px;
	font-size: 9px;
	font-weight: 700;
}
QFrame[verticalRole="controlCard"] {
	background: #101722;
	border: 1px solid #2A374A;
	border-radius: 12px;
}
QFrame[verticalRole="controlCard"]:hover {
	border-color: #35445B;
}
QWidget#verticalCanvasSettingsSection {
	background: transparent;
}
QFrame#verticalCompositionCard QLabel[verticalRole="sectionTitle"],
QFrame#verticalCanvasCard QLabel[verticalRole="sectionTitle"],
QFrame#verticalPositionCard QLabel[verticalRole="sectionTitle"] {
	color: #E9D5FF;
}
QWidget#verticalTab QSlider::groove:horizontal {
	height: 4px;
	background: #2A3547;
	border-radius: 2px;
}
QWidget#verticalTab QSlider::sub-page:horizontal {
	background: #8B5CF6;
	border-radius: 2px;
}
QWidget#verticalTab QSlider::handle:horizontal {
	width: 14px;
	margin: -5px 0;
	background: #B46CFF;
	border: 2px solid #D8B4FE;
	border-radius: 7px;
}
QWidget#verticalTab QSlider::handle:horizontal:hover {
	background: #C084FC;
}
QLabel#verticalHeaderIcon {
	font-size: 25px;
	font-weight: 700;
}
QLabel#verticalActiveBadge {
	background: #172421;
	color: #83D7B0;
	border: 1px solid #285849;
	border-radius: 8px;
	padding: 0 12px;
	font-weight: 600;
}
QLabel#verticalActiveBadge[verticalActive="true"] {
	background: #132D27;
	color: #58DCA4;
	border-color: #245C49;
}
QLabel#verticalPreviewTitle {
	color: #F4F7FB;
	font-size: 13px;
	font-weight: 700;
}
QLabel#verticalPreviewRatio {
	background: #202A38;
	color: #C7D0DD;
	border: 1px solid #344258;
	border-radius: 6px;
	padding: 3px 7px;
	font-weight: 700;
}
QWidget[verticalRole="formSection"] {
	background: #111925;
	border-top: 1px solid #2B394C;
}
QLabel[verticalRole="sectionTitle"] {
	color: #F4F7FB;
	font-size: 14px;
	font-weight: 800;
}
QLabel#CaptureSectionTitle {
	color: #C9D3E2;
	font-size: 12px;
	font-weight: 800;
}
QLabel#CaptureDescription {
	color: #9AA8BB;
	font-size: 11px;
}
QLabel#captureReplayStateLabel {
	color: #F4F7FB;
	font-size: 18px;
	font-weight: 800;
}
QLabel#captureReplayStateLabel[replayActive="true"] {
	color: #55E6A5;
}
QLabel#captureBufferOrb {
	background: #241A3B;
	color: #9A7ACF;
	border: 2px solid #60418B;
	border-radius: 54px;
	font-size: 20px;
}
QLabel#CaptureStepNumber {
	background: #21183B;
	color: #C4A7FF;
	border: 1px solid #6941A5;
	border-radius: 16px;
	font-weight: 800;
}
QLabel#CaptureStepTitle {
	color: #F4F7FB;
	font-size: 13px;
	font-weight: 700;
}
QLabel#lastClipThumbnail {
	background: #0A111C;
	color: #78879B;
	border: 1px dashed #36485F;
	border-radius: 9px;
	padding: 10px;
}
QPushButton[captureRole="shortcut"] {
	text-align: left;
	padding-left: 14px;
}
QWidget#captureDashboard {
	background: #0C1016;
}
QFrame[cardRole="sessionSummary"] {
	background: #121821;
	border-color: #36445A;
}
QLabel#StatusTitle {
	color: #A4AEBD;
	font-size: 10px;
	font-weight: 600;
}
QLabel#StatusValue {
	color: #F4F7FB;
	font-size: 13px;
	font-weight: 700;
}
QLabel#StatusPill {
	background: #12351F;
	color: #22C55E;
	border: 1px solid #1C6334;
	border-radius: 6px;
	padding: 2px 6px;
	font-size: 10px;
	font-weight: 700;
}
QLabel#StatusPill[statusTone="pending"] {
	background: #352A12;
	color: #F59E0B;
	border-color: #705315;
}
QLabel#StatusPill[statusTone="error"] {
	background: #3A171B;
	color: #F87171;
	border-color: #7F2931;
}
QPushButton#PrimaryButton, QPushButton[controlRole="primary"] {
	background: #7C3AED;
	color: #FFFFFF;
	border: 1px solid #8B5CF6;
	border-radius: 8px;
	padding: 0 16px;
	min-height: 42px;
	font-weight: 700;
	qproperty-iconSize: 16px 16px;
}
QPushButton#PrimaryButton:hover, QPushButton[controlRole="primary"]:hover {
	background: #8B5CF6;
}
QPushButton#PrimaryButton:pressed, QPushButton[controlRole="primary"]:pressed {
	background: #6D28D9;
}
QPushButton#PrimaryButton:disabled, QPushButton[controlRole="primary"]:disabled {
	background: #30394A;
	color: #6F7B8C;
	border-color: #30394A;
}
QPushButton#SecondaryButton, QPushButton[controlRole="secondary"] {
	background: #171E29;
	color: #F4F7FB;
	border: 1px solid #3D4B61;
	border-radius: 8px;
	padding: 0 14px;
	min-height: 38px;
	font-weight: 600;
	qproperty-iconSize: 16px 16px;
}
QPushButton#SecondaryButton:hover, QPushButton[controlRole="secondary"]:hover {
	background: #202938;
	border-color: #7C3AED;
}
QPushButton#SecondaryButton:pressed, QPushButton[controlRole="secondary"]:pressed {
	background: #121821;
}
QPushButton#SecondaryButton:checked, QPushButton[controlRole="secondary"]:checked {
	background: #33205C;
	color: #C4A7FF;
	border-color: #8B5CF6;
}
QPushButton#SecondaryButton:disabled, QPushButton[controlRole="secondary"]:disabled {
	background: #121821;
	color: #6F7B8C;
	border-color: #283345;
}
QMessageBox {
	background: #111925;
	border: 1px solid #34435A;
}
QMessageBox QLabel {
	color: #E8EDF5;
	font-size: 13px;
}
QMessageBox QLabel#qt_msgbox_label {
	min-width: 340px;
	max-width: 480px;
}
QMessageBox QLabel#qt_msgboxex_icon_label {
	min-width: 32px;
	max-width: 32px;
	margin-left: 0;
	margin-right: 4px;
}
QMessageBox#updateStatusDialog QLabel#qt_msgbox_label {
	min-width: 300px;
	max-width: 360px;
}
QMessageBox#updateStatusDialog QPushButton {
	min-width: 84px;
	max-width: 104px;
	min-height: 32px;
	max-height: 32px;
	padding: 0 12px;
}
QMessageBox#remoteAuthenticationResultDialog QLabel#qt_msgbox_label {
	min-width: 520px;
	max-width: 520px;
	min-height: 76px;
	padding-top: 10px;
	padding-bottom: 8px;
}
QMessageBox#remoteAuthenticationResultDialog QPushButton {
	min-height: 36px;
	max-height: 36px;
	padding: 0 14px;
}
QMessageBox#remoteAuthenticationResultDialog QPushButton[compactDialogAction="true"] {
	min-width: 80px;
	max-width: 80px;
}
QMessageBox QPushButton {
	min-width: 132px;
	min-height: 36px;
	padding: 0 16px;
	border-radius: 7px;
	font-size: 11px;
	font-weight: 700;
}
QMessageBox QPushButton:hover {
	border-color: #8C52E5;
}
QPushButton[controlRole="filter"] {
	background: #121821;
	color: #A4AEBD;
	border: 1px solid #303B4D;
	border-radius: 7px;
	padding: 6px 9px;
	min-height: 22px;
	font-size: 11px;
	font-weight: 600;
}
QPushButton[controlRole="filter"]:hover {
	color: #F4F7FB;
	border-color: #6950A5;
}
QPushButton[controlRole="filter"]:checked {
	background: #33205C;
	color: #D8C7FF;
	border-color: #8B5CF6;
}
QPushButton[controlRole="compact"] {
	background: #121821;
	color: #C8D0DC;
	border: 1px solid #344157;
	border-radius: 6px;
	padding: 5px 8px;
	min-height: 20px;
	font-size: 10px;
	font-weight: 600;
}
QPushButton[controlRole="compact"]:hover {
	background: #29213D;
	color: #FFFFFF;
	border-color: #7C3AED;
}
QPushButton[controlRole="compact"][destructiveAction="true"] {
	background: #241419;
	border-color: #71303A;
	color: #FF8A94;
}
QPushButton[controlRole="compact"][destructiveAction="true"]:hover {
	background: #4A1D25;
	border-color: #EF5261;
	color: #FFFFFF;
}
QPushButton[controlRole="secondary"][destructiveAction="true"] {
	background: #241419;
	border-color: #71303A;
	color: #FF8A94;
}
QPushButton[controlRole="secondary"][destructiveAction="true"]:hover {
	background: #4A1D25;
	border-color: #EF5261;
	color: #FFFFFF;
}
QFrame#clipsExportHost QPushButton[toolbarAction="true"] {
	min-height: 32px;
	max-height: 32px;
	padding: 0 12px;
}
QPushButton[controlRole="compact"]:disabled {
	color: #667184;
	border-color: #252F3D;
}
)QSS") + QStringLiteral(R"QSS(
QPushButton[controlRole="icon"] {
	background: transparent;
	color: #A4AEBD;
	border: none;
	border-radius: 6px;
	min-width: 26px;
	min-height: 26px;
	font-size: 18px;
}
QPushButton[controlRole="icon"]:hover, QPushButton[controlRole="icon"]:checked {
	background: #33205C;
	color: #C4A7FF;
}
QLineEdit, QComboBox, QTextEdit {
	background: #121821;
	color: #F4F7FB;
	border: 1px solid #344157;
	border-radius: 8px;
	padding: 7px 10px;
	min-height: 22px;
	selection-background-color: #7C3AED;
}
QSpinBox, QDoubleSpinBox, QKeySequenceEdit {
	background: #121821;
	color: #F4F7FB;
	border: 1px solid #344157;
	border-radius: 8px;
	padding: 6px 8px;
	min-height: 22px;
	selection-background-color: #7C3AED;
}
QSpinBox:focus, QDoubleSpinBox:focus, QKeySequenceEdit:focus,
QTextEdit:focus {
	border-color: #8B5CF6;
}
QProgressBar {
	background: #121821;
	color: #F4F7FB;
	border: 1px solid #344157;
	border-radius: 6px;
	min-height: 16px;
	text-align: center;
}
QProgressBar::chunk {
	background: #7C3AED;
	border-radius: 5px;
}
QLineEdit:read-only {
	color: #A4AEBD;
	background: #0F151E;
}
QLabel#SettingsFieldLabel {
	color: #C8D0DC;
	font-size: 11px;
}
QLabel[statusRole="integration"] {
	background: #261F13;
	color: #F2B84B;
	border: 1px solid #5A451C;
	border-radius: 6px;
	padding: 3px 7px;
	font-size: 9px;
	font-weight: 700;
}
QLabel[statusRole="plan"] {
	background: #33205C;
	color: #C4A7FF;
	border: 1px solid #6941A5;
	border-radius: 6px;
	padding: 3px 8px;
	font-weight: 700;
}
QLabel#settingsErrorNotification {
	background: #3A171B;
	color: #FCA5A5;
	border: 1px solid #7F2931;
	border-radius: 7px;
	padding: 8px 10px;
}
QLabel#settingsWidthHint {
	background: #10243A;
	color: #BAE6FD;
	border: 1px solid #24577A;
	border-radius: 7px;
	padding: 8px 10px;
}
QLineEdit:focus, QComboBox:focus {
	border-color: #8B5CF6;
}
QComboBox::drop-down {
	border: none;
	width: 22px;
}
QLabel#captureNotification, QLabel#libraryErrorNotification,
QLabel#footerStatusLabel[notificationTone="success"],
QLabel#voiceRuntimeStatus[notificationTone="success"] {
	background: #12351F;
	color: #86EFAC;
	border: 1px solid #1C6334;
	border-radius: 7px;
	padding: 8px 10px;
}
QLabel#captureNotification[notificationTone="error"],
QLabel#libraryErrorNotification[notificationTone="error"],
QLabel#footerStatusLabel[notificationTone="error"],
QLabel#voiceRuntimeStatus[notificationTone="error"] {
	background: #3A171B;
	color: #FCA5A5;
	border-color: #7F2931;
}

QFrame#captionBusyOverlay {
  background: rgba(3, 7, 18, 210);
}
QFrame#remoteAuthOverlay {
  background: rgba(3, 7, 18, 220);
}
QFrame#remoteAuthCard {
  background: #111827;
  border: 1px solid #22D3EE;
  border-radius: 14px;
}
QLabel#remoteAuthTitle {
  color: #F8FAFC;
  font-size: 18px;
  font-weight: 700;
}
QLabel#remoteAuthMessage {
  color: #CBD5E1;
  font-size: 14px;
}
QProgressBar#remoteAuthProgress {
  min-height: 10px;
  max-height: 10px;
  border: 0;
  border-radius: 4px;
  background: #273449;
}
QProgressBar#remoteAuthProgress::chunk {
  background: #22D3EE;
  border-radius: 4px;
}
QFrame#captionBusyCard {
  background: #111827;
  border: 1px solid #7C3AED;
  border-radius: 14px;
}
QLabel#captionBusyTitle {
  color: #F8FAFC;
  font-size: 18px;
  font-weight: 700;
}
QLabel#captionBusyLabel {
  color: #CBD5E1;
  font-size: 14px;
}
QLabel#captionBusyPercent {
  color: #C084FC;
  font-size: 28px;
  font-weight: 800;
}
QLabel#captionBusyEta {
  color: #94A3B8;
  font-size: 13px;
}
QProgressBar#captionBusyProgress {
	min-height: 10px;
	max-height: 10px;
  border: 0;
  border-radius: 4px;
  background: #273449;
}
QProgressBar#captionBusyProgress::chunk {
  background: #A855F7;
  border-radius: 4px;
}
  QLabel#captureNotification {
  	padding: 3px 8px;
  	border-radius: 6px;
  }
QWidget#ToggleRow {
	background: transparent;
}
QCheckBox {
	color: #F4F7FB;
	spacing: 8px;
}
QCheckBox::indicator {
	width: 18px;
	height: 18px;
	border-radius: 4px;
	background: #101620;
	border: 1px solid #526077;
}
QCheckBox::indicator:checked {
	background: #7C3AED;
	border-color: #8B5CF6;
}
QCheckBox::indicator:hover {
	border-color: #8B5CF6;
}
QCheckBox::indicator:disabled {
	background: #202735;
	border-color: #343E50;
}
QWidget#SettingsFormRow {
	background: #111823;
	border: 1px solid #263246;
	border-radius: 7px;
}
QWidget#SettingsPathRow {
	background: #111823;
	border: 1px solid #263246;
	border-radius: 7px;
}
QWidget#SettingsFormRow:hover, QWidget#SettingsPathRow:hover {
	background: #151D2A;
	border-color: #303C50;
}
QFrame[settingsRole="statusTile"] {
	background: #0E1622;
	border: 1px solid #29364B;
	border-radius: 8px;
}
QFrame[settingsRole="statusTile"] QLabel#RemoteClipperStatusTitle {
	color: #91A4BF;
	font-size: 10px;
	font-weight: 700;
}
QFrame[settingsRole="statusTile"] QLabel[class="statusValue"] {
	color: #E7EDF7;
	font-size: 12px;
	font-weight: 600;
}
QLabel#SettingsFieldLabel {
	color: #D9E2F0;
	font-weight: 600;
}
QWidget#settingsTab QLabel {
	font-size: 12px;
}
QWidget#settingsTab QLabel#PageTitle {
	font-size: 20px;
}
QWidget#settingsTab QLabel#PageSubtitle {
	font-size: 13px;
}
QWidget#settingsTab QFrame[settingsRole="section"] QLabel#SectionLabel {
	font-size: 14px;
}
QWidget#settingsTab QLabel#SettingsFieldLabel {
	font-size: 13px;
}
QWidget#settingsTab QFrame[settingsRole="section"] QLabel[class="supporting"] {
	font-size: 12px;
}
QWidget#settingsSupportActions QPushButton {
	min-height: 32px;
}
QPushButton[controlRole="compact"] {
	padding-left: 8px;
	padding-right: 8px;
}
QLabel#ScoreBadge {
	background: #12351F;
	color: #22C55E;
	border: 1px solid #1C6334;
	border-radius: 10px;
	padding: 2px 8px;
	font-weight: 700;
}
QLabel#ProBadge {
	background: #33205C;
	color: #C4A7FF;
	border: 1px solid #6941A5;
	border-radius: 6px;
	padding: 3px 8px;
	font-size: 10px;
	font-weight: 800;
}
QFrame#UpgradeBanner {
	background: #1B1630;
	border: 1px solid #4E327E;
	border-radius: 10px;
}
QFrame#EmptyState {
	background: #121821;
	border: 1px dashed #36445A;
	border-radius: 10px;
}
QFrame#ClipThumbnail {
	background: #262F3E;
	border: 1px solid #36445A;
	border-radius: 6px;
}
QLabel#clipThumbnailImage {
	background: transparent;
	border: none;
	color: #C9D4E5;
	font-size: 18px;
}
QLabel#clipDurationBadge {
	background: rgba(7, 12, 20, 210);
	color: #FFFFFF;
	border: 1px solid rgba(255, 255, 255, 35);
	border-radius: 4px;
	font-size: 10px;
	font-weight: 700;
}
QLabel#ClipThumbnail {
	background: #262F3E;
	color: #E8EDF5;
	border: 1px solid #36445A;
	border-radius: 7px;
	font-size: 11px;
	font-weight: 700;
}
QLabel#TriggerBadge {
	background: #242D3B;
	color: #B7C1D0;
	border: 1px solid #354258;
	border-radius: 5px;
	padding: 2px 6px;
	font-size: 9px;
	font-weight: 700;
}
QTabBar#mainTabBar {
	background: #101622;
	border: none;
	border-bottom: 1px solid #263246;
	min-height: 64px;
}
QTabBar#mainTabBar::tab {
	background: transparent;
	color: #B8C1CF;
	border: none;
	border-bottom: 2px solid transparent;
	padding: 18px 20px 16px 20px;
	margin: 0;
	min-width: 116px;
	min-height: 28px;
	font-size: 12px;
	font-weight: 650;
}
QTabBar#mainTabBar::tab:hover {
	color: #F4F7FB;
	background: rgba(105, 70, 170, 38);
}
QTabBar#mainTabBar::tab:selected {
	color: #D8B8FF;
	background: rgba(105, 70, 170, 58);
	border-bottom-color: #B45CFF;
}
QLabel#mainTabBadge {
	background: rgba(126, 73, 206, 78);
	color: #D9BDFF;
	border: 1px solid rgba(174, 111, 255, 95);
	border-radius: 5px;
	padding: 2px 6px;
	margin-left: 4px;
	font-size: 9px;
	font-weight: 700;
}
QScrollArea {
	border: none;
	background: #0C1016;
}
QScrollArea > QWidget > QWidget {
	background: #0C1016;
}
QScrollBar:vertical {
	background: #0C1016;
	width: 8px;
	margin: 0;
}
QScrollBar::handle:vertical {
	background: #36445A;
	border-radius: 4px;
	min-height: 28px;
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
	height: 0;
}
)QSS");
}

void configureCompactUpdateDialog(QMessageBox *dialog, int textWidth)
{
	if (dialog == nullptr)
		return;

	if (auto *messageLabel =
		    dialog->findChild<QLabel *>(QStringLiteral("qt_msgbox_label"))) {
		messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		messageLabel->setWordWrap(true);
		messageLabel->setMinimumWidth(qMin(textWidth, 300));
		messageLabel->setMaximumWidth(textWidth);
	}
	if (auto *iconLabel = dialog->findChild<QLabel *>(
		    QStringLiteral("qt_msgboxex_icon_label"))) {
		iconLabel->setFixedWidth(32);
		iconLabel->setAlignment(Qt::AlignCenter);
	}
	for (auto *button : dialog->findChildren<QPushButton *>()) {
		const auto textWidth = button->fontMetrics().horizontalAdvance(
			button->text());
		button->setMinimumWidth(qMax(132, textWidth + 36));
		button->setSizePolicy(QSizePolicy::MinimumExpanding,
				      QSizePolicy::Fixed);
	}
}

void configureRemoteAuthenticationDialog(QMessageBox *dialog)
{
	if (dialog == nullptr)
		return;

	if (dialog->layout() != nullptr)
		dialog->layout()->setSizeConstraint(QLayout::SetNoConstraint);
	dialog->setMinimumSize(700, 250);
	dialog->resize(700, 250);

	if (auto *messageLabel =
		    dialog->findChild<QLabel *>(QStringLiteral("qt_msgbox_label"))) {
		messageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
		messageLabel->setWordWrap(true);
		messageLabel->setMinimumSize(520, 76);
		messageLabel->setMaximumWidth(520);
		messageLabel->setContentsMargins(0, 10, 0, 8);
	}
	if (auto *iconLabel = dialog->findChild<QLabel *>(
		    QStringLiteral("qt_msgboxex_icon_label"))) {
		iconLabel->setFixedWidth(36);
		iconLabel->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
		iconLabel->setContentsMargins(0, 10, 0, 0);
	}

	auto *okButton = qobject_cast<QPushButton *>(
		dialog->button(QMessageBox::Ok));
	for (auto *button : dialog->findChildren<QPushButton *>()) {
		int targetWidth = 80;
		if (button == okButton) {
			button->setProperty("compactDialogAction", true);
		} else {
			const int requiredWidth =
				button->fontMetrics().horizontalAdvance(button->text()) + 40;
			targetWidth = qMax(220, requiredWidth);
		}
		button->style()->unpolish(button);
		button->style()->polish(button);
		button->setFixedSize(targetWidth, 36);
		button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	}

	QPointer<QMessageBox> guardedDialog(dialog);
	QTimer::singleShot(0, dialog, [guardedDialog] {
		if (guardedDialog.isNull())
			return;
		if (guardedDialog->layout() != nullptr)
			guardedDialog->layout()->setSizeConstraint(QLayout::SetNoConstraint);
		guardedDialog->setMinimumSize(700, 250);
		guardedDialog->resize(700, 250);
	});
}

} // namespace clipcoach::ui::tokens
