#include <clipcoach/ui/components/empty-state.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QLabel>
#include <QVBoxLayout>

namespace clipcoach::ui {

EmptyState::EmptyState(const QString &title, const QString &description, QWidget *parent)
	: QFrame(parent)
{
	setObjectName(QStringLiteral("EmptyState"));
	setFrameShape(QFrame::NoFrame);
	setMinimumHeight(150);

	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl,
				   tokens::kSpaceXl);
	layout->setSpacing(tokens::kSpaceSm);
	layout->setAlignment(Qt::AlignCenter);

	auto *titleLabel = new QLabel(title, this);
	titleLabel->setObjectName(QStringLiteral("PageTitle"));
	titleLabel->setAlignment(Qt::AlignCenter);
	auto *descriptionLabel = new QLabel(description, this);
	descriptionLabel->setObjectName(QStringLiteral("SupportingText"));
	descriptionLabel->setAlignment(Qt::AlignCenter);
	descriptionLabel->setWordWrap(true);

	layout->addWidget(titleLabel);
	layout->addWidget(descriptionLabel);
}

} // namespace clipcoach::ui
