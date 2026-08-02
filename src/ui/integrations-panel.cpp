#include <clipcoach/ui/integrations-panel.hpp>

#include <clipcoach/integrations/chat-integration.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace clipcoach::ui {
namespace {

const char *stateKey(integrations::ChatConnectionState state)
{
	using integrations::ChatConnectionState;
	switch (state) {
	case ChatConnectionState::Disconnected:
		return strings::kSettingsDisconnected;
	case ChatConnectionState::Connecting:
		return strings::kIntegrationConnecting;
	case ChatConnectionState::Connected:
		return strings::kIntegrationConnected;
	case ChatConnectionState::TokenExpired:
		return strings::kIntegrationTokenExpired;
	case ChatConnectionState::NetworkError:
		return strings::kIntegrationNetworkError;
	case ChatConnectionState::ComingSoon:
		return strings::kSettingsComingSoon;
	case ChatConnectionState::ProRequired:
		return strings::kIntegrationProRequired;
	}
	return strings::kSettingsDisconnected;
}

} // namespace

IntegrationsPanel::IntegrationsPanel(
	TranslationFunction translator,
	integrations::ChatIntegrationManager *manager,
	AuthorizationOpener opener, QWidget *parent)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  manager_(manager),
	  opener_(std::move(opener))
{
	setObjectName(QStringLiteral("chatIntegrationsPanel"));
	if (!opener_) {
		opener_ = [](const QUrl &url) {
			return QDesktopServices::openUrl(url);
		};
	}
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(tokens::kSpaceSm);
	if (manager_ == nullptr) {
		auto *notice =
			new QLabel(text(strings::kIntegrationUnavailable), this);
		notice->setObjectName(
			QStringLiteral("chatIntegrationUnavailableNotice"));
		notice->setProperty("notificationTone",
				    QStringLiteral("warning"));
		notice->setWordWrap(true);
		layout->addWidget(notice);
	}
	addPlatform(integrations::ChatPlatform::Twitch,
		    QStringLiteral("Twitch"),
		    QStringLiteral("twitchConnectionStatus"));
	addPlatform(integrations::ChatPlatform::YouTube,
		    QStringLiteral("YouTube Live"),
		    QStringLiteral("youtubeConnectionStatus"));
	addPlatform(integrations::ChatPlatform::Kick,
		    QStringLiteral("Kick"),
		    QStringLiteral("kickConnectionStatus"));
	refresh();
}

QString IntegrationsPanel::text(const char *key) const
{
	return translator_ ? translator_(key) : QString::fromUtf8(key);
}

void IntegrationsPanel::addPlatform(
	integrations::ChatPlatform platform, const QString &name,
	const QString &statusObjectName)
{
	auto *row = new QWidget(this);
	auto *layout = new QHBoxLayout(row);
	layout->setContentsMargins(0, tokens::kSpaceXs, 0, tokens::kSpaceXs);
	layout->setSpacing(tokens::kSpaceSm);
	auto *nameLabel = new QLabel(name, row);
	nameLabel->setObjectName(QStringLiteral("SectionLabel"));
	auto *status = new QLabel(row);
	status->setObjectName(statusObjectName);
	status->setProperty("statusRole", QStringLiteral("integration"));
	auto *connectButton =
		new QPushButton(text(strings::kIntegrationConnect), row);
	connectButton->setObjectName(
		QStringLiteral("%1ConnectButton")
			.arg(QString::fromUtf8(
				integrations::chatPlatformName(platform))));
	auto *disconnectButton =
		new QPushButton(text(strings::kIntegrationDisconnect), row);
	disconnectButton->setObjectName(
		QStringLiteral("%1DisconnectButton")
			.arg(QString::fromUtf8(
				integrations::chatPlatformName(platform))));
	layout->addWidget(nameLabel);
	layout->addStretch(1);
	layout->addWidget(status);
	layout->addWidget(connectButton);
	layout->addWidget(disconnectButton);
	rows_[platform] = {status, connectButton, disconnectButton};
	connect(connectButton, &QPushButton::clicked, this,
		[this, platform] { beginConnect(platform); });
	connect(disconnectButton, &QPushButton::clicked, this,
		[this, platform] { disconnect(platform); });
	qobject_cast<QVBoxLayout *>(this->layout())->addWidget(row);
}

void IntegrationsPanel::refresh()
{
	using integrations::ChatConnectionState;
	for (auto &[platform, row] : rows_) {
		auto state = platform == integrations::ChatPlatform::Kick
				     ? ChatConnectionState::ComingSoon
				     : ChatConnectionState::ProRequired;
		if (manager_ != nullptr)
			state = manager_->state(platform);
		row.status->setText(text(stateKey(state)));
		const auto canConnect =
			state == ChatConnectionState::Disconnected ||
			state == ChatConnectionState::NetworkError ||
			state == ChatConnectionState::TokenExpired;
		row.connect->setEnabled(manager_ != nullptr && canConnect);
		row.disconnect->setEnabled(
			manager_ != nullptr &&
			(state == ChatConnectionState::Connected ||
			 state == ChatConnectionState::NetworkError ||
			 state == ChatConnectionState::TokenExpired));
		row.connect->setVisible(platform !=
				       integrations::ChatPlatform::Kick);
		row.disconnect->setVisible(platform !=
					  integrations::ChatPlatform::Kick);
	}
}

void IntegrationsPanel::beginConnect(
	integrations::ChatPlatform platform)
{
	if (manager_ == nullptr)
		return;
	const auto request = manager_->beginConnect(platform);
	if (request.succeeded()) {
		const QUrl url(
			QString::fromStdString(
				request.value->authorizationUrl),
			QUrl::StrictMode);
		if (url.isValid() && url.scheme() == QStringLiteral("https"))
			(void)opener_(url);
	}
	refresh();
}

void IntegrationsPanel::disconnect(
	integrations::ChatPlatform platform)
{
	if (manager_ == nullptr)
		return;
	std::string ignored;
	(void)manager_->disconnect(platform, &ignored);
	refresh();
}

} // namespace clipcoach::ui
