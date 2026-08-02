#include <clipcoach/ui/components/status-card.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace clipcoach::ui {

StatusCard::StatusCard(const QString &title, const QString &value, bool active, QWidget *parent)
	: QFrame(parent)
{
	setObjectName(QStringLiteral("StatusCard"));
	setFrameShape(QFrame::NoFrame);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setMinimumHeight(tokens::kSummaryCardMinHeight);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceSm, tokens::kSpaceMd,
				   tokens::kSpaceSm);
	layout->setSpacing(tokens::kSpaceXs);

	auto *titleLabel = new QLabel(title, this);
	titleLabel->setObjectName(QStringLiteral("StatusTitle"));
	titleLabel->setWordWrap(false);

	valueLabel_ = new QLabel(value, this);
	valueLabel_->setObjectName(QStringLiteral("StatusValue"));

	statusPill_ = new QLabel(value, this);
	statusPill_->setObjectName(QStringLiteral("StatusPill"));
	statusPill_->setAlignment(Qt::AlignCenter);
	statusPill_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	statusPill_->setVisible(active);

	valueRow_ = new QHBoxLayout();
	valueRow_->setContentsMargins(0, 0, 0, 0);
	valueRow_->setSpacing(tokens::kSpaceSm);
	valueRow_->addWidget(statusPill_, 0, Qt::AlignLeft);
	valueRow_->addWidget(valueLabel_, 0, Qt::AlignLeft);
	valueRow_->addStretch(1);

	layout->addWidget(titleLabel);
	layout->addLayout(valueRow_);
	valueLabel_->setVisible(!active);
}

void StatusCard::setValue(const QString &value)
{
	valueLabel_->setText(value);
	statusPill_->setText(value);
}

void StatusCard::setActive(bool active)
{
	statusPill_->setVisible(active);
	valueLabel_->setVisible(!active);
	setTone(active ? StatusTone::Active : StatusTone::Neutral);
}

void StatusCard::setTone(StatusTone tone)
{
	const char *value = "neutral";
	switch (tone) {
	case StatusTone::Active:
		value = "active";
		break;
	case StatusTone::Pending:
		value = "pending";
		break;
	case StatusTone::Error:
		value = "error";
		break;
	case StatusTone::Neutral:
		break;
	}
	const bool pillVisible = tone != StatusTone::Neutral;
	statusPill_->setProperty("statusTone", QString::fromLatin1(value));
	statusPill_->setVisible(pillVisible);
	valueLabel_->setVisible(!pillVisible);
	statusPill_->style()->unpolish(statusPill_);
	statusPill_->style()->polish(statusPill_);
}

QPushButton *StatusCard::addActionButton(const QString &text)
{
	if (actionButton_ == nullptr) {
		actionButton_ = new QPushButton(this);
		actionButton_->setObjectName(QStringLiteral("StatusCardAction"));
		actionButton_->setProperty("controlRole", QStringLiteral("secondary"));
		actionButton_->setCursor(Qt::PointingHandCursor);
		actionButton_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		actionButton_->setMinimumHeight(28);
		valueRow_->addWidget(actionButton_, 0, Qt::AlignRight | Qt::AlignVCenter);
	}
	actionButton_->setText(text);
	return actionButton_;
}

void StatusCard::setActionVisible(bool visible)
{
	if (actionButton_ != nullptr)
		actionButton_->setVisible(visible);
}

} // namespace clipcoach::ui
