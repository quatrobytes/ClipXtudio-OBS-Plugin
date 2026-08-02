#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace clipcoach {

enum class SpeechProcessingMode {
	Local,
	Cloud,
};

struct AudioSegment {
	std::vector<std::int16_t> pcmMono;
	int sampleRateHz{16000};
	std::chrono::system_clock::time_point startedAt{
		std::chrono::system_clock::now()};
	std::chrono::milliseconds duration{0};
};

enum class SpeechRecognitionError {
	None,
	Unavailable,
	InvalidAudio,
	SegmentTooLong,
	ConsentRequired,
	AuthenticationRequired,
	TransportFailure,
};

struct SpeechRecognitionResult {
	std::string transcript;
	std::string language;
	double confidence{0.0};
	SpeechRecognitionError error{SpeechRecognitionError::None};
	std::string errorMessage;

	[[nodiscard]] bool succeeded() const noexcept
	{
		return error == SpeechRecognitionError::None;
	}
};

class SpeechEngine {
public:
	virtual ~SpeechEngine() = default;
	[[nodiscard]] virtual SpeechRecognitionResult
	recognize(const AudioSegment &segment,
		  std::string_view languageHint) = 0;
};

class LocalSpeechEngine final : public SpeechEngine {
public:
	using Transcriber = std::function<SpeechRecognitionResult(
		const AudioSegment &, std::string_view)>;

	explicit LocalSpeechEngine(Transcriber transcriber,
				   std::chrono::milliseconds maximumSegment =
					   std::chrono::seconds(10));

	[[nodiscard]] SpeechRecognitionResult
	recognize(const AudioSegment &segment,
		  std::string_view languageHint) override;

private:
	Transcriber transcriber_;
	std::chrono::milliseconds maximumSegment_;
};

class CloudSpeechTransport {
public:
	virtual ~CloudSpeechTransport() = default;
	[[nodiscard]] virtual SpeechRecognitionResult
	transcribe(const AudioSegment &segment, std::string_view languageHint,
		   std::string_view shortLivedToken) = 0;
};

class CloudSpeechEngine final : public SpeechEngine {
public:
	using TokenProvider = std::function<std::string()>;

	CloudSpeechEngine(std::shared_ptr<CloudSpeechTransport> transport,
			  TokenProvider tokenProvider, bool explicitConsent,
			  std::chrono::milliseconds maximumSegment =
				  std::chrono::seconds(8));

	void setExplicitConsent(bool consent) noexcept;
	[[nodiscard]] SpeechRecognitionResult
	recognize(const AudioSegment &segment,
		  std::string_view languageHint) override;

private:
	std::shared_ptr<CloudSpeechTransport> transport_;
	TokenProvider tokenProvider_;
	bool explicitConsent_{false};
	std::chrono::milliseconds maximumSegment_;
};

} // namespace clipcoach
