#include "test-support.hpp"

#include <clipcoach/core/speech-engine.hpp>

namespace {

class FakeTransport final : public clipcoach::CloudSpeechTransport {
public:
	int calls{0};
	std::string receivedToken;

	clipcoach::SpeechRecognitionResult
	transcribe(const clipcoach::AudioSegment &, std::string_view language,
		   std::string_view token) override
	{
		++calls;
		receivedToken = std::string(token);
		return {"clip", std::string(language), 0.95,
			clipcoach::SpeechRecognitionError::None, {}};
	}
};

clipcoach::AudioSegment validSegment()
{
	clipcoach::AudioSegment segment;
	segment.sampleRateHz = 16000;
	segment.duration = std::chrono::seconds(2);
	segment.pcmMono.resize(32000);
	return segment;
}

} // namespace

int main()
{
	using namespace clipcoach;
	auto transport = std::make_shared<FakeTransport>();
	CloudSpeechEngine cloud(transport, [] { return "ephemeral-token"; },
				false);
	auto result = cloud.recognize(validSegment(), "es");
	clipcoach::test::expect(
		result.error == SpeechRecognitionError::ConsentRequired &&
			transport->calls == 0,
		"cloud engine must not transmit without explicit consent");

	cloud.setExplicitConsent(true);
	result = cloud.recognize(validSegment(), "es");
	clipcoach::test::expect(
		result.succeeded() && transport->calls == 1 &&
			transport->receivedToken == "ephemeral-token",
		"cloud engine must use a short-lived injected token");

	auto longSegment = validSegment();
	longSegment.duration = std::chrono::seconds(9);
	result = cloud.recognize(longSegment, "es");
	clipcoach::test::expect(
		result.error == SpeechRecognitionError::SegmentTooLong &&
			transport->calls == 1,
		"cloud engine must reject audio over its privacy limit");

	int localCalls = 0;
	LocalSpeechEngine local(
		[&localCalls](const AudioSegment &, std::string_view language) {
			++localCalls;
			return SpeechRecognitionResult{
				"save that", std::string(language), 0.9,
				SpeechRecognitionError::None, {}};
		});
	result = local.recognize(validSegment(), "en");
	clipcoach::test::expect(result.succeeded() && localCalls == 1,
			       "local transcriber must be injectable");

	return clipcoach::test::pass("speech-engine-test");
}
