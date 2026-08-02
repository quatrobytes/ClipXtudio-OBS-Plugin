#include <clipcoach/ui/components/context-help-button.hpp>

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QScreen>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace clipcoach::ui {

ContextHelpButton::ContextHelpButton(const QString &helpText, QWidget *parent)
	: QToolButton(parent)
{
	setObjectName(QStringLiteral("ContextHelpButton"));
	setProperty("uiRole", QStringLiteral("contextHelp"));
	setText(QStringLiteral("i"));
	setAutoRaise(true);
	setCheckable(true);
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::StrongFocus);
	setFixedSize(20, 20);
	setAccessibleName(tr("More information"));
	setHelpText(helpText);

	connect(this, &QToolButton::toggled, this, [this](bool checked) {
		if (checked)
			showHelpPopup();
		else
			hideHelpPopup();
	});
	if (qApp != nullptr)
		qApp->installEventFilter(this);
}

ContextHelpButton::~ContextHelpButton()
{
	if (qApp != nullptr)
		qApp->removeEventFilter(this);
	delete popup_;
}

void ContextHelpButton::setHelpText(const QString &helpText)
{
	helpText_ = helpText.trimmed();
	setToolTip(helpText_);
	setAccessibleDescription(helpText_);
	setVisible(!helpText_.isEmpty());
	if (popup_ != nullptr) {
		if (auto *label = popup_->findChild<QLabel *>(
			    QStringLiteral("ContextHelpPopupText")))
			label->setText(helpText_);
	}
	if (helpText_.isEmpty())
		hideHelpPopup();
}

QString ContextHelpButton::helpText() const
{
	return helpText_;
}

bool ContextHelpButton::event(QEvent *event)
{
	// The help text is kept in toolTip() for accessibility and automated
	// inspection, but the visible popup is click-controlled so it never opens
	// unexpectedly while the user is moving through Settings.
	if (event->type() == QEvent::ToolTip)
		return true;
	if (event->type() == QEvent::Hide)
		hideHelpPopup();
	return QToolButton::event(event);
}

bool ContextHelpButton::eventFilter(QObject *watched, QEvent *event)
{
	Q_UNUSED(watched);
	if (popup_ == nullptr || !popup_->isVisible())
		return false;

	if (event->type() == QEvent::MouseButtonPress) {
		const auto *mouseEvent = static_cast<QMouseEvent *>(event);
		const QPoint point = mouseEvent->globalPosition().toPoint();
		const QRect buttonRect(mapToGlobal(QPoint(0, 0)), size());
		const QRect popupRect(popup_->mapToGlobal(QPoint(0, 0)),
				      popup_->size());
		if (!buttonRect.contains(point) && !popupRect.contains(point))
			hideHelpPopup();
	} else if (event->type() == QEvent::KeyPress) {
		const auto *keyEvent = static_cast<QKeyEvent *>(event);
		if (keyEvent->key() == Qt::Key_Escape)
			hideHelpPopup();
	} else if (event->type() == QEvent::ApplicationDeactivate) {
		hideHelpPopup();
	}
	return false;
}

void ContextHelpButton::showHelpPopup()
{
	if (helpText_.isEmpty()) {
		const QSignalBlocker blocker(this);
		setChecked(false);
		return;
	}

	if (popup_ == nullptr) {
		popup_ = new QFrame(nullptr, Qt::ToolTip | Qt::FramelessWindowHint);
		popup_->setObjectName(QStringLiteral("ContextHelpPopup"));
		popup_->setAttribute(Qt::WA_ShowWithoutActivating);
		auto *layout = new QVBoxLayout(popup_);
		layout->setContentsMargins(12, 10, 12, 10);
		auto *label = new QLabel(helpText_, popup_);
		label->setObjectName(QStringLiteral("ContextHelpPopupText"));
		label->setWordWrap(true);
		label->setMinimumWidth(250);
		label->setMaximumWidth(360);
		layout->addWidget(label);
	}

	popup_->adjustSize();
	QPoint position = mapToGlobal(QPoint(0, height() + 6));
	if (const auto *screen = QGuiApplication::screenAt(
		    mapToGlobal(rect().center()))) {
		const QRect available = screen->availableGeometry();
		if (position.x() + popup_->width() > available.right())
			position.setX(available.right() - popup_->width());
		if (position.x() < available.left())
			position.setX(available.left());
		if (position.y() + popup_->height() > available.bottom())
			position.setY(mapToGlobal(
				QPoint(0, -popup_->height() - 6)).y());
	}
	popup_->move(position);
	popup_->show();
	popup_->raise();
}

void ContextHelpButton::hideHelpPopup()
{
	if (popup_ != nullptr)
		popup_->hide();
	if (isChecked()) {
		const QSignalBlocker blocker(this);
		setChecked(false);
	}
}

} // namespace clipcoach::ui
