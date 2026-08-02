#include "test-support.hpp"

#include <clipcoach/core/voice-audio-segmenter.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

int main()
{
	using clipcoach::VoiceAudioSegmenter;
	using clipcoach::VoiceAudioSegmenterConfig;
	using clipcoach::test::expect;

	VoiceAudioSegmenterConfig config;
	config.sampleRateHz = 1000;
	config.speechThreshold = 500;
	config.minimumSpeechMs = 100;
	config.trailingSilenceMs = 100;
	config.maximumSegmentMs = 1000;
	VoiceAudioSegmenter segmenter(config);

	std::vector<std::int16_t> silence(250, 0);
	expect(segmenter.push(silence.data(), silence.size()).empty(), "silence must not produce recognition work");

	std::vector<std::int16_t> sparseNoise(100, 0);
	for (std::size_t index = 0; index < sparseNoise.size(); index += 10)
		sparseNoise[index] = 1200;
	expect(segmenter.push(sparseNoise.data(), sparseNoise.size()).empty(),
	       "isolated sample spikes must not open a speech segment");

	std::vector<std::int16_t> speech(250, 4000);
	expect(segmenter.push(speech.data(), speech.size()).empty(), "active speech must remain buffered");
	auto completed = segmenter.push(silence.data(), 100);
	expect(completed.size() == 1, "trailing silence must close one utterance");
	expect(completed.front().sampleRateHz == 1000 && completed.front().duration.count() == 350,
	       "segment metadata must describe captured OBS audio");
	expect(std::count(completed.front().pcmMono.begin(), completed.front().pcmMono.end(), 4000) == 250,
	       "segment must preserve speech samples");

	std::vector<std::int16_t> shortNoise(50, 4000);
	(void)segmenter.push(shortNoise.data(), shortNoise.size());
	expect(segmenter.push(silence.data(), 100).empty(),
	       "short noise below the minimum speech duration must be discarded");

	std::vector<std::int16_t> longSpeech(1100, 5000);
	completed = segmenter.push(longSpeech.data(), longSpeech.size());
	expect(completed.size() == 1 && completed.front().duration.count() == 1000,
	       "maximum duration must bound recognition latency and memory");

	VoiceAudioSegmenter responsive;
	std::vector<std::int16_t> normalPhrase(6400, 2000);
	std::vector<std::int16_t> shortPause(1280, 0);
	expect(responsive.push(normalPhrase.data(), normalPhrase.size()).empty(),
	       "a phrase must remain open while speech is active");
	expect(responsive.push(shortPause.data(), shortPause.size()).size() == 1,
	       "the low-latency detector must close after 80 ms of silence");

	VoiceAudioSegmenter continuousSpeech;
	std::vector<std::int16_t> maximumWindow(19200, 2500);
	completed = continuousSpeech.push(maximumWindow.data(), maximumWindow.size());
	expect(completed.size() == 1 && completed.front().duration.count() == 1200,
	       "continuous microphone input must reach recognition within 1.2 seconds");

	return clipcoach::test::pass("voice-audio-segmenter-test");
}
