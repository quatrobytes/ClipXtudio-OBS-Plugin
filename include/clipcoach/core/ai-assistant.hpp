#pragma once

#include <clipcoach/core/clip-metadata.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach {

enum class AiLanguage { Auto, Spanish, English };
enum class AiRequestScope { Clip, Session };
enum class AiError {
	None,
	Disabled,
	ConsentRequired,
	ProRequired,
	InvalidInput,
	UsageLimit,
	Network,
	InvalidResponse,
	Persistence,
};

struct SubtitleCue {
	std::int64_t startMilliseconds{0};
	std::int64_t endMilliseconds{0};
	std::string text;
};

struct AiAssistantRequest {
	AiRequestScope scope{AiRequestScope::Clip};
	std::string clipId;
	std::string sessionId;
	std::string transcript;
	AiLanguage language{AiLanguage::Auto};
	std::string requestId;
};

struct AiAssistantResponse {
	std::vector<std::string> suggestedTitles;
	std::string caption;
	std::vector<std::string> hashtags; // Social publishing limit: at most five.
	std::string summary;
	std::vector<SubtitleCue> subtitleCues;
	std::optional<std::string> srt;
	std::optional<std::string> vtt;
	std::uint32_t usage{0};
	std::uint32_t limit{0};
	int qualityScore{-1};
	int hookStrength{-1};
	std::string qualityReason;
};

struct AiAssistantResult {
	bool success{false};
	AiError error{AiError::None};
	std::string code;
	std::string message;
	std::optional<AiAssistantResponse> response;
	std::filesystem::path srtPath;
	std::filesystem::path vttPath;
};

class AiApi {
public:
	using Callback = std::function<void(AiAssistantResult)>;
	virtual ~AiApi() = default;
	virtual void analyze(AiAssistantRequest request, std::string authorizationToken, Callback callback) = 0;
	virtual void cancelAll() noexcept = 0;
};

struct AiAssistantConfiguration {
	bool enabled{false};
	bool privacyConsent{false};
	AiLanguage language{AiLanguage::Auto};
	std::filesystem::path subtitleDirectory;
};

struct AiPersistedClipResult {
	std::string clipId;
	std::vector<std::string> suggestedTitles;
	std::string selectedTitle;
	std::string caption;
	std::vector<std::string> hashtags;
	std::string summary;
	std::filesystem::path subtitlePath;
	AiLanguage language{AiLanguage::Auto};
	int score{-1};
	int hookStrength{-1};
	std::string qualityReason;
};

class FeatureGateService;

class AiAssistantService final {
public:
	using AuthorizationTokenProvider = std::function<std::string()>;
	using ClipPersistence = std::function<bool(const AiPersistedClipResult &, std::string *)>;
	using SessionPersistence =
		std::function<bool(const std::string &, const std::string &, AiLanguage, std::string *)>;
	using Completion = std::function<void(AiAssistantResult)>;

	AiAssistantService(AiApi &api, const FeatureGateService &featureGates, AuthorizationTokenProvider tokenProvider,
			   ClipPersistence clipPersistence, SessionPersistence sessionPersistence = {});
	~AiAssistantService();

	void analyzeClip(const ClipMetadata &clip, std::string transcript,
			 const AiAssistantConfiguration &configuration, Completion completion = {});
	void summarizeSession(std::string sessionId, std::string transcript,
			      const AiAssistantConfiguration &configuration, Completion completion = {});
	void cancelAll() noexcept;

	[[nodiscard]] static bool isSupportedLanguage(AiLanguage language) noexcept;
	[[nodiscard]] static const char *languageCode(AiLanguage language) noexcept;

private:
	[[nodiscard]] std::optional<AiAssistantResult> validate(const std::string &id, const std::string &transcript,
								const AiAssistantConfiguration &configuration,
								bool session) const;
	void handleClipResponse(ClipMetadata clip, AiAssistantConfiguration configuration, AiAssistantResult result,
				Completion completion);

	AiApi &api_;
	const FeatureGateService &featureGates_;
	AuthorizationTokenProvider tokenProvider_;
	ClipPersistence clipPersistence_;
	SessionPersistence sessionPersistence_;
};

class SubtitleWriter final {
public:
	[[nodiscard]] static std::string toSrt(const std::vector<SubtitleCue> &cues);
	[[nodiscard]] static std::string toVtt(const std::vector<SubtitleCue> &cues);
	[[nodiscard]] static bool write(const std::filesystem::path &path, const std::string &contents,
					std::string *error = nullptr);
};

} // namespace clipcoach
