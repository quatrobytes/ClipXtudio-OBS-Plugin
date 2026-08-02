#include <clipcoach/ui/components/primary-button.hpp>

namespace clipcoach::ui {

PrimaryButton::PrimaryButton(const QString &text, QWidget *parent) : QPushButton(text, parent)
{
	setObjectName(QStringLiteral("PrimaryButton"));
	setProperty("controlRole", QStringLiteral("primary"));
	setCursor(Qt::PointingHandCursor);
	setMinimumHeight(42);
}

} // namespace clipcoach::ui
