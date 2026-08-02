#include <clipcoach/plugin/remote-capture-coordinator.hpp>

#include <clipcoach/core/settings-manager.hpp>

#include <QTimer>

#include <algorithm>

namespace clipcoach::plugin {
namespace {

std::string captureErrorCode(CaptureError error)
{
	switch (error) {
	case CaptureError::ReplayBufferInactive: return "REPLAY_BUFFER_INACTIVE";
	case CaptureError::ReplayBufferWarmingUp: return "REPLAY_BUFFER_WARMING_UP";
	case CaptureError::CaptureAlreadyPending: return "CAPTURE_BUSY";
	case CaptureError::InvalidDuration: return "INVALID_DURATION";
	case CaptureError::ProRequired: return "PRO_REQUIRED";
	case CaptureError::SavedFileUnavailable: return "SAVED_FILE_UNAVAILABLE";
	case CaptureError::FileRenameFailed: return "FILE_RENAME_FAILED";
	case CaptureError::SaveRejected: return "SAVE_REJECTED";
	case CaptureError::None: return "CAPTURE_FAILED";
	}
	return "CAPTURE_FAILED";
}

ExportQualityPreset qualityPreset(ExportQuality quality)
{
	switch (quality) {
	case ExportQuality::Low: return ExportQualityPreset::Low;
	case ExportQuality::Medium: return ExportQualityPreset::Medium;
	case ExportQuality::Maximum: return ExportQualityPreset::Maximum;
	case ExportQuality::High: return ExportQualityPreset::High;
	}
	return ExportQualityPreset::High;
}

} // namespace

RemoteCaptureCoordinator::RemoteCaptureCoordinator(ClipManager &clips, ExportManager &exports,
						   SettingsManager &settings, ReplayCapacityCallback ensureCapacity)
	: clips_(clips), exports_(exports), settings_(settings), ensureCapacity_(std::move(ensureCapacity)),
	  exportTimer_(std::make_unique<QTimer>())
{
	exportTimer_->setInterval(250);
	savedObserver_ = clips_.addClipSavedObserver([this](const ClipMetadata &clip) { clipSaved(clip); });
	errorObserver_ = clips_.addErrorObserver(
		[this](CaptureError error, const std::string &message) { captureError(error, message); });
	QObject::connect(exportTimer_.get(), &QTimer::timeout, [this] { inspectExports(); });
}

RemoteCaptureCoordinator::~RemoteCaptureCoordinator()
{
	cancel();
	clips_.removeClipSavedObserver(savedObserver_);
	clips_.removeErrorObserver(errorObserver_);
}

void RemoteCaptureCoordinator::capture(const remote::RemoteCapturePlan &plan, Completion completion)
{
	if (plan_) {
		completion({plan.commandUuid, false, {}, {}, 0, {}, {}, "CAPTURE_BUSY",
			    "Another remote capture is still processing"});
		return;
	}
	plan_ = plan;
	completion_ = std::move(completion);
	if (ensureCapacity_) ensureCapacity_(plan.replayWindowSeconds);
	const auto result = clips_.captureTriggered(plan.replayWindowSeconds, TriggerType::Manual,
						    "remote_clipper", 0, plan.requestedBy);
	if (!result.accepted) {
		complete({plan.commandUuid, false, {}, {}, 0, {}, {}, captureErrorCode(result.error), result.message});
	}
}

void RemoteCaptureCoordinator::cancel()
{
	if (exportTimer_) exportTimer_->stop();
	plan_.reset();
	completion_ = {};
	exportJobIds_.clear();
}

void RemoteCaptureCoordinator::clipSaved(const ClipMetadata &clip)
{
	if (!plan_) return;
	std::string error;
	exportJobIds_ = exports_.enqueue(requestFor(clip), &error);
	if (exportJobIds_.empty()) {
		complete({plan_->commandUuid, false, {}, {}, 0, {}, {}, "EXPORT_REJECTED",
			  error.empty() ? "The compensated clip could not be queued" : error});
		return;
	}
	exportTimer_->start();
}

void RemoteCaptureCoordinator::captureError(CaptureError error, const std::string &message)
{
	if (!plan_) return;
	complete({plan_->commandUuid, false, {}, {}, 0, {}, {}, captureErrorCode(error), message});
}

void RemoteCaptureCoordinator::inspectExports()
{
	if (!plan_ || exportJobIds_.empty()) return;
	bool completeAll = true;
	std::vector<ExportJob> jobs;
	for (const auto &id : exportJobIds_) {
		auto job = exports_.job(id);
		if (job.state == ExportJobState::Error || job.state == ExportJobState::Cancelled) {
			complete({plan_->commandUuid, false, {}, {}, 0, {}, {}, "EXPORT_FAILED",
				  job.error.empty() ? "Remote clip processing failed" : job.error});
			return;
		}
		completeAll = completeAll && job.state == ExportJobState::Done;
		jobs.push_back(std::move(job));
	}
	if (!completeAll) return;
	std::string files;
	for (const auto &job : jobs) {
		if (!files.empty()) files += ", ";
		files += job.outputPath.filename().string();
	}
	const auto orientation = plan_->both ? "both" : plan_->vertical ? "vertical" : "horizontal";
	complete({plan_->commandUuid, true, jobs.front().id, files, plan_->durationSeconds, orientation,
		  "Remote clip saved with delay compensation"});
}

void RemoteCaptureCoordinator::complete(remote::RemoteCommandResult result)
{
	if (exportTimer_) exportTimer_->stop();
	auto completion = std::move(completion_);
	plan_.reset();
	exportJobIds_.clear();
	if (completion) completion(std::move(result));
}

ExportRequest RemoteCaptureCoordinator::requestFor(const ClipMetadata &clip) const
{
	ExportRequest request;
	request.clipId = clip.id + "-remote";
	request.sourcePath = clip.filePath;
	request.outputBaseName = clip.filePath.stem().string() + "_remote";
	request.durationSeconds = plan_->durationSeconds;
	request.endOffsetSeconds = plan_->delayCompensationSeconds;
	request.orientation = plan_->both ? ExportOrientation::Both
			     : plan_->vertical ? ExportOrientation::Vertical
				       : ExportOrientation::Horizontal;
	const auto &settings = settings_.settings();
	request.outputDirectory = settings.exportDirectory.empty()
		? clip.filePath.parent_path() / "ClipXtudio Remote"
		: settings.exportDirectory / "Remote Clipper";
	request.verticalWidth = settings.verticalWidth;
	request.verticalHeight = settings.verticalHeight;
	request.preset = qualityPreset(settings.exportQuality);
	request.verticalSource = VerticalExportSource::CenterCrop;
	return request;
}

} // namespace clipcoach::plugin
