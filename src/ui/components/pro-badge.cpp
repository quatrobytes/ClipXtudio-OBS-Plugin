#include <clipcoach/ui/components/pro-badge.hpp>

namespace clipcoach::ui {

ProBadge::ProBadge(const QString &text, QWidget *parent) : QLabel(text, parent)
{
	setObjectName(QStringLiteral("ProBadge"));
	setAlignment(Qt::AlignCenter);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

} // namespace clipcoach::ui
