#include "../unit/test-support.hpp"

#include <clipcoach/ui/components/clip-card.hpp>
#include <clipcoach/ui/components/context-help-button.hpp>
#include <clipcoach/ui/components/empty-state.hpp>
#include <clipcoach/ui/components/primary-button.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/components/score-badge.hpp>
#include <clipcoach/ui/components/secondary-button.hpp>
#include <clipcoach/ui/components/settings-section.hpp>
#include <clipcoach/ui/components/status-card.hpp>
#include <clipcoach/ui/components/tab-bar.hpp>
#include <clipcoach/ui/components/toggle-row.hpp>
#include <clipcoach/ui/components/upgrade-banner.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QFrame>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	clipcoach::ui::StatusCard statusCard(QStringLiteral("Replay"), QStringLiteral("Active"));
	clipcoach::ui::StatusCard planStatusCard(QStringLiteral("Plan"), QStringLiteral("Free"));
	auto *planAction = planStatusCard.addActionButton(QStringLiteral("Upgrade plan"));
	clipcoach::ui::PrimaryButton primaryButton(QStringLiteral("Primary"));
	clipcoach::ui::SecondaryButton secondaryButton(QStringLiteral("Secondary"));
	clipcoach::ui::ToggleRow toggleRow(QStringLiteral("Trigger"), QStringLiteral("Description"), true);
	toggleRow.setHelpButtonObjectName(QStringLiteral("triggerInfoButton"));
	clipcoach::ui::ClipCard clipCard(QStringLiteral("Clip"), QStringLiteral("Today"), 92);
	clipcoach::ui::ClipCardViewData verticalData;
	verticalData.title = QStringLiteral("Vertical clip");
	verticalData.favoriteTooltip = QStringLiteral("Favorite");
	verticalData.previewLabel = QStringLiteral("Preview");
	verticalData.editLabel = QStringLiteral("Edit and trim");
	verticalData.exportLabel = QStringLiteral("Already 9:16");
	verticalData.captionLabel = QStringLiteral("Caption");
	verticalData.subtitlesLabel = QStringLiteral("Subtitles");
	verticalData.openFolderLabel = QStringLiteral("Open folder");
	verticalData.deleteLabel = QStringLiteral("Delete clip");
	verticalData.exportVerticalEnabled = false;
	verticalData.processing = true;
	verticalData.processingLabel = QStringLiteral("Processing clip");
	clipcoach::ui::ClipCard verticalClipCard(verticalData);
	verticalClipCard.resize(480, 82);
	verticalClipCard.show();
	application.processEvents();
	auto *pendingDot = verticalClipCard.findChild<QLabel *>(QStringLiteral("clipPendingDot"));
	auto *editorButton = verticalClipCard.findChild<QPushButton *>(QStringLiteral("clipQuickEditorButton"));
	clipcoach::test::expect(pendingDot != nullptr && !pendingDot->isHidden() &&
					pendingDot->toolTip() == QStringLiteral("Processing clip"),
				"a processing clip card must expose a visible accessible pending dot");
	clipcoach::test::expect(editorButton != nullptr && editorButton->toolTip() == QStringLiteral("Edit and trim"),
				"each persisted clip must expose the quick editor action");
	clipcoach::ui::ScoreBadge scoreBadge(88);
	clipcoach::ui::SettingsSection settingsSection(QStringLiteral("General"));
	clipcoach::ui::TabBar tabBar;
	clipcoach::ui::EmptyState emptyState(QStringLiteral("Empty"), QStringLiteral("No items"));
	clipcoach::ui::ProBadge proBadge(QStringLiteral("PRO"));
	clipcoach::ui::UpgradeBanner upgradeBanner(QStringLiteral("Pro"), QStringLiteral("Features"),
						   QStringLiteral("Upgrade"), QStringLiteral("PRO"));

	clipcoach::test::expect(statusCard.objectName() == QStringLiteral("StatusCard"),
				"StatusCard must expose a stable object name");
	clipcoach::test::expect(planAction != nullptr && !planAction->isHidden(),
				"a status card action must be visible when it is available");
	planStatusCard.setActionVisible(false);
	clipcoach::test::expect(planAction->isHidden(), "a status card action must be hideable for active Pro plans");
	clipcoach::test::expect(primaryButton.objectName() == QStringLiteral("PrimaryButton"),
				"PrimaryButton must expose a stable object name");
	clipcoach::test::expect(secondaryButton.objectName() == QStringLiteral("SecondaryButton"),
				"SecondaryButton must expose a stable object name");
	clipcoach::test::expect(toggleRow.findChild<QCheckBox *>() != nullptr,
				"ToggleRow must contain a native QCheckBox");
	auto *contextHelp = toggleRow.findChild<QToolButton *>(QStringLiteral("triggerInfoButton"));
	clipcoach::test::expect(contextHelp != nullptr && contextHelp->toolTip() == QStringLiteral("Description") &&
					contextHelp->cursor().shape() == Qt::PointingHandCursor &&
					contextHelp->text() == QStringLiteral("i") && contextHelp->isCheckable(),
				"ToggleRow help must use a clickable contextual information button");
	contextHelp->click();
	application.processEvents();
	QWidget *helpPopup = nullptr;
	for (auto *candidate : QApplication::topLevelWidgets()) {
		if (candidate->objectName() == QStringLiteral("ContextHelpPopup")) {
			helpPopup = candidate;
			break;
		}
	}
	clipcoach::test::expect(helpPopup != nullptr && helpPopup->objectName() == QStringLiteral("ContextHelpPopup") &&
					contextHelp->isChecked(),
				"the first help click must open the contextual popup");
	contextHelp->click();
	application.processEvents();
	clipcoach::test::expect(!contextHelp->isChecked() && (helpPopup == nullptr || !helpPopup->isVisible()),
				"clicking the same information icon again must close its popup");
	clipcoach::test::expect(clipCard.findChild<clipcoach::ui::ScoreBadge *>() != nullptr,
				"ClipCard must contain a ScoreBadge");
	clipcoach::test::expect(
		!verticalClipCard.findChild<QPushButton *>(QStringLiteral("clipExportVerticalButton"))->isEnabled(),
		"an already vertical ClipCard must not offer another 9:16 conversion");
	int compactActions = 0;
	int actionTop = -1;
	for (auto *button : verticalClipCard.findChildren<QPushButton *>()) {
		if (!button->property("clipAction").toBool())
			continue;
		++compactActions;
		clipcoach::test::expect(button->maximumHeight() <= 34, "clip actions must remain compact");
		clipcoach::test::expect(button->geometry().right() < verticalClipCard.width(),
					"every clip action must remain inside the card");
		if (actionTop < 0)
			actionTop = button->geometry().top();
		else
			clipcoach::test::expect(button->geometry().top() == actionTop,
						"all clip actions must share one row");
	}
	clipcoach::test::expect(compactActions == 7,
				"ClipCard must expose seven aligned compact actions including the editor");
	auto *deleteAction = verticalClipCard.findChild<QPushButton *>(QStringLiteral("clipDeleteButton"));
	clipcoach::test::expect(deleteAction != nullptr && deleteAction->property("destructiveAction").toBool(),
				"ClipCard must expose a clearly destructive delete action");
	auto *selection = verticalClipCard.findChild<QCheckBox *>(QStringLiteral("clipSelectionCheck"));
	auto *thumbnail = verticalClipCard.findChild<QWidget *>(QStringLiteral("ClipThumbnail"));
	auto *details = verticalClipCard.findChild<QWidget *>(QStringLiteral("clipDetails"));
	clipcoach::test::expect(selection != nullptr && thumbnail != nullptr && details != nullptr &&
					selection->geometry().left() < thumbnail->geometry().left() &&
					thumbnail->geometry().left() < details->geometry().left() &&
					selection->geometry().left() < verticalClipCard.width() / 3,
				"selection, thumbnail and metadata must remain grouped and aligned on the left");
	for (auto *button : verticalClipCard.findChildren<QPushButton *>()) {
		if (button->property("clipAction").toBool())
			clipcoach::test::expect(
				!button->icon().isNull() && !button->toolTip().isEmpty(),
				"every compact clip action must have a visible icon and explanatory tooltip");
	}
	clipcoach::test::expect(scoreBadge.score() == 88, "ScoreBadge must preserve its score");
	scoreBadge.setLabel(QStringLiteral("Score"));
	clipcoach::test::expect(scoreBadge.text() == QStringLiteral("Score: 88"),
				"ScoreBadge must render a localized label next to the score");
	clipcoach::test::expect(settingsSection.contentLayout() != nullptr,
				"SettingsSection must expose its content layout");
	clipcoach::test::expect(tabBar.documentMode(), "TabBar must use compact document mode");
	clipcoach::test::expect(emptyState.objectName() == QStringLiteral("EmptyState"),
				"EmptyState must expose a stable object name");
	clipcoach::test::expect(proBadge.objectName() == QStringLiteral("ProBadge"),
				"ProBadge must expose a stable object name");
	auto *upgradeButton = upgradeBanner.findChild<QPushButton *>();
	clipcoach::test::expect(
		upgradeButton != nullptr &&
			upgradeButton->property("externalUrl").toString().endsWith(QStringLiteral("#pricing")),
		"UpgradeBanner must link every Pro action to public pricing");

	QMessageBox compactUpdateDialog(QMessageBox::Information, QStringLiteral("ClipXtudio is up to date"),
					QStringLiteral("Installed version: 0.5.39\nYou are using the latest version."),
					QMessageBox::Ok);
	compactUpdateDialog.setObjectName(QStringLiteral("updateStatusDialog"));
	compactUpdateDialog.setStyleSheet(clipcoach::ui::tokens::styleSheet());
	clipcoach::ui::tokens::configureCompactUpdateDialog(&compactUpdateDialog, 360);
	compactUpdateDialog.show();
	application.processEvents();
	auto *updateIcon = compactUpdateDialog.findChild<QLabel *>(QStringLiteral("qt_msgboxex_icon_label"));
	auto *updateText = compactUpdateDialog.findChild<QLabel *>(QStringLiteral("qt_msgbox_label"));
	clipcoach::test::expect(updateIcon != nullptr && updateIcon->width() <= 32,
				"the update dialog icon must not reserve a wide empty column");
	clipcoach::test::expect(updateText != nullptr && updateText->width() <= 360,
				"the up-to-date message must remain in a compact text column");

	QMessageBox remoteAuthenticationDialog(
		QMessageBox::Warning, QStringLiteral("No se pudo autenticar"),
		QStringLiteral(
			"No fue posible renovar o validar Remote Clipper. Comprueba tu conexión y vuelve a intentarlo."),
		QMessageBox::Ok);
	remoteAuthenticationDialog.setObjectName(QStringLiteral("remoteAuthenticationResultDialog"));
	remoteAuthenticationDialog.addButton(QStringLiteral("Abrir mi cuenta"), QMessageBox::ActionRole);
	remoteAuthenticationDialog.setStyleSheet(clipcoach::ui::tokens::styleSheet());
	clipcoach::ui::tokens::configureRemoteAuthenticationDialog(&remoteAuthenticationDialog);
	remoteAuthenticationDialog.show();
	application.processEvents();
	auto *remoteText = remoteAuthenticationDialog.findChild<QLabel *>(QStringLiteral("qt_msgbox_label"));
	clipcoach::test::expect(remoteText != nullptr && remoteText->wordWrap() && remoteText->width() == 520 &&
					remoteText->height() >= 76,
				"the Remote Clipper authentication message must wrap without being clipped");
	clipcoach::test::expect(remoteAuthenticationDialog.width() >= 700 && remoteAuthenticationDialog.height() >= 250,
				"the Remote Clipper result dialog must preserve enough space under Windows scaling");
	auto *remoteOkButton = remoteAuthenticationDialog.button(QMessageBox::Ok);
	clipcoach::test::expect(remoteOkButton != nullptr && remoteOkButton->width() == 80,
				"the Remote Clipper OK action must stay compact");
	for (auto *button : remoteAuthenticationDialog.findChildren<QPushButton *>()) {
		if (button == remoteOkButton)
			continue;
		clipcoach::test::expect(button->width() >= button->fontMetrics().horizontalAdvance(button->text()) + 40,
					"Remote Clipper dialog buttons must fit their localized text");
	}

	return clipcoach::test::pass("components-construction-test");
}
