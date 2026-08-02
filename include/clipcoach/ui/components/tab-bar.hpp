#pragma once

#include <QTabBar>

namespace clipcoach::ui {

class TabBar final : public QTabBar {
public:
	explicit TabBar(QWidget *parent = nullptr);
	int addNavigationTab(const QIcon &icon, const QString &text);
	void setTabBadge(int index, const QString &text);
};

} // namespace clipcoach::ui
