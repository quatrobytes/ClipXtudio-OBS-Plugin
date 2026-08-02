#include "../unit/test-support.hpp"

#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/export/ffmpeg-export-backend.hpp>

#include <QProcess>

#include <chrono>
#include <filesystem>

int main()
{
	using namespace clipcoach;
	using clipcoach::test::expect;
	const auto directory = std::filesystem::temp_directory_path() /
			       ("clipcoach-ffmpeg-" +
				std::to_string(std::chrono::steady_clock::now()
						       .time_since_epoch()
						       .count()));
	std::filesystem::create_directories(directory);
	const auto source = directory / "source.mp4";

	QProcess fixture;
	fixture.start(
		QStringLiteral(FFMPEG_EXECUTABLE),
		{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
		 QStringLiteral("error"), QStringLiteral("-f"),
		 QStringLiteral("lavfi"), QStringLiteral("-i"),
		 QStringLiteral("testsrc2=size=640x360:rate=30:duration=4"),
		 QStringLiteral("-f"), QStringLiteral("lavfi"),
		 QStringLiteral("-i"),
		 QStringLiteral("sine=frequency=1000:duration=4"),
		 QStringLiteral("-c:v"), QStringLiteral("libx264"),
		 QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"),
		 QStringLiteral("-c:a"), QStringLiteral("aac"),
		 QStringLiteral("-shortest"),
		 QString::fromStdString(source.u8string())});
	expect(fixture.waitForFinished(30000) && fixture.exitCode() == 0,
	       "FFmpeg media fixture must be generated");

	{
		ExportManager missingRuntime(
			std::make_unique<exporting::FfmpegExportBackend>(
				directory / "missing-ffmpeg.exe"));
		ExportRequest missingRequest;
		missingRequest.clipId = "missing-runtime";
		missingRequest.sourcePath = source;
		missingRequest.outputDirectory = directory / "missing-runtime";
		missingRequest.outputBaseName = "missing";
		missingRequest.durationSeconds = 2;
		const auto missingIds = missingRuntime.enqueue(missingRequest);
		expect(missingRuntime.waitUntilIdle(std::chrono::seconds(5)),
		       "missing bundled runtime must fail without hanging");
		const auto missingJob = missingRuntime.job(missingIds.front());
		expect(missingJob.state == ExportJobState::Error &&
			       missingJob.error.find("reinstall ClipXtudio") !=
				       std::string::npos,
		       "missing bundled runtime must return an actionable error");
	}

	ExportManager manager(
		std::make_unique<exporting::FfmpegExportBackend>(
			std::filesystem::path(FFMPEG_EXECUTABLE)));
	ExportRequest request;
	request.clipId = "fixture";
	request.sourcePath = source;
	request.outputDirectory = directory / "exports";
	request.outputBaseName = "Vertical smoke";
	request.orientation = ExportOrientation::Both;
	request.preset = ExportQualityPreset::Low;
	request.durationSeconds = 1;
	request.verticalWidth = 720;
	request.verticalHeight = 1280;
	const auto ids = manager.enqueue(request);
	expect(ids.size() == 2,
	       "Both fixture must enqueue horizontal and vertical outputs");
	expect(manager.waitUntilIdle(std::chrono::seconds(30)),
	       "vertical FFmpeg export must finish");
	const auto horizontal = manager.job(ids.front());
	const auto job = manager.job(ids.back());
	expect(horizontal.state == ExportJobState::Done &&
		       std::filesystem::is_regular_file(horizontal.outputPath) &&
		       job.state == ExportJobState::Done &&
		       std::filesystem::is_regular_file(job.outputPath),
	       "horizontal and vertical FFmpeg exports must produce MP4 files");

	QProcess probe;
	probe.start(
		QStringLiteral(FFPROBE_EXECUTABLE),
		{QStringLiteral("-v"), QStringLiteral("error"),
		 QStringLiteral("-select_streams"), QStringLiteral("v:0"),
		 QStringLiteral("-show_entries"),
		 QStringLiteral("stream=width,height"),
		 QStringLiteral("-of"),
		 QStringLiteral("csv=p=0:s=x"),
		 QString::fromStdString(job.outputPath.u8string())});
	expect(probe.waitForFinished(10000) && probe.exitCode() == 0 &&
		       probe.readAllStandardOutput().trimmed() == "720x1280",
	       "exported video must be exactly 720x1280");
	QProcess durationProbe;
	durationProbe.start(
		QStringLiteral(FFPROBE_EXECUTABLE),
		{QStringLiteral("-v"), QStringLiteral("error"),
		 QStringLiteral("-show_entries"),
		 QStringLiteral("format=duration"), QStringLiteral("-of"),
		 QStringLiteral("default=noprint_wrappers=1:nokey=1"),
		 QString::fromStdString(job.outputPath.u8string())});
	expect(durationProbe.waitForFinished(10000) &&
		       durationProbe.exitCode() == 0,
	       "ffprobe must read the exported duration");
	bool durationValid = false;
	const auto duration =
		durationProbe.readAllStandardOutput().trimmed().toDouble(
			&durationValid);
	expect(durationValid && duration >= 0.85 && duration <= 1.15,
	       "export must trim the replay tail to the requested duration");

	ExportRequest trimmed;
	trimmed.clipId = "quick-editor-trim";
	trimmed.sourcePath = source;
	trimmed.outputDirectory = directory / "edited";
	trimmed.outputBaseName = "edited selection";
	trimmed.orientation = ExportOrientation::Horizontal;
	trimmed.preset = ExportQualityPreset::High;
	trimmed.durationSeconds = 2;
	trimmed.trimStartMilliseconds = 1250;
	trimmed.trimDurationMilliseconds = 1500;
	trimmed.outputFps = 60;
	trimmed.preserveSourceFrame = true;
	const auto trimmedIds = manager.enqueue(trimmed);
	expect(trimmedIds.size() == 1 && manager.waitUntilIdle(std::chrono::seconds(30)),
	       "quick editor trim must finish without blocking the UI worker");
	const auto trimmedJob = manager.job(trimmedIds.front());
	expect(trimmedJob.state == ExportJobState::Done &&
		       std::filesystem::is_regular_file(trimmedJob.outputPath),
	       "quick editor must create an MP4 for the selected range");
	QProcess trimProbe;
	trimProbe.start(
		QStringLiteral(FFPROBE_EXECUTABLE),
		{QStringLiteral("-v"), QStringLiteral("error"),
		 QStringLiteral("-select_streams"), QStringLiteral("v:0"),
		 QStringLiteral("-show_entries"),
		 QStringLiteral("stream=width,height,avg_frame_rate:format=duration"),
		 QStringLiteral("-of"), QStringLiteral("json"),
		 QString::fromStdString(trimmedJob.outputPath.u8string())});
	expect(trimProbe.waitForFinished(10000) && trimProbe.exitCode() == 0,
	       "ffprobe must inspect the quick editor output");
	const auto trimJson = trimProbe.readAllStandardOutput();
	expect(trimJson.contains("\"width\": 640") &&
		       trimJson.contains("\"height\": 360") &&
		       trimJson.contains("\"avg_frame_rate\": \"60/1\""),
	       "quick editor must preserve source resolution and apply the selected 60 FPS output");

	ExportRequest cutRanges;
	cutRanges.clipId = "quick-editor-multiple-cuts";
	cutRanges.sourcePath = source;
	cutRanges.outputDirectory = directory / "edited";
	cutRanges.outputBaseName = "edited multiple cuts";
	cutRanges.orientation = ExportOrientation::Horizontal;
	cutRanges.preset = ExportQualityPreset::Medium;
	cutRanges.durationSeconds = 2;
	cutRanges.trimDurationMilliseconds = 1700;
	cutRanges.keepSegments = {{250, 800}, {1800, 900}};
	cutRanges.preserveSourceFrame = true;
	const auto cutIds = manager.enqueue(cutRanges);
	expect(cutIds.size() == 1 && manager.waitUntilIdle(std::chrono::seconds(30)),
	       "quick editor multi-range export must finish");
	const auto cutJob = manager.job(cutIds.front());
	expect(cutJob.state == ExportJobState::Done &&
		       std::filesystem::is_regular_file(cutJob.outputPath),
	       "quick editor must concatenate all retained ranges into one MP4");
	QProcess cutProbe;
	cutProbe.start(
		QStringLiteral(FFPROBE_EXECUTABLE),
		{QStringLiteral("-v"), QStringLiteral("error"),
		 QStringLiteral("-show_entries"), QStringLiteral("format=duration"),
		 QStringLiteral("-of"), QStringLiteral("default=noprint_wrappers=1:nokey=1"),
		 QString::fromStdString(cutJob.outputPath.u8string())});
	expect(cutProbe.waitForFinished(10000) && cutProbe.exitCode() == 0,
	       "ffprobe must inspect the multi-range output");
	bool cutDurationValid = false;
	const auto cutDuration = cutProbe.readAllStandardOutput().trimmed().toDouble(
		&cutDurationValid);
	expect(cutDurationValid && cutDuration >= 1.55 && cutDuration <= 1.85,
	       "multi-range output duration must equal the sum of retained ranges");

	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);
	return clipcoach::test::pass("ffmpeg-export-smoke");
}
