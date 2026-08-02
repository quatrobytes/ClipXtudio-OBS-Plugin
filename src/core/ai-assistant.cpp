#include <clipcoach/core/ai-assistant.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace clipcoach {
namespace {

AiAssistantResult failure(AiError error, std::string code, std::string message)
{
	return {false, error, std::move(code), std::move(message), std::nullopt, {}, {}};
}

std::string requestId()
{
	std::array<unsigned char, 16> bytes{};
	if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
		return {};
	bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fU) | 0x40U);
	bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fU) | 0x80U);
	std::ostringstream output;
	output << std::hex << std::setfill('0');
	for (std::size_t index = 0; index < bytes.size(); ++index) {
		if (index == 4 || index == 6 || index == 8 || index == 10)
			output << '-';
		output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
	}
	return output.str();
}

std::string timestamp(std::int64_t milliseconds, bool comma)
{
	const auto hours = milliseconds / 3'600'000;
	milliseconds %= 3'600'000;
	const auto minutes = milliseconds / 60'000;
	milliseconds %= 60'000;
	const auto seconds = milliseconds / 1000;
	const auto millis = milliseconds % 1000;
	std::ostringstream output;
	output << std::setfill('0') << std::setw(2) << hours << ':' << std::setw(2) << minutes << ':' << std::setw(2)
	       << seconds << (comma ? ',' : '.') << std::setw(3) << millis;
	return output.str();
}

bool validResponse(const AiAssistantResponse &response)
{
	// Backends released before 0.5.59 may still return up to 30 hashtags.
	// Accept that wire format and normalize it before persistence and UI use.
	if (response.suggestedTitles.size() > 10 || response.hashtags.size() > 30 || response.caption.size() > 10'000 ||
	    response.summary.size() > 20'000 || response.qualityReason.size() > 2'000 || response.qualityScore < -1 ||
	    response.qualityScore > 100 || response.hookStrength < -1 || response.hookStrength > 100)
		return false;
	for (const auto &title : response.suggestedTitles)
		if (title.empty() || title.size() > 240)
			return false;
	for (const auto &hashtag : response.hashtags)
		if (hashtag.empty() || hashtag.size() > 100)
			return false;
	for (const auto &cue : response.subtitleCues)
		if (cue.startMilliseconds < 0 || cue.endMilliseconds <= cue.startMilliseconds || cue.text.empty() ||
		    cue.text.size() > 2000)
			return false;
	return response.srt.value_or("").size() <= 2'000'000 && response.vtt.value_or("").size() <= 2'000'000;
}

void normalizeResponse(AiAssistantResponse &response)
{
	if (response.hashtags.size() > 5)
		response.hashtags.resize(5);
}

} // namespace

AiAssistantService::AiAssistantService(AiApi &api, const FeatureGateService &featureGates,
				       AuthorizationTokenProvider tokenProvider, ClipPersistence clipPersistence,
				       SessionPersistence sessionPersistence)
	: api_(api),
	  featureGates_(featureGates),
	  tokenProvider_(std::move(tokenProvider)),
	  clipPersistence_(std::move(clipPersistence)),
	  sessionPersistence_(std::move(sessionPersistence))
{
}

AiAssistantService::~AiAssistantService()
{
	cancelAll();
}

void AiAssistantService::analyzeClip(const ClipMetadata &clip, std::string transcript,
				     const AiAssistantConfiguration &configuration, Completion completion)
{
	if (const auto invalid = validate(clip.id, transcript, configuration, false)) {
		if (completion)
			completion(*invalid);
		return;
	}
	AiAssistantRequest request{AiRequestScope::Clip,   clip.id,    clip.sessionId, std::move(transcript),
				   configuration.language, requestId()};
	api_.analyze(std::move(request), tokenProvider_ ? tokenProvider_() : std::string{},
		     [this, clip, configuration, completion = std::move(completion)](AiAssistantResult result) mutable {
			     handleClipResponse(clip, configuration, std::move(result), std::move(completion));
		     });
}

void AiAssistantService::summarizeSession(std::string sessionId, std::string transcript,
					  const AiAssistantConfiguration &configuration, Completion completion)
{
	if (const auto invalid = validate(sessionId, transcript, configuration, true)) {
		if (completion)
			completion(*invalid);
		return;
	}
	AiAssistantRequest request{AiRequestScope::Session, {},         sessionId, std::move(transcript),
				   configuration.language,  requestId()};
	api_.analyze(std::move(request), tokenProvider_ ? tokenProvider_() : std::string{},
		     [this, sessionId = std::move(sessionId), language = configuration.language,
		      completion = std::move(completion)](AiAssistantResult result) mutable {
			     if (result.success && (!result.response || !validResponse(*result.response) ||
						    result.response->summary.empty())) {
				     result = failure(AiError::InvalidResponse, "AI_INVALID_RESPONSE",
						      "AI backend returned an invalid response");
			     }
			     if (result.success)
				     normalizeResponse(*result.response);
			     if (result.success && sessionPersistence_) {
				     std::string error;
				     if (!sessionPersistence_(sessionId, result.response->summary, language, &error))
					     result = failure(AiError::Persistence, "AI_PERSISTENCE_FAILED",
							      std::move(error));
			     }
			     if (completion)
				     completion(std::move(result));
		     });
}

void AiAssistantService::cancelAll() noexcept
{
	api_.cancelAll();
}

bool AiAssistantService::isSupportedLanguage(AiLanguage language) noexcept
{
	return language == AiLanguage::Auto || language == AiLanguage::Spanish || language == AiLanguage::English;
}

const char *AiAssistantService::languageCode(AiLanguage language) noexcept
{
	switch (language) {
	case AiLanguage::Spanish:
		return "es";
	case AiLanguage::English:
		return "en";
	case AiLanguage::Auto:
		return "auto";
	}
	return "auto";
}

std::optional<AiAssistantResult> AiAssistantService::validate(const std::string &id, const std::string &transcript,
							      const AiAssistantConfiguration &configuration,
							      bool session) const
{
	if (!configuration.enabled)
		return failure(AiError::Disabled, "AI_DISABLED", "AI Assistant is disabled");
	if (!configuration.privacyConsent)
		return failure(AiError::ConsentRequired, "AI_CONSENT_REQUIRED",
			       "Review and accept the AI privacy notice first");
	const auto feature = session ? Feature::SessionRecap : Feature::AiCaptions;
	const auto gate = featureGates_.check(feature);
	if (!gate.allowed)
		return failure(AiError::ProRequired, gate.code, gate.message);
	if (id.empty() || transcript.empty() || transcript.size() > 100'000 ||
	    !isSupportedLanguage(configuration.language) || requestId().empty())
		return failure(AiError::InvalidInput, "AI_INVALID_INPUT",
			       "AI request text, language or identifier is invalid");
	return std::nullopt;
}

void AiAssistantService::handleClipResponse(ClipMetadata clip, AiAssistantConfiguration configuration,
					    AiAssistantResult result, Completion completion)
{
	if (result.success && (!result.response || !validResponse(*result.response) ||
			       result.response->suggestedTitles.empty() || result.response->caption.empty()))
		result = failure(AiError::InvalidResponse, "AI_INVALID_RESPONSE",
				 "AI backend returned an invalid response");
	if (result.success)
		normalizeResponse(*result.response);
	if (result.success) {
		const auto directory = configuration.subtitleDirectory.empty()
					       ? clip.filePath.parent_path() / "ClipXtudio Subtitles"
					       : configuration.subtitleDirectory;
		const auto stem = clip.filePath.stem().string();
		const auto &response = *result.response;
		std::string error;
		const auto srt = response.srt.value_or(SubtitleWriter::toSrt(response.subtitleCues));
		const auto vtt = response.vtt.value_or(SubtitleWriter::toVtt(response.subtitleCues));
		if (!srt.empty()) {
			result.srtPath = directory / (stem + ".srt");
			if (!SubtitleWriter::write(result.srtPath, srt, &error))
				result = failure(AiError::Persistence, "AI_SUBTITLE_WRITE_FAILED", error);
		}
		if (result.success && !vtt.empty()) {
			result.vttPath = directory / (stem + ".vtt");
			if (!SubtitleWriter::write(result.vttPath, vtt, &error))
				result = failure(AiError::Persistence, "AI_SUBTITLE_WRITE_FAILED", error);
		}
		if (result.success && clipPersistence_) {
			AiPersistedClipResult persisted;
			persisted.clipId = clip.id;
			persisted.suggestedTitles = response.suggestedTitles;
			persisted.selectedTitle = response.suggestedTitles.empty() ? clip.title
										   : response.suggestedTitles.front();
			persisted.caption = response.caption;
			persisted.hashtags = response.hashtags;
			persisted.summary = response.summary;
			persisted.subtitlePath = !result.srtPath.empty() ? result.srtPath : result.vttPath;
			persisted.language = configuration.language;
			persisted.hookStrength = response.hookStrength;
			persisted.qualityReason = response.qualityReason;
			if (response.qualityScore >= 0 && response.hookStrength >= 0) {
				persisted.score = std::clamp(static_cast<int>(std::lround(response.qualityScore * 0.6 +
											  response.hookStrength * 0.4)),
							     0, 100);
			} else if (response.qualityScore >= 0) {
				persisted.score = response.qualityScore;
			}
			if (!clipPersistence_(persisted, &error))
				result = failure(AiError::Persistence, "AI_PERSISTENCE_FAILED", error);
		}
	}
	if (completion)
		completion(std::move(result));
}

std::string SubtitleWriter::toSrt(const std::vector<SubtitleCue> &cues)
{
	std::ostringstream output;
	for (std::size_t index = 0; index < cues.size(); ++index)
		output << index + 1 << "\n"
		       << timestamp(cues[index].startMilliseconds, true) << " --> "
		       << timestamp(cues[index].endMilliseconds, true) << "\n"
		       << cues[index].text << "\n\n";
	return output.str();
}

std::string SubtitleWriter::toVtt(const std::vector<SubtitleCue> &cues)
{
	std::ostringstream output;
	output << "WEBVTT\n\n";
	for (const auto &cue : cues)
		output << timestamp(cue.startMilliseconds, false) << " --> " << timestamp(cue.endMilliseconds, false)
		       << "\n"
		       << cue.text << "\n\n";
	return output.str();
}

bool SubtitleWriter::write(const std::filesystem::path &path, const std::string &contents, std::string *error)
{
	if (path.empty() || contents.empty() || contents.size() > 2'000'000) {
		if (error)
			*error = "subtitle output is invalid";
		return false;
	}
	std::error_code fileError;
	std::filesystem::create_directories(path.parent_path(), fileError);
	if (fileError) {
		if (error)
			*error = "could not create subtitle directory";
		return false;
	}
	const auto temporary = path.string() + ".tmp";
	const auto backup = path.string() + ".bak";
	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
	output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
	output.close();
	if (!output) {
		std::filesystem::remove(temporary, fileError);
		if (error)
			*error = "could not write subtitle file";
		return false;
	}
	std::filesystem::remove(backup, fileError);
	fileError.clear();
	if (std::filesystem::exists(path, fileError)) {
		fileError.clear();
		std::filesystem::rename(path, backup, fileError);
		if (fileError) {
			std::filesystem::remove(temporary, fileError);
			if (error)
				*error = "could not replace existing subtitle file";
			return false;
		}
	}
	fileError.clear();
	std::filesystem::rename(temporary, path, fileError);
	if (fileError) {
		std::filesystem::remove(temporary, fileError);
		fileError.clear();
		if (std::filesystem::exists(backup, fileError))
			std::filesystem::rename(backup, path, fileError);
		if (error)
			*error = "could not publish subtitle file";
		return false;
	}
	std::filesystem::remove(backup, fileError);
	return true;
}

} // namespace clipcoach
