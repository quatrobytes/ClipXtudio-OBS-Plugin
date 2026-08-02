#include <clipcoach/ui/components/settings-section.hpp>
#include <clipcoach/ui/components/context-help-button.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace clipcoach::ui {

SettingsSection::SettingsSection(const QString &title, QWidget *parent) : QFrame(parent)
{
	setObjectName(QStringLiteral("SettingsSection"));
	setFrameShape(QFrame::NoFrame);

	contentLayout_ = new QVBoxLayout(this);
	contentLayout_->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceMd, tokens::kSpaceXl,
					  tokens::kSpaceMd);
	contentLayout_->setSpacing(tokens::kSpaceSm);

	auto *heading = new QHBoxLayout();
	heading->setContentsMargins(0, 0, 0, 0);
	heading->setSpacing(tokens::kSpaceSm);
	auto *titleLabel = new QLabel(title, this);
	titleLabel->setObjectName(QStringLiteral("SectionLabel"));
	heading->addWidget(titleLabel);
	helpButton_ = new ContextHelpButton({}, this);
	heading->addWidget(helpButton_, 0, Qt::AlignVCenter);
	heading->addStretch(1);
	contentLayout_->addLayout(heading);
}

QVBoxLayout *SettingsSection::contentLayout() const noexcept
{
	return contentLayout_;
}

void SettingsSection::setHelpText(const QString &helpText)
{
	helpButton_->setObjectName(objectName().isEmpty()
					  ? QStringLiteral("sectionInfoButton")
					  : objectName() + QStringLiteral("InfoButton"));
	helpButton_->setHelpText(helpText);
}

} // namespace clipcoach::ui
