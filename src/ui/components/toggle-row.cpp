#include <clipcoach/ui/components/toggle-row.hpp>
#include <clipcoach/ui/components/context-help-button.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>

namespace clipcoach::ui {

ToggleRow::ToggleRow(const QString &title, const QString &description, bool checked, QWidget *parent)
	: QWidget(parent)
{
	setObjectName(QStringLiteral("ToggleRow"));
	setAttribute(Qt::WA_StyledBackground, true);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, tokens::kSpaceSm, 0, tokens::kSpaceSm);
	layout->setSpacing(tokens::kSpaceMd);

	auto *titleLabel = new QLabel(title, this);
	titleLabel->setObjectName(QStringLiteral("SectionLabel"));
	helpButton_ = new ContextHelpButton(description, this);

	toggle_ = new QCheckBox(this);
	toggle_->setObjectName(QStringLiteral("toggleCheckBox"));
	toggle_->setChecked(checked);
	toggle_->setAccessibleName(title);
	toggle_->setCursor(Qt::PointingHandCursor);

	layout->addWidget(titleLabel);
	layout->addWidget(helpButton_, 0, Qt::AlignVCenter);
	layout->addStretch(1);
	layout->addWidget(toggle_, 0, Qt::AlignVCenter);
}

bool ToggleRow::isChecked() const
{
	return toggle_->isChecked();
}

void ToggleRow::setChecked(bool checked)
{
	toggle_->setChecked(checked);
}

void ToggleRow::setHelpButtonObjectName(const QString &objectName)
{
	helpButton_->setObjectName(objectName);
}

} // namespace clipcoach::ui
