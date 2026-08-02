#include <clipcoach/plugin/windows-voice-trigger-controller.hpp>

#include <clipcoach/core/trigger-engine.hpp>
#include <clipcoach/core/voice-audio-segmenter.hpp>
#include <clipcoach/core/voice-trigger-service.hpp>

#include <QMetaObject>

#include <media-io/audio-resampler.h>
#include <obs-module.h>
#include <util/platform.h>
#include <whisper.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cmath>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace clipcoach::plugin {
namespace {

constexpr int kWhisperSampleRate = 16000;
// Voice commands are time-sensitive. Keeping old utterances queued behind an
// active inference makes a command appear to react several seconds late.
constexpr std::size_t kMaximumQueuedSegments = 1;

std::vector<std::string> splitPhrases(const std::string &csv)
{
	std::vector<std::string> phrases;
	std::stringstream stream(csv);
	std::string phrase;
	while (std::getline(stream, phrase, ',')) {
		const auto first = phrase.find_first_not_of(" \t");
		const auto last = phrase.find_last_not_of(" \t");
		if (first != std::string::npos)
			phrases.push_back(phrase.substr(first, last - first + 1));
	}
	return phrases;
}

std::string joinPhrases(const std::vector<std::string> &phrases)
{
	std::string prompt;
	for (const auto &phrase : phrases) {
		if (!prompt.empty())
			prompt += ". ";
		prompt += phrase;
	}
	return prompt;
}

bool sameVoiceSettings(const Settings &left, const Settings &right)
{
	return left.triggerVoiceEnabled == right.triggerVoiceEnabled &&
	       left.triggerAudioSpikeEnabled == right.triggerAudioSpikeEnabled &&
	       left.triggerKeywordEnabled == right.triggerKeywordEnabled &&
	       left.triggerSensitivity == right.triggerSensitivity && left.preRollSeconds == right.preRollSeconds &&
	       left.postRollSeconds == right.postRollSeconds && left.triggerAction == right.triggerAction &&
	       left.triggerCooldownSeconds == right.triggerCooldownSeconds &&
	       left.voiceTriggerPhrasesCsv == right.voiceTriggerPhrasesCsv &&
	       left.triggerKeywordsCsv == right.triggerKeywordsCsv &&
	       left.voiceAudioSourceName == right.voiceAudioSourceName &&
	       left.voiceTriggerLanguage == right.voiceTriggerLanguage && left.voiceSpeechMode == right.voiceSpeechMode;
}

std::string trim(std::string value)
{
	const auto first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return {};
	const auto last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
}

class WhisperSpeechEngine final : public SpeechEngine {
public:
	~WhisperSpeechEngine() override
	{
		if (context_ != nullptr)
			whisper_free(context_);
	}

	bool load(const std::string &modelPath, std::string &error)
	{
		if (context_ != nullptr)
			return true;
		auto params = whisper_context_default_params();
		params.use_gpu = false;
		params.flash_attn = false;
		context_ = whisper_init_from_file_with_params(modelPath.c_str(), params);
		if (context_ == nullptr) {
			error = "The bundled multilingual speech model could not be loaded";
			return false;
		}
		loaded_.store(true);
		return true;
	}

	void setPrompt(std::string prompt) { prompt_ = std::move(prompt); }
	[[nodiscard]] bool loaded() const noexcept { return loaded_.load(); }
	[[nodiscard]] const std::string &lastTranscript() const noexcept { return lastTranscript_; }

	SpeechRecognitionResult recognize(const AudioSegment &segment, std::string_view languageHint) override
	{
		if (context_ == nullptr)
			return {{},
				{},
				0.0,
				SpeechRecognitionError::Unavailable,
				"The local speech model is not loaded"};
		if (segment.sampleRateHz != kWhisperSampleRate || segment.pcmMono.empty())
			return {{},
				{},
				0.0,
				SpeechRecognitionError::InvalidAudio,
				"OBS audio was not converted to 16 kHz mono"};

		std::vector<float> samples;
		samples.reserve(segment.pcmMono.size());
		for (const auto sample : segment.pcmMono)
			samples.push_back(static_cast<float>(sample) / 32768.0F);

		auto params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
		const auto hardwareThreads = std::max(1u, std::thread::hardware_concurrency());
		// Four workers keep the tiny local model responsive without monopolizing
		// the creator's CPU while OBS is encoding.
		params.n_threads = static_cast<int>(std::min(4u, hardwareThreads));
		params.translate = false;
		params.no_context = true;
		params.no_timestamps = true;
		params.single_segment = true;
		// Voice Trigger only recognizes short configured commands. A compact
		// encoder context avoids processing a full transcription-sized window.
		params.audio_ctx = 256;
		params.print_special = false;
		params.print_progress = false;
		params.print_realtime = false;
		params.print_timestamps = false;
		params.suppress_blank = true;
		params.suppress_nst = true;
		params.temperature = 0.0F;
		params.greedy.best_of = 1;
		params.initial_prompt = prompt_.empty() ? nullptr : prompt_.c_str();
		const std::string language(languageHint);
		params.language = language.empty() || language == "auto" ? "auto" : language.c_str();
		params.detect_language = language.empty() || language == "auto";

		if (whisper_full(context_, params, samples.data(), static_cast<int>(samples.size())) != 0)
			return {{},
				{},
				0.0,
				SpeechRecognitionError::Unavailable,
				"Local multilingual recognition failed"};

		std::string transcript;
		double probabilityTotal = 0.0;
		int probabilityCount = 0;
		const int segments = whisper_full_n_segments(context_);
		for (int segmentIndex = 0; segmentIndex < segments; ++segmentIndex) {
			if (const char *text = whisper_full_get_segment_text(context_, segmentIndex))
				transcript += text;
			const int tokens = whisper_full_n_tokens(context_, segmentIndex);
			for (int token = 0; token < tokens; ++token) {
				const float probability = whisper_full_get_token_p(context_, segmentIndex, token);
				if (std::isfinite(probability) && probability > 0.0F) {
					probabilityTotal += probability;
					++probabilityCount;
				}
			}
		}
		lastTranscript_ = trim(std::move(transcript));
		const double confidence = probabilityCount > 0 ? probabilityTotal / probabilityCount : 0.0;
		return {lastTranscript_,
			language.empty() ? "auto" : language,
			confidence,
			SpeechRecognitionError::None,
			{}};
	}

private:
	whisper_context *context_{nullptr};
	std::atomic<bool> loaded_{false};
	std::string prompt_;
	std::string lastTranscript_;
};

struct SourceCollector {
	std::vector<std::string> names;
};

bool collectAudioSource(void *data, obs_source_t *source)
{
	auto &collector = *static_cast<SourceCollector *>(data);
	if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) == 0)
		return true;
	const char *name = obs_source_get_name(source);
	if (name != nullptr && *name != '\0')
		collector.names.emplace_back(name);
	return true;
}

std::vector<std::string> enumerateAudioSources()
{
	SourceCollector collector;
	obs_enum_sources(collectAudioSource, &collector);
	std::sort(collector.names.begin(), collector.names.end());
	collector.names.erase(std::unique(collector.names.begin(), collector.names.end()), collector.names.end());
	return collector.names;
}

bool likelyMicrophone(obs_source_t *source)
{
	const char *id = obs_source_get_id(source);
	const char *name = obs_source_get_name(source);
	std::string value = std::string(id ? id : "") + " " + std::string(name ? name : "");
	std::transform(value.begin(), value.end(), value.begin(),
		       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value.find("input_capture") != std::string::npos || value.find("mic") != std::string::npos ||
	       value.find("micro") != std::string::npos;
}

obs_source_t *resolveAudioSource(const std::string &configuredName)
{
	if (!configuredName.empty()) {
		auto *source = obs_get_source_by_name(configuredName.c_str());
		if (source != nullptr && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0)
			return source;
		if (source != nullptr)
			obs_source_release(source);
		return nullptr;
	}

	// OBS reserves output channels 3-6 for Mic/Aux devices. Prefer those
	// global sources because they are the microphones selected in OBS Audio
	// settings and remain active independently of the current scene.
	for (uint32_t channel = 3; channel <= 6; ++channel) {
		auto *source = obs_get_output_source(channel);
		if (source == nullptr)
			continue;
		if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0)
			return source;
		obs_source_release(source);
	}

	struct Candidate {
		obs_source_t *activeMicrophone{nullptr};
		obs_source_t *microphone{nullptr};
		obs_source_t *activeAudio{nullptr};
		obs_source_t *audio{nullptr};
	} candidate;
	obs_enum_sources(
		[](void *data, obs_source_t *source) {
			auto &candidate = *static_cast<Candidate *>(data);
			if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) == 0)
				return true;
			const bool active = obs_source_active(source);
			const bool microphone = likelyMicrophone(source);
			if (candidate.audio == nullptr)
				candidate.audio = obs_source_get_ref(source);
			if (active && candidate.activeAudio == nullptr)
				candidate.activeAudio = obs_source_get_ref(source);
			if (microphone && candidate.microphone == nullptr)
				candidate.microphone = obs_source_get_ref(source);
			if (active && microphone && candidate.activeMicrophone == nullptr)
				candidate.activeMicrophone = obs_source_get_ref(source);
			return true;
		},
		&candidate);
	auto *selected = candidate.activeMicrophone != nullptr ? candidate.activeMicrophone
			 : candidate.microphone != nullptr     ? candidate.microphone
			 : candidate.activeAudio != nullptr    ? candidate.activeAudio
							       : candidate.audio;
	for (auto *source : {candidate.activeMicrophone, candidate.microphone, candidate.activeAudio, candidate.audio})
		if (source != nullptr && source != selected)
			obs_source_release(source);
	return selected;
}

std::string bundledModelPath()
{
	char *path = obs_module_file("models/ggml-tiny-q5_1.bin");
	if (path == nullptr)
		return {};
	std::string result(path);
	bfree(path);
	return result;
}

} // namespace

class WindowsVoiceTriggerController::Impl {
public:
	Impl(WindowsVoiceTriggerController &owner, TriggerEngine &engine)
		: owner_(owner),
		  engine_(engine),
		  service_(speech_, engine, false)
	{
	}

	~Impl() { stop(); }

	void apply(const Settings &settings)
	{
		if (initialized_ && sameVoiceSettings(settings_, settings)) {
			const auto currentStatus = status();
			if (currentStatus.state != VoiceRuntimeState::SourceUnavailable &&
			    currentStatus.state != VoiceRuntimeState::WaitingForAudio)
				return;
		}
		settings_ = settings;
		initialized_ = true;
		reconfigure();
	}

	void setPro(bool unlocked)
	{
		if (initialized_ && proUnlocked_ == unlocked)
			return;
		proUnlocked_ = unlocked;
		service_.setProUnlocked(unlocked);
		reconfigure();
	}

	void setCallback(StatusCallback callback)
	{
		std::scoped_lock lock(statusMutex_);
		callback_ = std::move(callback);
	}

	VoiceRuntimeStatus status() const
	{
		std::scoped_lock lock(statusMutex_);
		return status_;
	}

	std::vector<std::string> availableSources() const { return enumerateAudioSources(); }

	void shutdown()
	{
		setCallback({});
		stop();
	}

private:
	void reconfigure()
	{
		stop();
		VoiceTriggerConfiguration config;
		config.enabled = settings_.triggerVoiceEnabled;
		config.sensitivity = settings_.triggerSensitivity;
		config.preRollSeconds = settings_.preRollSeconds;
		config.postRollSeconds = settings_.postRollSeconds;
		config.action = settings_.triggerAction;
		config.cooldown = std::chrono::seconds(settings_.triggerCooldownSeconds);
		config.language = settings_.voiceTriggerLanguage;
		config.phrases = splitPhrases(settings_.voiceTriggerPhrasesCsv);
		std::string error;
		if (!service_.configure(config, &error)) {
			publish(VoiceRuntimeState::Error, error);
			return;
		}
		if (!proUnlocked_ || (!settings_.triggerVoiceEnabled && !settings_.triggerAudioSpikeEnabled &&
				      !settings_.triggerKeywordEnabled)) {
			publish(VoiceRuntimeState::Disabled, "OBS audio triggers are disabled");
			return;
		}
		if ((settings_.triggerVoiceEnabled || settings_.triggerKeywordEnabled) &&
		    settings_.voiceSpeechMode != SpeechProcessingMode::Local) {
			publish(VoiceRuntimeState::Error,
				"Cloud recognition is not configured; select local OBS audio");
			return;
		}

		source_ = resolveAudioSource(settings_.voiceAudioSourceName);
		if (source_ == nullptr) {
			publish(VoiceRuntimeState::SourceUnavailable,
				settings_.voiceAudioSourceName.empty()
					? "OBS has no audio input source"
					: "The selected OBS audio source no longer exists");
			return;
		}
		sourceName_ = obs_source_get_name(source_);

		obs_audio_info audioInfo{};
		if (!obs_get_audio_info(&audioInfo)) {
			publish(VoiceRuntimeState::Error, "OBS audio configuration is unavailable");
			releaseSource();
			return;
		}
		const resample_info sourceInfo{audioInfo.samples_per_sec, AUDIO_FORMAT_FLOAT_PLANAR,
					       audioInfo.speakers};
		const resample_info destinationInfo{kWhisperSampleRate, AUDIO_FORMAT_16BIT, SPEAKERS_MONO};
		resampler_ = audio_resampler_create(&destinationInfo, &sourceInfo);
		if (resampler_ == nullptr) {
			publish(VoiceRuntimeState::Error, "OBS audio could not be converted for local speech");
			releaseSource();
			return;
		}

		segmenter_.setSpeechThreshold(std::max(120, 1100 - settings_.triggerSensitivity * 10));
		speech_.setPrompt(joinPhrases(config.phrases));
		running_.store(true);
		audioReceiving_.store(false);
		if (settings_.triggerVoiceEnabled || settings_.triggerKeywordEnabled) {
			publish(VoiceRuntimeState::Starting,
				"Loading bundled multilingual model and attaching to OBS audio");
			worker_ = std::thread([this] { run(); });
		} else {
			publish(VoiceRuntimeState::Listening, "Monitoring the selected OBS source for audio spikes");
		}
		obs_source_add_audio_capture_callback(source_, audioCaptured, this);
	}

	void stop()
	{
		if (source_ != nullptr)
			obs_source_remove_audio_capture_callback(source_, audioCaptured, this);
		running_.store(false);
		queueCondition_.notify_all();
		if (worker_.joinable())
			worker_.join();
		{
			std::scoped_lock lock(queueMutex_);
			queue_.clear();
		}
		segmenter_.reset();
		if (resampler_ != nullptr) {
			audio_resampler_destroy(resampler_);
			resampler_ = nullptr;
		}
		releaseSource();
		audioReceiving_.store(false);
	}

	void releaseSource()
	{
		if (source_ != nullptr) {
			obs_source_release(source_);
			source_ = nullptr;
		}
		sourceName_.clear();
	}

	static void audioCaptured(void *data, obs_source_t *, const audio_data *audio, bool muted)
	{
		static_cast<Impl *>(data)->onAudio(audio, muted);
	}

	void onAudio(const audio_data *audio, bool)
	{
		// Voice commands intentionally keep listening to the configured input
		// when it is muted in the OBS program mix. Muting the stream must not
		// silently disable the explicitly enabled voice trigger.
		if (!running_.load() || audio == nullptr || audio->frames == 0 || resampler_ == nullptr)
			return;
		uint8_t *output[MAX_AV_PLANES]{};
		uint32_t outputFrames = 0;
		uint64_t timestampOffset = 0;
		const uint8_t *input[MAX_AV_PLANES]{};
		for (std::size_t plane = 0; plane < MAX_AV_PLANES; ++plane)
			input[plane] = audio->data[plane];
		if (!audio_resampler_resample(resampler_, output, &outputFrames, &timestampOffset, input,
					      audio->frames) ||
		    outputFrames == 0 || output[0] == nullptr)
			return;

		if (!audioReceiving_.exchange(true) && speech_.loaded())
			publish(VoiceRuntimeState::Listening, "Listening to the selected OBS audio source");
		const auto *samples = reinterpret_cast<const std::int16_t *>(output[0]);
		if (settings_.triggerAudioSpikeEnabled) {
			const auto now = std::chrono::steady_clock::now();
			if (now - lastAudioSpikeSignal_ >= std::chrono::milliseconds(250)) {
				long double squareSum = 0.0;
				for (std::size_t index = 0; index < outputFrames; ++index) {
					const auto sample = static_cast<long double>(samples[index]);
					squareSum += sample * sample;
				}
				const auto rms = std::sqrt(static_cast<double>(squareSum / outputFrames));
				TriggerSignal signal;
				signal.type = SmartTriggerType::AudioSpike;
				signal.audioIntensity = std::clamp(rms / 6000.0, 0.0, 1.0);
				signal.durationSeconds = std::max(settings_constraints::kMinClipDurationSeconds,
								  settings_.preRollSeconds + settings_.postRollSeconds);
				(void)engine_.process(signal);
				lastAudioSpikeSignal_ = now;
			}
		}
		if (!settings_.triggerVoiceEnabled && !settings_.triggerKeywordEnabled)
			return;
		auto segments = segmenter_.push(samples, outputFrames);
		if (segments.empty())
			return;
		std::scoped_lock lock(queueMutex_);
		for (auto &segment : segments) {
			while (queue_.size() >= kMaximumQueuedSegments)
				queue_.pop_front();
			blog(LOG_INFO, "[ClipXtudio] Voice utterance queued duration_ms=%lld",
			     static_cast<long long>(segment.duration.count()));
			queue_.push_back(std::move(segment));
		}
		queueCondition_.notify_one();
	}

	void run()
	{
		std::string error;
		if (!speech_.load(bundledModelPath(), error)) {
			publish(VoiceRuntimeState::ModelUnavailable, error);
			running_.store(false);
			return;
		}
		publish(audioReceiving_.load() ? VoiceRuntimeState::Listening : VoiceRuntimeState::WaitingForAudio,
			audioReceiving_.load() ? "Listening to the selected OBS audio source"
					       : "Model ready; waiting for audio from the selected OBS source");

		while (running_.load()) {
			AudioSegment segment;
			{
				std::unique_lock lock(queueMutex_);
				queueCondition_.wait(lock, [this] { return !running_.load() || !queue_.empty(); });
				if (!running_.load())
					break;
				segment = std::move(queue_.front());
				queue_.pop_front();
			}
			VoiceTriggerResult result;
			if (settings_.triggerVoiceEnabled)
				result = service_.process(segment);
			else {
				const auto recognition = speech_.recognize(segment, settings_.voiceTriggerLanguage);
				result.status = recognition.succeeded() ? VoiceTriggerStatus::NoPhraseMatch
									: VoiceTriggerStatus::RecognitionFailed;
				result.message = recognition.errorMessage;
			}
			const auto transcript = speech_.lastTranscript();
			if (settings_.triggerKeywordEnabled && !transcript.empty()) {
				TriggerSignal keywordSignal;
				keywordSignal.type = SmartTriggerType::Keyword;
				keywordSignal.text = transcript;
				keywordSignal.keywordStrength = 1.0;
				keywordSignal.durationSeconds =
					std::max(settings_constraints::kMinClipDurationSeconds,
						 settings_.preRollSeconds + settings_.postRollSeconds);
				(void)engine_.process(keywordSignal);
			}
			if (result.status == VoiceTriggerStatus::EventCreated)
				blog(LOG_INFO, "[ClipXtudio] Voice command matched; trigger event created");
			else if (result.status == VoiceTriggerStatus::LowConfidence)
				blog(LOG_DEBUG, "[ClipXtudio] Speech ignored because confidence was below threshold");
			else if (result.status == VoiceTriggerStatus::NoPhraseMatch)
				blog(LOG_DEBUG, "[ClipXtudio] Speech recognized without a configured command match");
			if (!transcript.empty())
				publish(VoiceRuntimeState::Listening,
					result.status == VoiceTriggerStatus::EventCreated
						? "Voice command matched and trigger event created"
						: "Speech recognized; no eligible command fired",
					transcript);
			if (result.status == VoiceTriggerStatus::RecognitionFailed)
				publish(VoiceRuntimeState::Error, result.message);
		}
	}

	void publish(VoiceRuntimeState state, std::string message, std::string transcript = {})
	{
		StatusCallback callback;
		VoiceRuntimeStatus snapshot;
		{
			std::scoped_lock lock(statusMutex_);
			status_.state = state;
			status_.modelLoaded = speech_.loaded();
			status_.audioReceiving = audioReceiving_.load();
			status_.sourceName = sourceName_;
			status_.message = std::move(message);
			if (!transcript.empty())
				status_.lastTranscript = std::move(transcript);
			snapshot = status_;
			callback = callback_;
		}
		blog(LOG_INFO, "[ClipXtudio] Voice runtime state=%d model=%s audio=%s source=%s",
		     static_cast<int>(snapshot.state), snapshot.modelLoaded ? "ready" : "loading",
		     snapshot.audioReceiving ? "receiving" : "waiting",
		     snapshot.sourceName.empty() ? "(none)" : snapshot.sourceName.c_str());
		if (callback)
			QMetaObject::invokeMethod(
				&owner_,
				[callback = std::move(callback), snapshot = std::move(snapshot)] {
					callback(snapshot);
				},
				Qt::QueuedConnection);
	}

	WindowsVoiceTriggerController &owner_;
	TriggerEngine &engine_;
	WhisperSpeechEngine speech_;
	VoiceTriggerService service_;
	VoiceAudioSegmenter segmenter_;
	Settings settings_;
	bool initialized_{false};
	bool proUnlocked_{false};

	obs_source_t *source_{nullptr};
	audio_resampler_t *resampler_{nullptr};
	std::string sourceName_;
	std::atomic<bool> running_{false};
	std::atomic<bool> audioReceiving_{false};
	std::chrono::steady_clock::time_point lastAudioSpikeSignal_{};
	std::thread worker_;
	std::mutex queueMutex_;
	std::condition_variable queueCondition_;
	std::deque<AudioSegment> queue_;

	mutable std::mutex statusMutex_;
	VoiceRuntimeStatus status_;
	StatusCallback callback_;
};

WindowsVoiceTriggerController::WindowsVoiceTriggerController(TriggerEngine &engine, QObject *parent)
	: QObject(parent),
	  impl_(std::make_unique<Impl>(*this, engine))
{
}

WindowsVoiceTriggerController::~WindowsVoiceTriggerController() = default;

void WindowsVoiceTriggerController::applySettings(const Settings &settings)
{
	impl_->apply(settings);
}

void WindowsVoiceTriggerController::setProUnlocked(bool unlocked)
{
	impl_->setPro(unlocked);
}

void WindowsVoiceTriggerController::shutdown() noexcept
{
	try {
		impl_->shutdown();
	} catch (...) {
		// OBS is already leaving the frontend lifecycle. Cleanup must never
		// propagate an exception across its C callback boundary.
	}
}

VoiceRuntimeStatus WindowsVoiceTriggerController::status() const
{
	return impl_->status();
}

void WindowsVoiceTriggerController::setStatusCallback(StatusCallback callback)
{
	impl_->setCallback(std::move(callback));
}

std::vector<std::string> WindowsVoiceTriggerController::availableAudioSources() const
{
	return impl_->availableSources();
}

} // namespace clipcoach::plugin
