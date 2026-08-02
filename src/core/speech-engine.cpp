#include <clipcoach/core/speech-engine.hpp>

#include <algorithm>
#include <exception>

namespace clipcoach {
namespace {

SpeechRecognitionResult failure(SpeechRecognitionError error,
				std::string message)
{
	SpeechRecognitionResult result;
	result.error = error;
	result.errorMessage = std::move(message);
	return result;
}

std::chrono::milliseconds measuredDuration(const AudioSegment &segment)
{
	if (segment.sampleRateHz <= 0)
		return {};
	return std::chrono::milliseconds(
		static_cast<std::int64_t>(segment.pcmMono.size()) * 1000 /
		segment.sampleRateHz);
}

SpeechRecognitionResult validateSegment(
	const AudioSegment &segment,
	std::chrono::milliseconds maximumSegment)
{
	if (segment.sampleRateHz < 8000 || segment.sampleRateHz > 48000 ||
	    segment.pcmMono.empty() || segment.duration.count() <= 0)
		return failure(SpeechRecognitionError::InvalidAudio,
			       "Audio must be mono PCM at 8-48 kHz");
	if (std::max(segment.duration, measuredDuration(segment)) >
	    maximumSegment)
		return failure(SpeechRecognitionError::SegmentTooLong,
			       "Audio segment exceeds the configured privacy limit");
	return {};
}

} // namespace

LocalSpeechEngine::LocalSpeechEngine(
	Transcriber transcriber, std::chrono::milliseconds maximumSegment)
	: transcriber_(std::move(transcriber)),
	  maximumSegment_(maximumSegment)
{
}

SpeechRecognitionResult
LocalSpeechEngine::recognize(const AudioSegment &segment,
			     std::string_view languageHint)
{
	const auto validation = validateSegment(segment, maximumSegment_);
	if (!validation.succeeded())
		return validation;
	if (!transcriber_)
		return failure(SpeechRecognitionError::Unavailable,
			       "No local speech backend is installed");
	try {
		return transcriber_(segment, languageHint);
	} catch (const std::exception &error) {
		return failure(SpeechRecognitionError::Unavailable,
			       std::string("Local speech backend failed: ") +
				       error.what());
	} catch (...) {
		return failure(SpeechRecognitionError::Unavailable,
			       "Local speech backend failed");
	}
}

CloudSpeechEngine::CloudSpeechEngine(
	std::shared_ptr<CloudSpeechTransport> transport,
	TokenProvider tokenProvider, bool explicitConsent,
	std::chrono::milliseconds maximumSegment)
	: transport_(std::move(transport)),
	  tokenProvider_(std::move(tokenProvider)),
	  explicitConsent_(explicitConsent),
	  maximumSegment_(maximumSegment)
{
}

void CloudSpeechEngine::setExplicitConsent(bool consent) noexcept
{
	explicitConsent_ = consent;
}

SpeechRecognitionResult
CloudSpeechEngine::recognize(const AudioSegment &segment,
			     std::string_view languageHint)
{
	if (!explicitConsent_)
		return failure(SpeechRecognitionError::ConsentRequired,
			       "Cloud speech requires explicit user consent");
	const auto validation = validateSegment(segment, maximumSegment_);
	if (!validation.succeeded())
		return validation;
	if (!transport_)
		return failure(SpeechRecognitionError::Unavailable,
			       "Cloud speech transport is unavailable");

	std::string token = tokenProvider_ ? tokenProvider_() : std::string{};
	if (token.empty())
		return failure(
			SpeechRecognitionError::AuthenticationRequired,
			"A short-lived account token is required");
	try {
		auto result =
			transport_->transcribe(segment, languageHint, token);
		std::fill(token.begin(), token.end(), '\0');
		return result;
	} catch (const std::exception &error) {
		std::fill(token.begin(), token.end(), '\0');
		return failure(SpeechRecognitionError::TransportFailure,
			       std::string("Cloud speech request failed: ") +
				       error.what());
	} catch (...) {
		std::fill(token.begin(), token.end(), '\0');
		return failure(SpeechRecognitionError::TransportFailure,
			       "Cloud speech request failed");
	}
}

} // namespace clipcoach
