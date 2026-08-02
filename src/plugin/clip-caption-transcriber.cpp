#include <clipcoach/plugin/clip-caption-transcriber.hpp>

#include <QCoreApplication>
#include <QMetaObject>
#include <QProcess>
#include <QStringList>

#include <whisper.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace clipcoach::plugin {
namespace {

std::string trim(std::string value)
{
	const auto first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return {};
	const auto last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
}

void deliver(ClipCaptionTranscriber::Completion completion,
	     ClipTranscriptionResult result)
{
	if (!completion)
		return;
	if (auto *application = QCoreApplication::instance()) {
		QMetaObject::invokeMethod(
			application,
			[completion = std::move(completion),
			 result = std::move(result)]() mutable {
				completion(std::move(result));
			},
			Qt::QueuedConnection);
		return;
	}
	completion(std::move(result));
}

void deliverProgress(const ClipCaptionTranscriber::Progress &progress,
		     int percentage)
{
	if (!progress)
		return;
	percentage = std::clamp(percentage, 0, 100);
	if (auto *application = QCoreApplication::instance()) {
		QMetaObject::invokeMethod(
			application,
			[progress, percentage] { progress(percentage); },
			Qt::QueuedConnection);
		return;
	}
	progress(percentage);
}

} // namespace

class ClipCaptionTranscriber::Impl final {
public:
	Impl(std::filesystem::path ffmpegExecutable,
	     std::filesystem::path whisperModel)
		: ffmpegExecutable_(std::move(ffmpegExecutable)),
		  whisperModel_(std::move(whisperModel)),
		  worker_([this] { run(); })
	{
	}

	~Impl()
	{
		stopping_.store(true);
		condition_.notify_all();
		if (worker_.joinable())
			worker_.join();
		if (context_ != nullptr)
			whisper_free(context_);
	}

	void enqueue(std::filesystem::path mediaPath, std::string language,
		     Progress progress, Completion completion)
	{
		{
			std::lock_guard lock(mutex_);
			jobs_.push_back(
				{std::move(mediaPath), std::move(language),
					 std::move(progress), std::move(completion)});
		}
		condition_.notify_one();
	}

private:
	struct Job {
		std::filesystem::path mediaPath;
		std::string language;
		Progress progress;
		Completion completion;
	};

	void run()
	{
		while (!stopping_.load()) {
			Job job;
			{
				std::unique_lock lock(mutex_);
				condition_.wait(lock, [this] {
					return stopping_.load() || !jobs_.empty();
				});
				if (stopping_.load())
					break;
				job = std::move(jobs_.front());
				jobs_.pop_front();
			}
			deliver(std::move(job.completion),
				transcribe(job.mediaPath, job.language,
					   job.progress));
		}
	}

	ClipTranscriptionResult transcribe(
		const std::filesystem::path &mediaPath,
		const std::string &language, const Progress &progress)
	{
		deliverProgress(progress, 2);
		std::error_code error;
		if (!std::filesystem::is_regular_file(mediaPath, error))
			return {false, {}, "The clip file is not available"};
		if (!std::filesystem::is_regular_file(ffmpegExecutable_, error))
			return {false, {}, "The bundled FFmpeg tool is not available"};
		if (!ensureModelLoaded())
			return {false, {}, modelError_};
		deliverProgress(progress, 8);

		QProcess ffmpeg;
		ffmpeg.setProgram(
			QString::fromStdString(ffmpegExecutable_.u8string()));
		ffmpeg.setArguments(
			{QStringLiteral("-hide_banner"),
			 QStringLiteral("-loglevel"), QStringLiteral("error"),
			 QStringLiteral("-i"),
			 QString::fromStdString(mediaPath.u8string()),
			 QStringLiteral("-vn"), QStringLiteral("-ac"),
			 QStringLiteral("1"), QStringLiteral("-ar"),
			 QStringLiteral("16000"), QStringLiteral("-f"),
			 QStringLiteral("s16le"), QStringLiteral("pipe:1")});
		ffmpeg.start(QIODevice::ReadOnly);
		if (!ffmpeg.waitForStarted(10'000))
			return {false, {}, "FFmpeg could not start"};
		if (!ffmpeg.waitForFinished(120'000)) {
			ffmpeg.kill();
			ffmpeg.waitForFinished(3'000);
			return {false, {}, "Audio transcription timed out"};
		}
		const QByteArray pcm = ffmpeg.readAllStandardOutput();
		if (ffmpeg.exitStatus() != QProcess::NormalExit ||
		    ffmpeg.exitCode() != 0 || pcm.size() < 2) {
			const auto details =
				QString::fromUtf8(ffmpeg.readAllStandardError())
					.trimmed();
			return {false, {},
				details.isEmpty()
					? "The clip has no readable audio"
					: details.toStdString()};
		}
		deliverProgress(progress, 20);

		const auto sampleCount = pcm.size() / 2;
		std::vector<float> samples;
		samples.reserve(static_cast<std::size_t>(sampleCount));
		const auto *bytes =
			reinterpret_cast<const unsigned char *>(pcm.constData());
		for (qsizetype index = 0; index + 1 < pcm.size();
		     index += 2) {
			const auto value = static_cast<std::int16_t>(
				static_cast<std::uint16_t>(bytes[index]) |
				(static_cast<std::uint16_t>(bytes[index + 1])
				 << 8));
			samples.push_back(static_cast<float>(value) /
					  32768.0F);
		}

		auto parameters =
			whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
		parameters.n_threads = static_cast<int>(std::min(
			4u, std::max(1u, std::thread::hardware_concurrency())));
		parameters.translate = false;
		parameters.no_context = true;
		parameters.no_timestamps = true;
		parameters.single_segment = false;
		parameters.print_special = false;
		parameters.print_progress = false;
		parameters.print_realtime = false;
		parameters.print_timestamps = false;
		parameters.suppress_blank = true;
		parameters.suppress_nst = true;
		parameters.temperature = 0.0F;
		parameters.progress_callback = [](whisper_context *, whisper_state *,
						 int value, void *data) {
			const auto *callback = static_cast<const Progress *>(data);
			if (callback != nullptr)
				deliverProgress(*callback, 20 + ((value * 78) / 100));
		};
		parameters.progress_callback_user_data =
			const_cast<Progress *>(&progress);
		const auto transcriptionDeadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
		parameters.abort_callback = [](void *data) {
			const auto *deadline = static_cast<const std::chrono::steady_clock::time_point *>(data);
			return std::chrono::steady_clock::now() >= *deadline;
		};
		parameters.abort_callback_user_data = const_cast<std::chrono::steady_clock::time_point *>(
			&transcriptionDeadline);
		const std::string normalizedLanguage =
			language == "es" || language == "en" ? language : "auto";
		parameters.language = normalizedLanguage.c_str();
		parameters.detect_language = normalizedLanguage == "auto";

		if (whisper_full(context_, parameters, samples.data(),
				 static_cast<int>(samples.size())) != 0) {
			if (std::chrono::steady_clock::now() >= transcriptionDeadline)
				return {false, {}, "La transcripción local superó el límite de 3 minutos"};
			return {false, {}, "Local speech recognition could not analyze this clip"};
		}

		std::string transcript;
		const int segmentCount = whisper_full_n_segments(context_);
		for (int index = 0; index < segmentCount; ++index) {
			if (const char *segment =
				    whisper_full_get_segment_text(context_, index))
				transcript += segment;
		}
		transcript = trim(std::move(transcript));
		if (transcript.empty())
			return {false, {},
				"No spoken words were detected in this clip"};
		deliverProgress(progress, 100);
		return {true, std::move(transcript), {}};
	}

	bool ensureModelLoaded()
	{
		if (context_ != nullptr)
			return true;
		std::error_code error;
		if (!std::filesystem::is_regular_file(whisperModel_, error)) {
			modelError_ =
				"The bundled multilingual speech model is unavailable";
			return false;
		}
		auto parameters = whisper_context_default_params();
		parameters.use_gpu = false;
		parameters.flash_attn = false;
		const auto modelPath = whisperModel_.u8string();
		context_ = whisper_init_from_file_with_params(
			modelPath.c_str(), parameters);
		if (context_ == nullptr) {
			modelError_ =
				"The bundled multilingual speech model could not be loaded";
			return false;
		}
		return true;
	}

	std::filesystem::path ffmpegExecutable_;
	std::filesystem::path whisperModel_;
	whisper_context *context_{nullptr};
	std::string modelError_;
	std::atomic<bool> stopping_{false};
	std::mutex mutex_;
	std::condition_variable condition_;
	std::deque<Job> jobs_;
	std::thread worker_;
};

ClipCaptionTranscriber::ClipCaptionTranscriber(
	std::filesystem::path ffmpegExecutable,
	std::filesystem::path whisperModel)
	: impl_(std::make_unique<Impl>(std::move(ffmpegExecutable),
				      std::move(whisperModel)))
{
}

ClipCaptionTranscriber::~ClipCaptionTranscriber() = default;

void ClipCaptionTranscriber::transcribe(std::filesystem::path mediaPath,
					std::string language,
					Progress progress,
					Completion completion)
{
	impl_->enqueue(std::move(mediaPath), std::move(language),
		       std::move(progress), std::move(completion));
}

} // namespace clipcoach::plugin
