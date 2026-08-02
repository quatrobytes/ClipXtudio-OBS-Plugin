#include <clipcoach/ui/components/secondary-button.hpp>

namespace clipcoach::ui {

SecondaryButton::SecondaryButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
	setObjectName(QStringLiteral("SecondaryButton"));
	setProperty("controlRole", QStringLiteral("secondary"));
	setCursor(Qt::PointingHandCursor);
	setMinimumHeight(34);
}

} // namespace clipcoach::ui
