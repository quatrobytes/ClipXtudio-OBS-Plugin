#include <clipcoach/ui/components/upgrade-banner.hpp>
#include <clipcoach/ui/components/primary-button.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QLabel>
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>

namespace clipcoach::ui {

UpgradeBanner::UpgradeBanner(const QString &title, const QString &description, const QString &buttonText,
			     const QString &badgeText, QWidget *parent)
	: QFrame(parent)
{
	setObjectName(QStringLiteral("UpgradeBanner"));
	setFrameShape(QFrame::NoFrame);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceMd, tokens::kSpaceLg, tokens::kSpaceMd);
	layout->setSpacing(tokens::kSpaceMd);

	auto *badge = new ProBadge(badgeText, this);
	auto *copyLayout = new QVBoxLayout();
	copyLayout->setSpacing(tokens::kSpaceXs);
	auto *titleLabel = new QLabel(title, this);
	titleLabel->setObjectName(QStringLiteral("SectionLabel"));
	auto *descriptionLabel = new QLabel(description, this);
	descriptionLabel->setObjectName(QStringLiteral("SupportingText"));
	descriptionLabel->setWordWrap(true);
	copyLayout->addWidget(titleLabel);
	copyLayout->addWidget(descriptionLabel);

	auto *button = new PrimaryButton(buttonText, this);
	button->setObjectName(QStringLiteral("upgradeButton"));
	button->setMinimumWidth(116);
	QUrl purchaseUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL));
	purchaseUrl.setPath(QStringLiteral("/"));
	purchaseUrl.setFragment(QStringLiteral("pricing"));
	button->setProperty("externalUrl", purchaseUrl.toString());
	connect(button, &QPushButton::clicked, this, [purchaseUrl] {
		if (purchaseUrl.isValid() && (purchaseUrl.scheme() == QStringLiteral("https") ||
					      ((purchaseUrl.host() == QStringLiteral("127.0.0.1") ||
						purchaseUrl.host() == QStringLiteral("localhost")) &&
					       purchaseUrl.scheme() == QStringLiteral("http"))))
			QDesktopServices::openUrl(purchaseUrl);
	});

	layout->addWidget(badge, 0, Qt::AlignTop);
	layout->addLayout(copyLayout, 1);
	layout->addWidget(button, 0, Qt::AlignVCenter);
}

} // namespace clipcoach::ui
