#include "test-support.hpp"

#include <clipcoach/integrations/chat-integration.hpp>
#include <clipcoach/integrations/chat-pulse-service.hpp>

#include <map>

namespace {

class MemorySecureStorage final : public clipcoach::security::SecureStorage {
public:
	bool store(std::string_view key, const clipcoach::security::SecretBytes &secret, std::string *) override
	{
		values[std::string(key)] = secret;
		return true;
	}

	clipcoach::security::SecretReadResult load(std::string_view key) override
	{
		const auto iterator = values.find(std::string(key));
		if (iterator == values.end())
			return {{}, clipcoach::security::SecureStorageError::NotFound, "not found"};
		return {iterator->second, clipcoach::security::SecureStorageError::None, {}};
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
	bool networkFails{false};
	bool refreshCalled{false};
	bool revoked{false};
	bool exchangeExpired{false};

	clipcoach::integrations::IntegrationResult<clipcoach::integrations::AuthorizationRequest>
	beginAuthorization(clipcoach::integrations::ChatPlatform platform) override
	{
		return decltype(beginAuthorization(platform))::success({"https://auth.example/connect", "csrf-state"});
	}

	clipcoach::integrations::IntegrationResult<clipcoach::integrations::OAuthCredential>
	exchangeAuthorizationCode(clipcoach::integrations::ChatPlatform, std::string_view) override
	{
		return decltype(exchangeAuthorizationCode(clipcoach::integrations::ChatPlatform::Twitch, ""))::success(
			{"access", "refresh",
			 std::chrono::system_clock::now() +
				 (exchangeExpired ? -std::chrono::hours(1) : std::chrono::hours(1))});
	}

	clipcoach::integrations::IntegrationResult<clipcoach::integrations::OAuthCredential>
	refreshCredential(clipcoach::integrations::ChatPlatform, std::string_view) override
	{
		refreshCalled = true;
		return decltype(refreshCredential(clipcoach::integrations::ChatPlatform::Twitch, ""))::success(
			{"new-access", "new-refresh", std::chrono::system_clock::now() + std::chrono::hours(1)});
	}

	clipcoach::integrations::IntegrationResult<std::vector<clipcoach::integrations::ChatMessage>>
	readMessages(clipcoach::integrations::ChatPlatform, std::string_view) override
	{
		if (networkFails)
			return decltype(readMessages(clipcoach::integrations::ChatPlatform::Twitch, ""))::failure(
				"network unavailable");
		return decltype(readMessages(clipcoach::integrations::ChatPlatform::Twitch, ""))::success(
			{{{"1", "viewer", "!clip", std::chrono::system_clock::now(), false}}});
	}

	void revoke(clipcoach::integrations::ChatPlatform, std::string_view) noexcept override { revoked = true; }
};

void expect(bool condition, const char *message)
{
	clipcoach::test::expect(condition, message);
}

} // namespace

int main()
{
	using namespace clipcoach::integrations;
	MemorySecureStorage storage;
	FakeTransport transport;
	ChatIntegrationManager manager(storage, transport, true);

	auto authorization = manager.beginConnect(ChatPlatform::Twitch);
	expect(authorization.succeeded() && manager.state(ChatPlatform::Twitch) == ChatConnectionState::Connecting,
	       "Twitch must begin OAuth connection");
	std::string error;
	expect(!manager.completeConnect(ChatPlatform::Twitch, "code", "wrong-state", &error),
	       "OAuth state mismatch must be rejected");
	expect(manager.completeConnect(ChatPlatform::Twitch, "code", "csrf-state", &error) &&
		       manager.state(ChatPlatform::Twitch) == ChatConnectionState::Connected && !storage.values.empty(),
	       "OAuth credential must be stored only in SecureStorage");

	auto poll = manager.poll(ChatPlatform::Twitch);
	expect(poll.state == ChatConnectionState::Connected && poll.messages.size() == 1,
	       "connected Twitch integration must read messages");
	transport.networkFails = true;
	poll = manager.poll(ChatPlatform::Twitch);
	expect(poll.state == ChatConnectionState::NetworkError && !poll.error.empty(),
	       "network errors must be surfaced without disconnecting secrets");

	expect(manager.disconnect(ChatPlatform::Twitch, &error) &&
		       manager.state(ChatPlatform::Twitch) == ChatConnectionState::Disconnected &&
		       storage.values.empty() && transport.revoked,
	       "disconnect must revoke and erase credentials");

	transport.networkFails = false;
	transport.exchangeExpired = true;
	authorization = manager.beginConnect(ChatPlatform::YouTube);
	expect(authorization.succeeded() &&
		       manager.completeConnect(ChatPlatform::YouTube, "code", "csrf-state", &error),
	       "YouTube must complete OAuth connection");
	poll = manager.poll(ChatPlatform::YouTube);
	expect(poll.state == ChatConnectionState::Connected && transport.refreshCalled,
	       "expired OAuth token must refresh before reading chat");
	clipcoach::TriggerEngine triggerEngine(true);
	ChatPulseService pulseService(triggerEngine, true);
	ChatPulseConfiguration pulseConfig;
	pulseConfig.enabled = true;
	expect(pulseService.configure(pulseConfig), "Chat Pulse service must configure for integration sync");
	ChatIntegrationController controller(manager, pulseService);
	const auto sync = controller.sync(ChatPlatform::YouTube);
	expect(sync.state == ChatConnectionState::Connected && sync.messageCount == 1 && sync.pulseCount == 1 &&
		       triggerEngine.recentEvents().size() == 1,
	       "provider sync must forward chat messages into TriggerEngine");

	const auto kick = manager.beginConnect(ChatPlatform::Kick);
	expect(!kick.succeeded() && manager.state(ChatPlatform::Kick) == ChatConnectionState::ComingSoon,
	       "Kick must remain a Coming Soon placeholder");

	ChatIntegrationManager freeManager(storage, transport, false);
	expect(!freeManager.beginConnect(ChatPlatform::YouTube).succeeded() &&
		       freeManager.state(ChatPlatform::YouTube) == ChatConnectionState::ProRequired,
	       "chat integrations must require Pro");

	return clipcoach::test::pass("chat-integration-test");
}
