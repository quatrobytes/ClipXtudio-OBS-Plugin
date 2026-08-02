#include <clipcoach/export/ffmpeg-export-backend.hpp>

#include <QProcess>
#include <QStringList>

#include <algorithm>
#include <chrono>
#include <string>

namespace clipcoach::exporting {
namespace {

struct EncodingPreset {
	const char *speed;
	const char *crf;
	const char *audioBitrate;
};

EncodingPreset preset(ExportQualityPreset quality)
{
	switch (quality) {
	case ExportQualityPreset::Low:
		return {"ultrafast", "30", "96k"};
	case ExportQualityPreset::Medium:
		return {"veryfast", "26", "128k"};
	case ExportQualityPreset::High:
		return {"medium", "21", "160k"};
	case ExportQualityPreset::Maximum:
		return {"slow", "17", "192k"};
	}
	return {"medium", "21", "160k"};
}

QString cropFilter(int width, int height)
{
	return QStringLiteral(
		       "scale=%1:%2:force_original_aspect_ratio=increase,"
		       "crop=%1:%2,setsar=1")
		.arg(width)
		.arg(height);
}

QString canvasFilter(int width, int height)
{
	return QStringLiteral(
		       "scale=%1:%2:force_original_aspect_ratio=decrease,"
		       "pad=%1:%2:(ow-iw)/2:(oh-ih)/2,setsar=1")
		.arg(width)
		.arg(height);
}

std::string readableError(const QByteArray &stderrData)
{
	auto message = QString::fromUtf8(stderrData).trimmed();
	if (message.size() > 2000) {
		message = message.right(2000);
	}
	return message.isEmpty() ? "FFmpeg exited without an error message"
				 : message.toStdString();
}

void consumeProgress(QByteArray &buffer, int durationSeconds,
		     const ExportBackend::ProgressCallback &callback)
{
	while (true) {
		const auto newline = buffer.indexOf('\n');
		if (newline < 0) {
			break;
		}
		const auto line = buffer.left(newline).trimmed();
		buffer.remove(0, newline + 1);
		if (line.startsWith("out_time_us=")) {
			bool valid = false;
			const auto microseconds =
				line.mid(sizeof("out_time_us=") - 1).toLongLong(
					&valid);
			if (valid && durationSeconds > 0) {
				const auto percent = static_cast<int>(
					(microseconds * 100) /
					(static_cast<long long>(
						 durationSeconds) *
					 1000000LL));
				callback(std::clamp(percent, 0, 99));
			}
		} else if (line == "progress=end") {
			callback(99);
		}
	}
}

} // namespace

FfmpegExportBackend::FfmpegExportBackend(std::filesystem::path executable)
	: executable_(std::move(executable))
{
}

ExportBackendResult FfmpegExportBackend::execute(
	const ExportJob &job, const std::filesystem::path &temporaryPath,
	ProgressCallback progress, const std::atomic_bool &cancelRequested)
{
	const auto encoding = preset(job.request.preset);
	const auto source =
		job.orientation == ExportOrientation::Vertical &&
				job.request.verticalSource ==
					VerticalExportSource::VerticalCanvas
			? job.request.verticalCanvasPath
			: job.request.sourcePath;
	std::error_code fileError;
	if (executable_.empty() ||
	    !std::filesystem::is_regular_file(executable_, fileError)) {
		return ExportBackendResult::fail(
			"ClipXtudio media engine is missing; reinstall ClipXtudio to restore it");
	}
	fileError.clear();
	if (!std::filesystem::is_regular_file(source, fileError)) {
		return ExportBackendResult::fail(
			"export source does not exist or is not a regular file");
	}

	QStringList arguments{
		QStringLiteral("-hide_banner"),
		QStringLiteral("-nostdin"),
		QStringLiteral("-n"),
		QStringLiteral("-loglevel"),
		QStringLiteral("error"),
		QStringLiteral("-stats_period"),
		QStringLiteral("0.25"),
		QStringLiteral("-progress"),
		QStringLiteral("pipe:1"),
	};
	const bool segmented = !job.request.keepSegments.empty();
	if (segmented) {
		arguments << QStringLiteral("-i")
			  << QString::fromStdString(source.u8string());
	} else if (job.request.trimStartMilliseconds >= 0) {
		arguments << QStringLiteral("-i")
			  << QString::fromStdString(source.u8string())
			  << QStringLiteral("-ss")
			  << QString::number(job.request.trimStartMilliseconds / 1000.0, 'f', 3)
			  << QStringLiteral("-t")
			  << QString::number(job.request.trimDurationMilliseconds / 1000.0, 'f', 3);
	} else {
		arguments << QStringLiteral("-sseof")
			  << QStringLiteral("-%1").arg(job.request.durationSeconds + job.request.endOffsetSeconds)
			  << QStringLiteral("-i")
			  << QString::fromStdString(source.u8string())
			  << QStringLiteral("-t")
			  << QString::number(job.request.durationSeconds);
	}
	if (segmented) {
		QStringList filters;
		QString concatInputs;
		for (qsizetype index = 0;
		     index < static_cast<qsizetype>(job.request.keepSegments.size()); ++index) {
			const auto &segment = job.request.keepSegments[static_cast<std::size_t>(index)];
			const auto start = QString::number(segment.startMilliseconds / 1000.0, 'f', 3);
			const auto duration = QString::number(segment.durationMilliseconds / 1000.0, 'f', 3);
			filters << QStringLiteral("[0:v:0]trim=start=%1:duration=%2,setpts=PTS-STARTPTS[v%3]")
				.arg(start, duration).arg(index);
			filters << QStringLiteral("[0:a:0]atrim=start=%1:duration=%2,asetpts=PTS-STARTPTS[a%3]")
				.arg(start, duration).arg(index);
			concatInputs += QStringLiteral("[v%1][a%1]").arg(index);
		}
		filters << QStringLiteral("%1concat=n=%2:v=1:a=1[vout][aout]")
			.arg(concatInputs).arg(static_cast<qulonglong>(job.request.keepSegments.size()));
		arguments << QStringLiteral("-filter_complex") << filters.join(QLatin1Char(';'))
			  << QStringLiteral("-map") << QStringLiteral("[vout]")
			  << QStringLiteral("-map") << QStringLiteral("[aout]");
	} else {
		arguments << QStringLiteral("-map") << QStringLiteral("0:v:0")
			  << QStringLiteral("-map") << QStringLiteral("0:a?");
	}
	if (job.orientation == ExportOrientation::Vertical &&
	    !job.request.preserveSourceFrame) {
		arguments << QStringLiteral("-vf")
			  << (job.request.verticalSource ==
					      VerticalExportSource::VerticalCanvas
				      ? canvasFilter(job.request.verticalWidth,
						     job.request.verticalHeight)
				      : cropFilter(job.request.verticalWidth,
						   job.request.verticalHeight));
	}
	if (job.request.outputFps > 0) {
		arguments << QStringLiteral("-r")
			  << QString::number(job.request.outputFps);
	}
	arguments << QStringLiteral("-c:v") << QStringLiteral("libx264")
		  << QStringLiteral("-preset") << QString::fromLatin1(encoding.speed)
		  << QStringLiteral("-crf") << QString::fromLatin1(encoding.crf)
		  << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p")
		  << QStringLiteral("-c:a") << QStringLiteral("aac")
		  << QStringLiteral("-b:a")
		  << QString::fromLatin1(encoding.audioBitrate)
		  << QStringLiteral("-movflags") << QStringLiteral("+faststart")
		  << QString::fromStdString(temporaryPath.u8string());

	QProcess process;
	process.setProcessChannelMode(QProcess::SeparateChannels);
	process.start(QString::fromStdString(executable_.u8string()), arguments,
		      QIODevice::ReadOnly);
	if (!process.waitForStarted(5000)) {
		return ExportBackendResult::fail(
			"ClipXtudio media engine could not start; reinstall ClipXtudio or contact support");
	}

	QByteArray progressBuffer;
	QByteArray errorBuffer;
	while (process.state() != QProcess::NotRunning) {
		if (cancelRequested.load()) {
			process.terminate();
			if (!process.waitForFinished(1000)) {
				process.kill();
				process.waitForFinished(1000);
			}
			return ExportBackendResult::cancelledResult();
		}
		process.waitForReadyRead(100);
		progressBuffer += process.readAllStandardOutput();
		errorBuffer += process.readAllStandardError();
		consumeProgress(progressBuffer, job.request.durationSeconds,
				progress);
		if (errorBuffer.size() > 8192) {
			errorBuffer = errorBuffer.right(8192);
		}
	}
	progressBuffer += process.readAllStandardOutput();
	errorBuffer += process.readAllStandardError();
	consumeProgress(progressBuffer, job.request.durationSeconds, progress);
	if (process.exitStatus() != QProcess::NormalExit ||
	    process.exitCode() != 0) {
		return ExportBackendResult::fail(readableError(errorBuffer));
	}
	return ExportBackendResult::ok();
}

} // namespace clipcoach::exporting
