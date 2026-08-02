#pragma once

#include <QLabel>

namespace clipcoach::ui {

class ProBadge final : public QLabel {
	Q_OBJECT

public:
	explicit ProBadge(const QString &text, QWidget *parent = nullptr);
};

} // namespace clipcoach::ui
