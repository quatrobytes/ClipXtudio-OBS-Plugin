#include "../unit/test-support.hpp"

#include <clipcoach/integrations/chat-integration.hpp>
#include <clipcoach/ui/integrations-panel.hpp>

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QUrl>

#include <map>

namespace {

class MemoryStorage final : public clipcoach::security::SecureStorage {
public:
	bool store(std::string_view key, const clipcoach::security::SecretBytes &secret, std::string *) override
	{
		values[std::string(key)] = secret;
		return true;
	}
	clipcoach::security::SecretReadResult load(std::string_view key) override
	{
		const auto found = values.find(std::string(key));
		if (found == values.end())
			return {{}, clipcoach::security::SecureStorageError::NotFound, "missing"};
		return {found->second, clipcoach::security::SecureStorageError::None, {}};
	}
	bool remove(std::string_view key, std::string *) override
	{
		values.erase(std::string(key));
		return true;
	}
	std::map<std::string, clipcoach::security::SecretBytes> values;
};

class FakeTransport final : public clipcoach::integrations::ChatPlatformTransport {
public:
	clipcoach::integrations::IntegrationResult<clipcoach::integrations::AuthorizationRequest>
	beginAuthorization(clipcoach::integrations::ChatPlatform) override
	{
		return decltype(beginAuthorization(clipcoach::integrations::ChatPlatform::Twitch))::success(
			{"https://accounts.example/authorize", "csrf-state"});
	}
	clipcoach::integrations::IntegrationResult<clipcoach::integrations::OAuthCredential>
	exchangeAuthorizationCode(clipcoach::integrations::ChatPlatform, std::string_view) override
	{
		return decltype(exchangeAuthorizationCode(clipcoach::integrations::ChatPlatform::Twitch, ""))::success(
			{"access", "refresh", std::chrono::system_clock::now() + std::chrono::hours(1)});
	}
	clipcoach::integrations::IntegrationResult<clipcoach::integrations::OAuthCredential>
	refreshCredential(clipcoach::integrations::ChatPlatform, std::string_view) override
	{
		return decltype(exchangeAuthorizationCode(clipcoach::integrations::ChatPlatform::Twitch, ""))::failure(
			"unused");
	}
	clipcoach::integrations::IntegrationResult<std::vector<clipcoach::integrations::ChatMessage>>
	readMessages(clipcoach::integrations::ChatPlatform, std::string_view) override
	{
		return decltype(readMessages(clipcoach::integrations::ChatPlatform::Twitch, ""))::success({});
	}
	void revoke(clipcoach::integrations::ChatPlatform, std::string_view) noexcept override {}
};

} // namespace

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);
	MemoryStorage storage;
	FakeTransport transport;
	clipcoach::integrations::ChatIntegrationManager manager(storage, transport, true);
	bool opened = false;
	clipcoach::ui::IntegrationsPanel panel([](const char *key) { return QString::fromUtf8(key); }, &manager,
					       [&opened](const QUrl &url) {
						       opened = url.scheme() == QStringLiteral("https");
						       return true;
					       });

	auto *twitchStatus = panel.findChild<QLabel *>(QStringLiteral("twitchConnectionStatus"));
	auto *youtubeStatus = panel.findChild<QLabel *>(QStringLiteral("youtubeConnectionStatus"));
	auto *kickStatus = panel.findChild<QLabel *>(QStringLiteral("kickConnectionStatus"));
	auto *connect = panel.findChild<QPushButton *>(QStringLiteral("twitchConnectButton"));
	auto *disconnect = panel.findChild<QPushButton *>(QStringLiteral("twitchDisconnectButton"));
	clipcoach::test::expect(twitchStatus && youtubeStatus && kickStatus && connect && disconnect,
				"Integrations panel must show Twitch, YouTube and Kick");
	clipcoach::test::expect(kickStatus->text() == QStringLiteral("Settings.Integrations.ComingSoon"),
				"Kick must be a Coming Soon placeholder");

	connect->click();
	clipcoach::test::expect(opened && manager.state(clipcoach::integrations::ChatPlatform::Twitch) ==
						  clipcoach::integrations::ChatConnectionState::Connecting,
				"Connect must open HTTPS OAuth and show Connecting");
	std::string error;
	clipcoach::test::expect(manager.completeConnect(clipcoach::integrations::ChatPlatform::Twitch, "code",
							"csrf-state", &error),
				"OAuth callback must complete");
	panel.refresh();
	clipcoach::test::expect(twitchStatus->text() == QStringLiteral("Integrations.State.Connected"),
				"completed account must show Connected");
	disconnect->click();
	clipcoach::test::expect(storage.values.empty() &&
					manager.state(clipcoach::integrations::ChatPlatform::Twitch) ==
						clipcoach::integrations::ChatConnectionState::Disconnected,
				"Disconnect UI must erase secure credential");
	clipcoach::ui::IntegrationsPanel unavailable([](const char *key) { return QString::fromUtf8(key); }, nullptr);
	clipcoach::test::expect(
		unavailable.findChild<QLabel *>(QStringLiteral("chatIntegrationUnavailableNotice")) != nullptr &&
			!unavailable.findChild<QPushButton *>(QStringLiteral("twitchConnectButton"))->isEnabled(),
		"a build without production OAuth transport must explain the blocker and disable connect");

	return clipcoach::test::pass("integrations-panel-test");
}
