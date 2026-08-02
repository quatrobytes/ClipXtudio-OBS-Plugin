#include "test-support.hpp"

#include <clipcoach/core/ai-assistant.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <chrono>
#include <filesystem>

namespace {

class MockAiApi final : public clipcoach::AiApi {
public:
	void analyze(clipcoach::AiAssistantRequest request, std::string token, Callback callback) override
	{
		lastRequest = std::move(request);
		lastToken = std::move(token);
		++calls;
		callback(next);
	}
	void cancelAll() noexcept override { cancelled = true; }

	clipcoach::AiAssistantRequest lastRequest;
	std::string lastToken;
	clipcoach::AiAssistantResult next;
	int calls{0};
	bool cancelled{false};
};

std::filesystem::path temporaryDirectory()
{
	return std::filesystem::temp_directory_path() /
	       ("clipcoach-ai-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

} // namespace

int main()
{
	using namespace clipcoach;
	using clipcoach::test::expect;

	FeatureGateService gates;
	MockAiApi api;
	AiPersistedClipResult persisted;
	bool persistedCalled = false;
	AiAssistantService service(
		api, gates, [] { return std::string("signed-license-token"); },
		[&](const AiPersistedClipResult &value, std::string *) {
			persisted = value;
			persistedCalled = true;
			return true;
		});

	ClipMetadata clip;
	clip.id = "clip-1";
	clip.sessionId = "session-1";
	clip.filePath = temporaryDirectory() / "source.mp4";
	clip.title = "Original";
	AiAssistantConfiguration configuration;
	configuration.enabled = true;
	configuration.privacyConsent = true;
	configuration.language = AiLanguage::Spanish;
	configuration.subtitleDirectory = clip.filePath.parent_path() / "subtitles";

	AiAssistantResult result;
	service.analyzeClip(clip, "Esta fue una jugada increíble.", configuration,
			    [&](AiAssistantResult value) { result = std::move(value); });
	expect(!result.success && result.error == AiError::ProRequired && api.calls == 0,
	       "Free must reject AI before the backend request");

	gates.setEntitlementState(EntitlementState::ProActive);
	api.next.success = true;
	api.next.response = AiAssistantResponse{{"La jugada imposible", "No parecía real"},
						"Una jugada para recordar.",
						{"#gaming", "#clip"},
						"El jugador completa una jugada decisiva.",
						{{0, 1500, "Esta fue una jugada increíble."}}};
	api.next.response->qualityScore = 82;
	api.next.response->hookStrength = 92;
	api.next.response->qualityReason = "Gancho claro y buen remate";
	service.analyzeClip(clip, "Esta fue una jugada increíble.", configuration,
			    [&](AiAssistantResult value) { result = std::move(value); });
	expect(result.success && api.calls == 1, "Pro must accept a valid AI response");
	expect(api.lastRequest.clipId == clip.id && api.lastRequest.sessionId == clip.sessionId &&
		       api.lastRequest.transcript == "Esta fue una jugada increíble." &&
		       api.lastRequest.language == AiLanguage::Spanish && !api.lastRequest.requestId.empty(),
	       "AI request must contain scoped text, language and idempotency id");
	expect(api.lastToken == "signed-license-token", "AI request must use account license auth");
	expect(persistedCalled && persisted.clipId == clip.id && persisted.selectedTitle == "La jugada imposible" &&
		       persisted.caption == "Una jugada para recordar." && persisted.hashtags.size() == 2 &&
		       persisted.score == 86 && persisted.hookStrength == 92,
	       "mock response must be linked and persisted to the clip");
	expect(std::filesystem::is_regular_file(result.srtPath) && std::filesystem::is_regular_file(result.vttPath),
	       "subtitle cues must create SRT and VTT files");
	expect(SubtitleWriter::toSrt({{1000, 2500, "Hello"}}).find("00:00:01,000 --> 00:00:02,500") !=
			       std::string::npos &&
		       SubtitleWriter::toVtt({{1000, 2500, "Hello"}}).find("WEBVTT") == 0,
	       "subtitle writers must use valid SRT and VTT timestamps");

	auto excessiveHashtags = *api.next.response;
	excessiveHashtags.hashtags = {"#one", "#two", "#three", "#four", "#five", "#six"};
	api.next.response = std::move(excessiveHashtags);
	service.analyzeClip(clip, "text", configuration, [&](AiAssistantResult value) { result = std::move(value); });
	expect(result.success && result.response.has_value() && result.response->hashtags.size() == 5 &&
		       persisted.hashtags.size() == 5,
	       "legacy backend responses must be accepted and normalized to five publishable hashtags");

	configuration.language = static_cast<AiLanguage>(99);
	service.analyzeClip(clip, "text", configuration, [&](AiAssistantResult value) { result = std::move(value); });
	expect(!result.success && result.error == AiError::InvalidInput,
	       "unsupported AI language must fail before network");

	configuration.language = AiLanguage::English;
	api.next = {false, AiError::UsageLimit, "AI_USAGE_LIMIT", "Monthly credits exhausted"};
	service.analyzeClip(clip, "English transcript", configuration,
			    [&](AiAssistantResult value) { result = std::move(value); });
	expect(!result.success && result.error == AiError::UsageLimit, "backend usage errors must remain controlled");

	gates.setEntitlementState(EntitlementState::Revoked);
	service.analyzeClip(clip, "text", configuration, [&](AiAssistantResult value) { result = std::move(value); });
	expect(!result.success && result.error == AiError::ProRequired, "backend-revoked licenses must block AI");

	std::error_code error;
	std::filesystem::remove_all(clip.filePath.parent_path(), error);
	return clipcoach::test::pass("ai-assistant-test");
}
