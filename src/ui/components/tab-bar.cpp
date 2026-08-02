#include <clipcoach/ui/components/tab-bar.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QLabel>
#include <QSizePolicy>

namespace clipcoach::ui {

TabBar::TabBar(QWidget *parent) : QTabBar(parent)
{
	setObjectName(QStringLiteral("mainTabBar"));
	setDocumentMode(true);
	setDrawBase(false);
	setExpanding(false);
	setElideMode(Qt::ElideRight);
	setIconSize(QSize(20, 20));
	setUsesScrollButtons(true);
	setFocusPolicy(Qt::StrongFocus);
	setMinimumHeight(64);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

int TabBar::addNavigationTab(const QIcon &icon, const QString &text)
{
	const auto index = addTab(icon, text);
	setTabToolTip(index, text);
	return index;
}

void TabBar::setTabBadge(int index, const QString &text)
{
	if (index < 0 || index >= count())
		return;

	auto *badge = new QLabel(text, this);
	badge->setObjectName(QStringLiteral("mainTabBadge"));
	badge->setAttribute(Qt::WA_TransparentForMouseEvents);
	badge->setAlignment(Qt::AlignCenter);
	setTabButton(index, QTabBar::RightSide, badge);
}

} // namespace clipcoach::ui
