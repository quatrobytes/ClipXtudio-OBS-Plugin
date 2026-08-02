#include <clipcoach/integrations/chat-integration.hpp>

#include <algorithm>
#include <cstring>
#include <limits>

namespace clipcoach::integrations {
namespace {

using security::SecretBytes;

void appendUint32(SecretBytes &output, std::uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
		output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void appendInt64(SecretBytes &output, std::int64_t value)
{
	const auto numeric = static_cast<std::uint64_t>(value);
	for (int shift = 0; shift < 64; shift += 8)
		output.push_back(static_cast<std::uint8_t>(numeric >> shift));
}

bool readUint32(const SecretBytes &input, std::size_t &offset,
		std::uint32_t &value)
{
	if (offset + 4 > input.size())
		return false;
	value = 0;
	for (int shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(input[offset++]) << shift;
	return true;
}

bool readInt64(const SecretBytes &input, std::size_t &offset,
	       std::int64_t &value)
{
	if (offset + 8 > input.size())
		return false;
	std::uint64_t numeric = 0;
	for (int shift = 0; shift < 64; shift += 8)
		numeric |= static_cast<std::uint64_t>(input[offset++]) << shift;
	value = static_cast<std::int64_t>(numeric);
	return true;
}

bool appendString(SecretBytes &output, const std::string &value)
{
	if (value.size() > std::numeric_limits<std::uint32_t>::max())
		return false;
	appendUint32(output, static_cast<std::uint32_t>(value.size()));
	output.insert(output.end(), value.begin(), value.end());
	return true;
}

bool readString(const SecretBytes &input, std::size_t &offset,
		std::string &value)
{
	std::uint32_t length = 0;
	if (!readUint32(input, offset, length) ||
	    offset + length > input.size())
		return false;
	value.assign(reinterpret_cast<const char *>(input.data() + offset),
		     length);
	offset += length;
	return true;
}

SecretBytes serializeCredential(const OAuthCredential &credential)
{
	SecretBytes result{'C', 'C', 'O', '1'};
	if (!appendString(result, credential.accessToken) ||
	    !appendString(result, credential.refreshToken))
		return {};
	appendInt64(
		result,
		std::chrono::duration_cast<std::chrono::seconds>(
			credential.expiresAt.time_since_epoch())
			.count());
	return result;
}

std::optional<OAuthCredential>
deserializeCredential(const SecretBytes &secret)
{
	if (secret.size() < 4 || secret[0] != 'C' || secret[1] != 'C' ||
	    secret[2] != 'O' || secret[3] != '1')
		return std::nullopt;
	std::size_t offset = 4;
	OAuthCredential credential;
	std::int64_t expiresAt = 0;
	if (!readString(secret, offset, credential.accessToken) ||
	    !readString(secret, offset, credential.refreshToken) ||
	    !readInt64(secret, offset, expiresAt) ||
	    offset != secret.size() || credential.accessToken.empty())
		return std::nullopt;
	credential.expiresAt = std::chrono::system_clock::time_point(
		std::chrono::seconds(expiresAt));
	return credential;
}

} // namespace

const char *chatPlatformName(ChatPlatform platform) noexcept
{
	switch (platform) {
	case ChatPlatform::Twitch:
		return "twitch";
	case ChatPlatform::YouTube:
		return "youtube";
	case ChatPlatform::Kick:
		return "kick";
	}
	return "unknown";
}

ChatIntegrationManager::ChatIntegrationManager(
	security::SecureStorage &secureStorage,
	ChatPlatformTransport &transport, bool proUnlocked)
	: secureStorage_(secureStorage),
	  transport_(transport),
	  states_{{ChatPlatform::Twitch,
		   proUnlocked ? ChatConnectionState::Disconnected
			       : ChatConnectionState::ProRequired},
		  {ChatPlatform::YouTube,
		   proUnlocked ? ChatConnectionState::Disconnected
			       : ChatConnectionState::ProRequired},
		  {ChatPlatform::Kick,
		   ChatConnectionState::ComingSoon}},
	  proUnlocked_(proUnlocked)
{
}

IntegrationResult<AuthorizationRequest>
ChatIntegrationManager::beginConnect(ChatPlatform platform)
{
	if (platform == ChatPlatform::Kick) {
		setState(platform, ChatConnectionState::ComingSoon);
		return IntegrationResult<AuthorizationRequest>::failure(
			"Kick integration is coming soon");
	}
	if (!proUnlocked_) {
		setState(platform, ChatConnectionState::ProRequired);
		return IntegrationResult<AuthorizationRequest>::failure(
			"Chat integrations require ClipXtudio Pro");
	}
	auto result = transport_.beginAuthorization(platform);
	if (!result.succeeded() || result.value->state.empty() ||
	    result.value->state.size() < 8 ||
	    result.value->authorizationUrl.rfind("https://", 0) != 0) {
		setState(platform, ChatConnectionState::NetworkError);
		return IntegrationResult<AuthorizationRequest>::failure(
			result.error.empty()
				? "Invalid OAuth authorization response"
				: std::move(result.error));
	}
	auto request = std::move(*result.value);
	pendingStates_[platform] = request.state;
	setState(platform, ChatConnectionState::Connecting);
	return IntegrationResult<AuthorizationRequest>::success(
		std::move(request));
}

bool ChatIntegrationManager::completeConnect(
	ChatPlatform platform, std::string_view code,
	std::string_view returnedState, std::string *error)
{
	if (error)
		error->clear();
	if (!proUnlocked_ || platform == ChatPlatform::Kick) {
		if (error)
			*error = "Chat integration is unavailable";
		return false;
	}
	const auto pending = pendingStates_.find(platform);
	if (pending == pendingStates_.end() ||
	    pending->second != returnedState || code.empty()) {
		if (error)
			*error = "OAuth state validation failed";
		return false;
	}
	auto exchanged =
		transport_.exchangeAuthorizationCode(platform, code);
	if (!exchanged.succeeded()) {
		setState(platform, ChatConnectionState::NetworkError);
		if (error)
			*error = exchanged.error;
		return false;
	}
	if (!persistCredential(platform, *exchanged.value, error)) {
		setState(platform, ChatConnectionState::Disconnected);
		return false;
	}
	pendingStates_.erase(pending);
	setState(platform, ChatConnectionState::Connected);
	return true;
}

ChatPollResult ChatIntegrationManager::poll(ChatPlatform platform)
{
	if (platform == ChatPlatform::Kick)
		return {{}, ChatConnectionState::ComingSoon,
			"Kick integration is coming soon"};
	if (!proUnlocked_)
		return {{}, ChatConnectionState::ProRequired,
			"Chat integrations require ClipXtudio Pro"};
	std::string error;
	auto credential = loadCredential(platform, &error);
	if (!credential) {
		setState(platform, ChatConnectionState::Disconnected);
		return {{}, state(platform), std::move(error)};
	}
	if (credential->expiresAt <= std::chrono::system_clock::now()) {
		if (credential->refreshToken.empty()) {
			setState(platform, ChatConnectionState::TokenExpired);
			return {{}, state(platform),
				"OAuth token expired and cannot be refreshed"};
		}
		auto refreshed = transport_.refreshCredential(
			platform, credential->refreshToken);
		if (refreshed.succeeded() &&
		    refreshed.value->refreshToken.empty())
			refreshed.value->refreshToken =
				credential->refreshToken;
		if (!refreshed.succeeded() ||
		    !persistCredential(platform,
				       refreshed.value.value_or(
					       OAuthCredential{}),
				       &error)) {
			setState(platform, ChatConnectionState::TokenExpired);
			return {{}, state(platform),
				refreshed.error.empty() ? std::move(error)
							: refreshed.error};
		}
		credential = std::move(refreshed.value);
	}
	auto messages =
		transport_.readMessages(platform, credential->accessToken);
	if (!messages.succeeded()) {
		setState(platform, ChatConnectionState::NetworkError);
		return {{}, state(platform), std::move(messages.error)};
	}
	setState(platform, ChatConnectionState::Connected);
	return {std::move(*messages.value), state(platform), {}};
}

bool ChatIntegrationManager::disconnect(ChatPlatform platform,
					std::string *error)
{
	if (error)
		error->clear();
	std::string ignored;
	if (auto credential = loadCredential(platform, &ignored);
	    credential && !credential->accessToken.empty())
		transport_.revoke(platform, credential->accessToken);
	if (!secureStorage_.remove(storageKey(platform), error))
		return false;
	pendingStates_.erase(platform);
	setState(platform, platform == ChatPlatform::Kick
				   ? ChatConnectionState::ComingSoon
				   : proUnlocked_
					     ? ChatConnectionState::Disconnected
					     : ChatConnectionState::ProRequired);
	return true;
}

ChatConnectionState
ChatIntegrationManager::state(ChatPlatform platform) const noexcept
{
	const auto iterator = states_.find(platform);
	return iterator == states_.end() ? ChatConnectionState::Disconnected
					: iterator->second;
}

void ChatIntegrationManager::setProUnlocked(bool unlocked) noexcept
{
	proUnlocked_ = unlocked;
	for (const auto platform :
	     {ChatPlatform::Twitch, ChatPlatform::YouTube})
		setState(platform, unlocked ? ChatConnectionState::Disconnected
					   : ChatConnectionState::ProRequired);
}

std::string
ChatIntegrationManager::storageKey(ChatPlatform platform) const
{
	return "oauth.chat." + std::string(chatPlatformName(platform));
}

bool ChatIntegrationManager::persistCredential(
	ChatPlatform platform, const OAuthCredential &credential,
	std::string *error)
{
	if (credential.accessToken.empty() ||
	    credential.expiresAt.time_since_epoch().count() <= 0) {
		if (error)
			*error = "OAuth credential is invalid";
		return false;
	}
	const auto serialized = serializeCredential(credential);
	if (serialized.empty()) {
		if (error)
			*error = "OAuth credential could not be encoded";
		return false;
	}
	return secureStorage_.store(storageKey(platform), serialized, error);
}

std::optional<OAuthCredential>
ChatIntegrationManager::loadCredential(ChatPlatform platform,
				       std::string *error)
{
	const auto stored = secureStorage_.load(storageKey(platform));
	if (!stored.succeeded() || !stored.value) {
		if (error)
			*error = stored.message.empty()
					 ? "OAuth credential is unavailable"
					 : stored.message;
		return std::nullopt;
	}
	auto decoded = deserializeCredential(*stored.value);
	if (!decoded && error)
		*error = "Stored OAuth credential is invalid";
	return decoded;
}

void ChatIntegrationManager::setState(ChatPlatform platform,
				      ChatConnectionState state)
{
	states_[platform] = state;
}

} // namespace clipcoach::integrations
