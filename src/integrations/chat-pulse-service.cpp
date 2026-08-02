#include <clipcoach/integrations/chat-pulse-service.hpp>
#include <clipcoach/core/voice-trigger-service.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace clipcoach::integrations {
namespace {

std::string lowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value;
}

bool containsClipCommand(const ChatMessage &message, const ChatPulseConfiguration &configuration)
{
	if (configuration.clipCommandRequiresModerator && !message.authorIsModerator)
		return false;
	std::istringstream stream(lowerAscii(message.text));
	std::string token;
	while (stream >> token) {
		while (token.size() > 1 && std::ispunct(static_cast<unsigned char>(token.back())))
			token.pop_back();
		if (token == "!clip")
			return true;
	}
	return false;
}

std::optional<std::uint32_t> nextCodePoint(std::string_view value, std::size_t &offset)
{
	if (offset >= value.size())
		return std::nullopt;
	const auto first = static_cast<unsigned char>(value[offset++]);
	if (first < 0x80)
		return first;
	int continuationCount = 0;
	std::uint32_t codePoint = 0;
	if ((first & 0xE0) == 0xC0) {
		continuationCount = 1;
		codePoint = first & 0x1F;
	} else if ((first & 0xF0) == 0xE0) {
		continuationCount = 2;
		codePoint = first & 0x0F;
	} else if ((first & 0xF8) == 0xF0) {
		continuationCount = 3;
		codePoint = first & 0x07;
	} else {
		return std::nullopt;
	}
	if (offset + continuationCount > value.size())
		return std::nullopt;
	for (int index = 0; index < continuationCount; ++index) {
		const auto continuation = static_cast<unsigned char>(value[offset++]);
		if ((continuation & 0xC0) != 0x80)
			return std::nullopt;
		codePoint = (codePoint << 6) | (continuation & 0x3F);
	}
	return codePoint;
}

std::size_t emojiCount(std::string_view text)
{
	std::size_t count = 0;
	std::size_t offset = 0;
	while (offset < text.size()) {
		const auto point = nextCodePoint(text, offset);
		if (!point)
			continue;
		if ((*point >= 0x1F300 && *point <= 0x1FAFF) || (*point >= 0x2600 && *point <= 0x27BF) ||
		    (*point >= 0x1F1E6 && *point <= 0x1F1FF))
			++count;
	}
	return count;
}

std::vector<std::string> meaningfulWords(std::string text)
{
	text = VoicePhraseMatcher::normalize(text);
	static const std::set<std::string> ignored = {"the", "and", "that", "this", "que", "para", "por",
						      "con", "una", "uno",  "los",  "las", "del",  "clip"};
	std::istringstream stream(text);
	std::vector<std::string> result;
	std::string word;
	while (stream >> word) {
		if (word.size() >= 3 && ignored.count(word) == 0)
			result.push_back(std::move(word));
	}
	return result;
}

double ratio(std::size_t actual, std::size_t threshold)
{
	return threshold == 0 ? 1.0
			      : std::clamp(static_cast<double>(actual) / static_cast<double>(threshold), 0.0, 1.0);
}

} // namespace

ChatPulseDetector::ChatPulseDetector(ChatPulseConfiguration configuration)
{
	std::string ignored;
	if (!configure(configuration, &ignored))
		configuration_ = {};
}

bool ChatPulseDetector::configure(const ChatPulseConfiguration &configuration, std::string *error)
{
	if (error)
		error->clear();
	if (configuration.window <= std::chrono::seconds::zero() || configuration.window > std::chrono::minutes(5) ||
	    configuration.burstMessageCount < 2 || configuration.burstMessageCount > 10000 ||
	    configuration.minimumUniqueAuthors < 1 ||
	    configuration.minimumUniqueAuthors > configuration.burstMessageCount || configuration.emojiCount < 2 ||
	    configuration.repeatedWordCount < 2 || configuration.cooldown < std::chrono::seconds::zero() ||
	    configuration.cooldown > std::chrono::hours(1) || configuration.sensitivity < 0 ||
	    configuration.sensitivity > 100 || configuration.preRollSeconds < 0 || configuration.preRollSeconds > 120 ||
	    configuration.postRollSeconds < 0 || configuration.postRollSeconds > 120) {
		if (error)
			*error = "Chat Pulse configuration is invalid";
		return false;
	}
	configuration_ = configuration;
	reset();
	return true;
}

std::optional<ChatPulseEvent> ChatPulseDetector::ingest(ChatPlatform platform, const ChatMessage &message)
{
	if (!configuration_.enabled || message.id.empty() || message.id.size() > 256 || message.authorId.empty() ||
	    message.authorId.size() > 256 || message.text.empty() || message.text.size() > 4096)
		return std::nullopt;
	auto &window = windows_[platform];
	auto &ids = messageIds_[platform];
	if (!message.id.empty() && ids.count(message.id) != 0)
		return std::nullopt;
	if (!window.empty() && message.sentAt < window.back().sentAt)
		return std::nullopt;
	while (!window.empty() && message.sentAt - window.front().sentAt > configuration_.window) {
		ids.erase(window.front().id);
		window.pop_front();
	}
	window.push_back(message);
	if (!message.id.empty())
		ids.insert(message.id);

	auto &lastPulse = lastPulseAt_[platform];
	if (lastPulse && message.sentAt - *lastPulse < configuration_.cooldown)
		return std::nullopt;

	std::set<std::string> authors;
	std::size_t emojis = 0;
	std::map<std::string, std::size_t> frequencies;
	for (const auto &current : window) {
		authors.insert(current.authorId);
		emojis += emojiCount(current.text);
		for (const auto &word : meaningfulWords(current.text))
			++frequencies[word];
	}

	ChatPulseEvent event;
	event.platform = platform;
	event.occurredAt = message.sentAt;
	event.messageCount = window.size();
	event.uniqueAuthors = authors.size();
	event.emojiCount = emojis;

	if (containsClipCommand(message, configuration_)) {
		event.reason = ChatPulseReason::ClipCommand;
		event.intensity = 1.0;
	} else if (emojis >= configuration_.emojiCount) {
		event.reason = ChatPulseReason::EmojiBurst;
		event.intensity = ratio(emojis, configuration_.emojiCount);
	} else if (window.size() >= configuration_.burstMessageCount &&
		   authors.size() >= configuration_.minimumUniqueAuthors) {
		event.reason = ChatPulseReason::MessageBurst;
		event.intensity = std::max(ratio(window.size(), configuration_.burstMessageCount),
					   ratio(authors.size(), configuration_.minimumUniqueAuthors));
	} else {
		const auto repeated = std::max_element(frequencies.begin(), frequencies.end(),
						       [](const auto &left, const auto &right) {
							       return left.second < right.second;
						       });
		if (repeated == frequencies.end() || repeated->second < configuration_.repeatedWordCount)
			return std::nullopt;
		event.reason = ChatPulseReason::RepeatedWord;
		event.repeatedWord = repeated->first;
		event.intensity = ratio(repeated->second, configuration_.repeatedWordCount);
	}
	lastPulse = message.sentAt;
	return event;
}

void ChatPulseDetector::reset()
{
	windows_.clear();
	messageIds_.clear();
	lastPulseAt_.clear();
}

ChatPulseService::ChatPulseService(TriggerEngine &triggerEngine, bool proUnlocked)
	: triggerEngine_(triggerEngine),
	  proUnlocked_(proUnlocked)
{
	triggerEngine_.setProUnlocked(proUnlocked);
}

bool ChatPulseService::configure(const ChatPulseConfiguration &configuration, std::string *error)
{
	if (configuration.enabled && !proUnlocked_) {
		if (error)
			*error = "Chat Pulse requires ClipXtudio Pro";
		return false;
	}
	if (!detector_.configure(configuration, error))
		return false;
	TriggerConfiguration triggerConfig;
	triggerConfig.enabled = configuration.enabled;
	triggerConfig.preRollSeconds = configuration.preRollSeconds;
	triggerConfig.postRollSeconds = configuration.postRollSeconds;
	triggerConfig.sensitivity = configuration.sensitivity;
	triggerConfig.action = configuration.action;
	triggerConfig.cooldown = configuration.cooldown;
	if (!triggerEngine_.setConfiguration(SmartTriggerType::ChatPulse, triggerConfig, error))
		return false;
	configuration_ = configuration;
	return true;
}

ChatPulseResult ChatPulseService::process(ChatPlatform platform, const ChatMessage &message)
{
	if (!proUnlocked_)
		return {{}, {}, TriggerRejection::ProRequired, "Chat Pulse requires ClipXtudio Pro"};
	const auto pulse = detector_.ingest(platform, message);
	if (!pulse)
		return {};
	TriggerSignal signal;
	signal.type = SmartTriggerType::ChatPulse;
	signal.occurredAt = pulse->occurredAt;
	signal.chatActivity = pulse->intensity;
	signal.durationSeconds = static_cast<int>(configuration_.window.count());
	signal.keyword = pulse->repeatedWord;
	const auto triggered = triggerEngine_.process(signal);
	return {pulse, triggered.event, triggered.rejection,
		triggered.event ? std::string{} : "Trigger engine rejected Chat Pulse"};
}

void ChatPulseService::setProUnlocked(bool unlocked) noexcept
{
	proUnlocked_ = unlocked;
	triggerEngine_.setProUnlocked(unlocked);
	if (!unlocked)
		configuration_.enabled = false;
}

void ChatPulseService::reset()
{
	detector_.reset();
}

ChatIntegrationController::ChatIntegrationController(ChatIntegrationManager &manager, ChatPulseService &pulseService)
	: manager_(manager),
	  pulseService_(pulseService)
{
}

ChatSyncResult ChatIntegrationController::sync(ChatPlatform platform)
{
	auto poll = manager_.poll(platform);
	ChatSyncResult result;
	result.state = poll.state;
	result.messageCount = poll.messages.size();
	result.error = std::move(poll.error);
	if (poll.state != ChatConnectionState::Connected)
		return result;
	for (const auto &message : poll.messages) {
		if (pulseService_.process(platform, message).triggerEvent)
			++result.pulseCount;
	}
	return result;
}

} // namespace clipcoach::integrations
