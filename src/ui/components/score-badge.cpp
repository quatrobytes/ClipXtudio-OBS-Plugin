#include <clipcoach/ui/components/score-badge.hpp>

#include <algorithm>

namespace clipcoach::ui {

ScoreBadge::ScoreBadge(int score, QWidget *parent) : QLabel(parent)
{
	setObjectName(QStringLiteral("ScoreBadge"));
	setAlignment(Qt::AlignCenter);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	setScore(score);
}

int ScoreBadge::score() const noexcept
{
	return score_;
}

void ScoreBadge::setScore(int score)
{
	score_ = std::clamp(score, 0, 100);
	updateText();
}

void ScoreBadge::setLabel(const QString &label)
{
	label_ = label.trimmed();
	updateText();
}

void ScoreBadge::updateText()
{
	setText(label_.isEmpty() ? QString::number(score_) : QStringLiteral("%1: %2").arg(label_).arg(score_));
	setAccessibleName(text());
}

} // namespace clipcoach::ui
