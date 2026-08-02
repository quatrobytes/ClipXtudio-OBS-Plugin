#pragma once

#include <clipcoach/core/trigger-engine.hpp>
#include <clipcoach/integrations/chat-integration.hpp>

#include <deque>
#include <map>
#include <set>

namespace clipcoach::integrations {

enum class ChatPulseReason {
	MessageBurst,
	EmojiBurst,
	ClipCommand,
	RepeatedWord,
};

struct ChatPulseConfiguration {
	bool enabled{false};
	std::chrono::seconds window{10};
	std::size_t burstMessageCount{12};
	std::size_t minimumUniqueAuthors{4};
	std::size_t emojiCount{18};
	std::size_t repeatedWordCount{7};
	std::chrono::seconds cooldown{15};
	bool clipCommandRequiresModerator{false};
	int sensitivity{70};
	int preRollSeconds{5};
	int postRollSeconds{5};
	TriggerAction action{TriggerAction::MarkMoment};
};

struct ChatPulseEvent {
	ChatPlatform platform{ChatPlatform::Twitch};
	ChatPulseReason reason{ChatPulseReason::MessageBurst};
	std::chrono::system_clock::time_point occurredAt;
	std::size_t messageCount{0};
	std::size_t uniqueAuthors{0};
	std::size_t emojiCount{0};
	std::string repeatedWord;
	double intensity{0.0};
};

class ChatPulseDetector final {
public:
	explicit ChatPulseDetector(ChatPulseConfiguration configuration = {});

	[[nodiscard]] bool configure(const ChatPulseConfiguration &configuration, std::string *error = nullptr);
	[[nodiscard]] std::optional<ChatPulseEvent> ingest(ChatPlatform platform, const ChatMessage &message);
	void reset();

private:
	ChatPulseConfiguration configuration_;
	std::map<ChatPlatform, std::deque<ChatMessage>> windows_;
	std::map<ChatPlatform, std::set<std::string>> messageIds_;
	std::map<ChatPlatform, std::optional<std::chrono::system_clock::time_point>> lastPulseAt_;
};

struct ChatPulseResult {
	std::optional<ChatPulseEvent> pulse;
	std::optional<TriggerEvent> triggerEvent;
	TriggerRejection rejection{TriggerRejection::None};
	std::string error;
};

class ChatPulseService final {
public:
	ChatPulseService(TriggerEngine &triggerEngine, bool proUnlocked);

	[[nodiscard]] bool configure(const ChatPulseConfiguration &configuration, std::string *error = nullptr);
	[[nodiscard]] ChatPulseResult process(ChatPlatform platform, const ChatMessage &message);
	void setProUnlocked(bool unlocked) noexcept;
	void reset();

private:
	TriggerEngine &triggerEngine_;
	ChatPulseDetector detector_;
	ChatPulseConfiguration configuration_;
	bool proUnlocked_{false};
};

struct ChatSyncResult {
	ChatConnectionState state{ChatConnectionState::Disconnected};
	std::size_t messageCount{0};
	std::size_t pulseCount{0};
	std::string error;
};

class ChatIntegrationController final {
public:
	ChatIntegrationController(ChatIntegrationManager &manager, ChatPulseService &pulseService);

	[[nodiscard]] ChatSyncResult sync(ChatPlatform platform);

private:
	ChatIntegrationManager &manager_;
	ChatPulseService &pulseService_;
};

} // namespace clipcoach::integrations
