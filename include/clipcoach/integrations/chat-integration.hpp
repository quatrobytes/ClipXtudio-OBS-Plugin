#pragma once

#include <clipcoach/security/secure-storage.hpp>

#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clipcoach::integrations {

enum class ChatPlatform {
	Twitch,
	YouTube,
	Kick,
};

enum class ChatConnectionState {
	Disconnected,
	Connecting,
	Connected,
	TokenExpired,
	NetworkError,
	ComingSoon,
	ProRequired,
};

struct ChatMessage {
	std::string id;
	std::string authorId;
	std::string text;
	std::chrono::system_clock::time_point sentAt{
		std::chrono::system_clock::now()};
	bool authorIsModerator{false};
};

struct OAuthCredential {
	std::string accessToken;
	std::string refreshToken;
	std::chrono::system_clock::time_point expiresAt;
};

struct AuthorizationRequest {
	std::string authorizationUrl;
	std::string state;
};

template<typename T> struct IntegrationResult {
	std::optional<T> value;
	std::string error;

	[[nodiscard]] bool succeeded() const noexcept
	{
		return value.has_value() && error.empty();
	}

	[[nodiscard]] static IntegrationResult success(T result)
	{
		return {std::move(result), {}};
	}

	[[nodiscard]] static IntegrationResult failure(std::string message)
	{
		return {{}, std::move(message)};
	}
};

class ChatPlatformTransport {
public:
	virtual ~ChatPlatformTransport() = default;
	[[nodiscard]] virtual IntegrationResult<AuthorizationRequest>
	beginAuthorization(ChatPlatform platform) = 0;
	[[nodiscard]] virtual IntegrationResult<OAuthCredential>
	exchangeAuthorizationCode(ChatPlatform platform,
				  std::string_view authorizationCode) = 0;
	[[nodiscard]] virtual IntegrationResult<OAuthCredential>
	refreshCredential(ChatPlatform platform,
			  std::string_view refreshToken) = 0;
	[[nodiscard]] virtual IntegrationResult<std::vector<ChatMessage>>
	readMessages(ChatPlatform platform,
		     std::string_view accessToken) = 0;
	virtual void revoke(ChatPlatform platform,
			    std::string_view accessToken) noexcept = 0;
};

struct ChatPollResult {
	std::vector<ChatMessage> messages;
	ChatConnectionState state{ChatConnectionState::Disconnected};
	std::string error;
};

class ChatIntegrationManager final {
public:
	ChatIntegrationManager(security::SecureStorage &secureStorage,
			       ChatPlatformTransport &transport,
			       bool proUnlocked);

	[[nodiscard]] IntegrationResult<AuthorizationRequest>
	beginConnect(ChatPlatform platform);
	[[nodiscard]] bool
	completeConnect(ChatPlatform platform, std::string_view code,
			std::string_view returnedState,
			std::string *error = nullptr);
	[[nodiscard]] ChatPollResult poll(ChatPlatform platform);
	[[nodiscard]] bool
	disconnect(ChatPlatform platform, std::string *error = nullptr);
	[[nodiscard]] ChatConnectionState
	state(ChatPlatform platform) const noexcept;
	void setProUnlocked(bool unlocked) noexcept;

private:
	[[nodiscard]] std::string storageKey(ChatPlatform platform) const;
	[[nodiscard]] bool persistCredential(
		ChatPlatform platform, const OAuthCredential &credential,
		std::string *error);
	[[nodiscard]] std::optional<OAuthCredential>
	loadCredential(ChatPlatform platform, std::string *error);
	void setState(ChatPlatform platform, ChatConnectionState state);

	security::SecureStorage &secureStorage_;
	ChatPlatformTransport &transport_;
	std::map<ChatPlatform, ChatConnectionState> states_;
	std::map<ChatPlatform, std::string> pendingStates_;
	bool proUnlocked_{false};
};

[[nodiscard]] const char *chatPlatformName(ChatPlatform platform) noexcept;

} // namespace clipcoach::integrations
