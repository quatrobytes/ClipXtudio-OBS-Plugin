#pragma once

#include <QFrame>

namespace clipcoach::ui {

class EmptyState final : public QFrame {
public:
	explicit EmptyState(const QString &title, const QString &description,
			    QWidget *parent = nullptr);
};

} // namespace clipcoach::ui
