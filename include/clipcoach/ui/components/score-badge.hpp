#pragma once

#include <QLabel>

namespace clipcoach::ui {

class ScoreBadge final : public QLabel {
	Q_OBJECT

public:
	explicit ScoreBadge(int score, QWidget *parent = nullptr);

	[[nodiscard]] int score() const noexcept;
	void setScore(int score);
	void setLabel(const QString &label);

private:
	void updateText();

	int score_{0};
	QString label_;
};

} // namespace clipcoach::ui
