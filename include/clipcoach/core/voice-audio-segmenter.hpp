#pragma once

#include <clipcoach/core/speech-engine.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace clipcoach {

struct VoiceAudioSegmenterConfig {
	int sampleRateHz{16000};
	int speechThreshold{700};
	int minimumSpeechMs{60};
	int trailingSilenceMs{80};
	int maximumSegmentMs{1200};
};

class VoiceAudioSegmenter final {
public:
	explicit VoiceAudioSegmenter(VoiceAudioSegmenterConfig config = {});

	[[nodiscard]] std::vector<AudioSegment> push(const std::int16_t *samples,
						     std::size_t count);
	void reset() noexcept;
	void setSpeechThreshold(int threshold) noexcept;

private:
	[[nodiscard]] AudioSegment finish();

	VoiceAudioSegmenterConfig config_;
	std::vector<std::int16_t> activeSamples_;
	std::size_t speechSamples_{0};
	std::size_t trailingSilentSamples_{0};
	bool active_{false};
};

} // namespace clipcoach
