#pragma once

#include <QFrame>

class QLabel;
class QHBoxLayout;
class QPushButton;

namespace clipcoach::ui {

enum class StatusTone {
	Neutral,
	Active,
	Pending,
	Error,
};

class StatusCard final : public QFrame {
public:
	explicit StatusCard(const QString &title, const QString &value, bool active = true, QWidget *parent = nullptr);

	void setValue(const QString &value);
	void setActive(bool active);
	void setTone(StatusTone tone);
	QPushButton *addActionButton(const QString &text);
	void setActionVisible(bool visible);

private:
	QLabel *valueLabel_{nullptr};
	QLabel *statusPill_{nullptr};
	QHBoxLayout *valueRow_{nullptr};
	QPushButton *actionButton_{nullptr};
};

} // namespace clipcoach::ui
