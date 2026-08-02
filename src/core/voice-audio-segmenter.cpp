#include <clipcoach/core/voice-audio-segmenter.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>

namespace clipcoach {

VoiceAudioSegmenter::VoiceAudioSegmenter(VoiceAudioSegmenterConfig config) : config_(config)
{
	if (config_.sampleRateHz <= 0)
		config_.sampleRateHz = 16000;
	if (config_.speechThreshold < 1)
		config_.speechThreshold = 1;
}

std::vector<AudioSegment> VoiceAudioSegmenter::push(const std::int16_t *samples, std::size_t count)
{
	std::vector<AudioSegment> completed;
	if (samples == nullptr || count == 0)
		return completed;

	const auto minimumSpeechSamples = static_cast<std::size_t>(config_.sampleRateHz) *
					  static_cast<std::size_t>(config_.minimumSpeechMs) / 1000;
	const auto trailingSilenceSamples = static_cast<std::size_t>(config_.sampleRateHz) *
					    static_cast<std::size_t>(config_.trailingSilenceMs) / 1000;
	const auto maximumSegmentSamples = static_cast<std::size_t>(config_.sampleRateHz) *
					   static_cast<std::size_t>(config_.maximumSegmentMs) / 1000;

	long double energy = 0.0;
	for (std::size_t index = 0; index < count; ++index) {
		const auto value = static_cast<long double>(samples[index]);
		energy += value * value;
	}
	const auto rms = std::sqrt(energy / static_cast<long double>(count));
	const bool speech = rms >= static_cast<long double>(config_.speechThreshold);
	if (!active_ && !speech)
		return completed;
	if (!active_) {
		active_ = true;
		activeSamples_.clear();
		speechSamples_ = 0;
		trailingSilentSamples_ = 0;
	}

	const auto remainingCapacity =
		maximumSegmentSamples > activeSamples_.size() ? maximumSegmentSamples - activeSamples_.size() : 0;
	const auto acceptedCount = std::min(count, remainingCapacity);
	activeSamples_.insert(activeSamples_.end(), samples, samples + acceptedCount);
	if (speech) {
		speechSamples_ += acceptedCount;
		trailingSilentSamples_ = 0;
	} else {
		trailingSilentSamples_ += acceptedCount;
	}

	const bool maximumReached = activeSamples_.size() >= maximumSegmentSamples;
	const bool utteranceEnded = speechSamples_ >= minimumSpeechSamples &&
				    trailingSilentSamples_ >= trailingSilenceSamples;
	if (maximumReached || utteranceEnded) {
		if (speechSamples_ >= minimumSpeechSamples)
			completed.push_back(finish());
		else
			reset();
	}

	return completed;
}

void VoiceAudioSegmenter::reset() noexcept
{
	activeSamples_.clear();
	speechSamples_ = 0;
	trailingSilentSamples_ = 0;
	active_ = false;
}

void VoiceAudioSegmenter::setSpeechThreshold(int threshold) noexcept
{
	config_.speechThreshold = std::max(1, threshold);
}

AudioSegment VoiceAudioSegmenter::finish()
{
	AudioSegment segment;
	segment.pcmMono = std::move(activeSamples_);
	segment.sampleRateHz = config_.sampleRateHz;
	segment.duration = std::chrono::milliseconds(segment.pcmMono.size() * 1000 /
						     static_cast<std::size_t>(config_.sampleRateHz));
	segment.startedAt = std::chrono::system_clock::now() - segment.duration;
	reset();
	return segment;
}

} // namespace clipcoach
