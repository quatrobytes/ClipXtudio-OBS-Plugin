#include <clipcoach/core/voice-trigger-service.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>
#include <sstream>

namespace clipcoach {
namespace {

void appendSpace(std::string &output)
{
	if (!output.empty() && output.back() != ' ')
		output.push_back(' ');
}

std::vector<std::string> words(const std::string &value)
{
	std::vector<std::string> result;
	std::istringstream stream(value);
	std::string word;
	while (stream >> word)
		result.push_back(std::move(word));
	return result;
}

std::string joinWords(const std::vector<std::string> &values, std::size_t begin, std::size_t count)
{
	std::string result;
	for (std::size_t index = begin; index < begin + count; ++index) {
		if (!result.empty())
			result.push_back(' ');
		result += values[index];
	}
	return result;
}

std::size_t levenshteinDistance(std::string_view left, std::string_view right)
{
	std::vector<std::size_t> previous(right.size() + 1);
	std::vector<std::size_t> current(right.size() + 1);
	for (std::size_t index = 0; index <= right.size(); ++index)
		previous[index] = index;
	for (std::size_t leftIndex = 1; leftIndex <= left.size(); ++leftIndex) {
		current[0] = leftIndex;
		for (std::size_t rightIndex = 1; rightIndex <= right.size(); ++rightIndex) {
			const auto substitution =
				previous[rightIndex - 1] + (left[leftIndex - 1] == right[rightIndex - 1] ? 0 : 1);
			current[rightIndex] =
				std::min({previous[rightIndex] + 1, current[rightIndex - 1] + 1, substitution});
		}
		previous.swap(current);
	}
	return previous.back();
}

double similarity(std::string_view left, std::string_view right)
{
	const auto longest = std::max(left.size(), right.size());
	if (longest == 0)
		return 1.0;
	return 1.0 - static_cast<double>(levenshteinDistance(left, right)) / static_cast<double>(longest);
}

double matchingThreshold(int sensitivity)
{
	return 0.82 + static_cast<double>(std::clamp(sensitivity, 0, 100)) * 0.0015;
}

double confidenceThreshold(int sensitivity)
{
	// Quantized local Whisper confidence is an average across all decoded
	// tokens and is naturally lower than SAPI grammar confidence. Keep the
	// threshold useful for rejecting noise without making exact commands
	// unreachable at high sensitivity.
	return 0.30 + static_cast<double>(std::clamp(sensitivity, 0, 100)) * 0.0025;
}

bool supportedLanguage(const std::string &language)
{
	return language == "auto" || language == "es" || language == "en" || language.rfind("es-", 0) == 0 ||
	       language.rfind("en-", 0) == 0;
}

} // namespace

std::string VoicePhraseMatcher::normalize(std::string_view text)
{
	std::string output;
	output.reserve(text.size());
	for (std::size_t index = 0; index < text.size();) {
		const auto current = static_cast<unsigned char>(text[index]);
		if (current < 0x80) {
			if (std::isalnum(current))
				output.push_back(static_cast<char>(std::tolower(current)));
			else
				appendSpace(output);
			++index;
			continue;
		}
		if (index + 1 < text.size() && current == 0xC3) {
			const auto next = static_cast<unsigned char>(text[index + 1]);
			char replacement = '\0';
			switch (next) {
			case 0x81:
			case 0xA1:
				replacement = 'a';
				break;
			case 0x89:
			case 0xA9:
				replacement = 'e';
				break;
			case 0x8D:
			case 0xAD:
				replacement = 'i';
				break;
			case 0x93:
			case 0xB3:
				replacement = 'o';
				break;
			case 0x9A:
			case 0xBA:
			case 0x9C:
			case 0xBC:
				replacement = 'u';
				break;
			case 0x91:
			case 0xB1:
				output += "\xC3\xB1";
				break;
			default:
				appendSpace(output);
				break;
			}
			if (replacement != '\0')
				output.push_back(replacement);
			index += 2;
			continue;
		}
		appendSpace(output);
		++index;
		while (index < text.size() && (static_cast<unsigned char>(text[index]) & 0xC0) == 0x80)
			++index;
	}
	while (!output.empty() && output.back() == ' ')
		output.pop_back();
	return output;
}

std::optional<VoicePhraseMatch> VoicePhraseMatcher::bestMatch(std::string_view transcript,
							      const std::vector<std::string> &phrases, int sensitivity)
{
	const auto normalizedTranscript = normalize(transcript);
	const auto transcriptWords = words(normalizedTranscript);
	if (transcriptWords.empty())
		return std::nullopt;

	std::optional<VoicePhraseMatch> best;
	for (const auto &configured : phrases) {
		const auto normalizedPhrase = normalize(configured);
		const auto phraseWords = words(normalizedPhrase);
		if (phraseWords.empty() || phraseWords.size() > transcriptWords.size())
			continue;
		for (std::size_t begin = 0; begin + phraseWords.size() <= transcriptWords.size(); ++begin) {
			const auto candidate = joinWords(transcriptWords, begin, phraseWords.size());
			const auto currentSimilarity =
				candidate == normalizedPhrase ? 1.0 : similarity(candidate, normalizedPhrase);
			const auto accepted = phraseWords.size() == 1
						      ? currentSimilarity == 1.0
						      : currentSimilarity >= matchingThreshold(sensitivity);
			if (accepted && (!best || currentSimilarity > best->similarity ||
					 (currentSimilarity == best->similarity &&
					  normalizedPhrase.size() >
						  VoicePhraseMatcher::normalize(best->configuredPhrase).size()))) {
				best = VoicePhraseMatch{configured, normalizedTranscript, currentSimilarity};
			}
		}
	}
	return best;
}

VoiceTriggerService::VoiceTriggerService(SpeechEngine &speechEngine, TriggerEngine &triggerEngine, bool proUnlocked)
	: speechEngine_(speechEngine),
	  triggerEngine_(triggerEngine),
	  proUnlocked_(proUnlocked)
{
	triggerEngine_.setProUnlocked(proUnlocked);
}

bool VoiceTriggerService::configure(const VoiceTriggerConfiguration &configuration, std::string *error)
{
	if (error)
		error->clear();
	if (configuration.enabled && !proUnlocked_) {
		if (error)
			*error = "Voice Trigger requires ClipXtudio Pro";
		return false;
	}
	if (configuration.sensitivity < 0 || configuration.sensitivity > 100 || configuration.preRollSeconds < 0 ||
	    configuration.preRollSeconds > 120 || configuration.postRollSeconds < 0 ||
	    configuration.postRollSeconds > 120 || configuration.cooldown.count() < 0 ||
	    configuration.cooldown > std::chrono::hours(1) || configuration.phrases.empty() ||
	    configuration.phrases.size() > 50 || !supportedLanguage(configuration.language) ||
	    std::any_of(configuration.phrases.begin(), configuration.phrases.end(), [](const std::string &phrase) {
		    return VoicePhraseMatcher::normalize(phrase).empty() || phrase.size() > 120;
	    })) {
		if (error)
			*error = "Voice Trigger configuration is invalid";
		return false;
	}
	std::set<std::string> normalizedPhrases;
	for (const auto &phrase : configuration.phrases) {
		if (!normalizedPhrases.insert(VoicePhraseMatcher::normalize(phrase)).second) {
			if (error)
				*error = "Voice Trigger phrases must be unique";
			return false;
		}
	}

	TriggerConfiguration triggerConfig;
	triggerConfig.enabled = configuration.enabled;
	triggerConfig.preRollSeconds = configuration.preRollSeconds;
	triggerConfig.postRollSeconds = configuration.postRollSeconds;
	triggerConfig.sensitivity = configuration.sensitivity;
	triggerConfig.action = configuration.action;
	triggerConfig.cooldown = configuration.cooldown;
	if (!triggerEngine_.setConfiguration(SmartTriggerType::Voice, triggerConfig, error))
		return false;
	configuration_ = configuration;
	return true;
}

const VoiceTriggerConfiguration &VoiceTriggerService::configuration() const noexcept
{
	return configuration_;
}

void VoiceTriggerService::setProUnlocked(bool unlocked) noexcept
{
	proUnlocked_ = unlocked;
	triggerEngine_.setProUnlocked(unlocked);
	if (!unlocked)
		configuration_.enabled = false;
}

VoiceTriggerResult VoiceTriggerService::process(const AudioSegment &segment)
{
	if (!proUnlocked_)
		return {VoiceTriggerStatus::ProRequired, {}, {}, "Voice Trigger requires ClipXtudio Pro"};
	if (!configuration_.enabled)
		return {VoiceTriggerStatus::Disabled, {}, {}, "Voice Trigger is disabled"};

	const auto recognition = speechEngine_.recognize(segment, configuration_.language);
	return processRecognition(recognition, segment.startedAt, segment.duration);
}

VoiceTriggerResult VoiceTriggerService::processRecognition(const SpeechRecognitionResult &recognition,
							   std::chrono::system_clock::time_point occurredAt,
							   std::chrono::milliseconds /*duration*/)
{
	if (!proUnlocked_)
		return {VoiceTriggerStatus::ProRequired, {}, {}, "Voice Trigger requires ClipXtudio Pro"};
	if (!configuration_.enabled)
		return {VoiceTriggerStatus::Disabled, {}, {}, "Voice Trigger is disabled"};
	if (!recognition.succeeded())
		return {VoiceTriggerStatus::RecognitionFailed, {}, {}, recognition.errorMessage};
	if (recognition.confidence < confidenceThreshold(configuration_.sensitivity))
		return {VoiceTriggerStatus::LowConfidence,
			{},
			{},
			"Speech confidence is below the configured threshold"};

	const auto match = VoicePhraseMatcher::bestMatch(recognition.transcript, configuration_.phrases,
							 configuration_.sensitivity);
	if (!match)
		return {VoiceTriggerStatus::NoPhraseMatch, {}, {}, "No configured voice phrase matched"};

	if (lastMatchAt_ && (occurredAt <= *lastMatchAt_ || occurredAt - *lastMatchAt_ < configuration_.cooldown))
		return {VoiceTriggerStatus::Cooldown, {}, match, "Voice Trigger is in cooldown"};

	TriggerSignal signal;
	signal.type = SmartTriggerType::Voice;
	signal.occurredAt = occurredAt;
	// For a constrained command grammar, exact phrase similarity is the
	// strongest evidence. Raw token probability from quantized Whisper is not
	// calibrated like a grammar recognizer, so combine both signals instead of
	// letting the lower raw token average make high sensitivity unusable.
	signal.voiceConfidence = match->similarity == 1.0
					 ? 1.0
					 : std::clamp(match->similarity * 0.8 + recognition.confidence * 0.2, 0.0, 1.0);
	// Score the saved moment, not only the sub-second spoken command. The final
	// virality score is replaced by the backend AI analysis after persistence.
	signal.durationSeconds = std::max(5, configuration_.preRollSeconds + configuration_.postRollSeconds);
	signal.text = recognition.transcript;
	signal.keyword = match->configuredPhrase;
	const auto result = triggerEngine_.process(signal);
	if (!result.event)
		return {result.rejection == TriggerRejection::Cooldown ||
					result.rejection == TriggerRejection::Duplicate
				? VoiceTriggerStatus::Cooldown
				: VoiceTriggerStatus::TriggerRejected,
			{},
			match,
			"Trigger engine rejected the voice event"};

	lastMatchAt_ = occurredAt;
	return {VoiceTriggerStatus::EventCreated, result.event, match, {}};
}

} // namespace clipcoach
