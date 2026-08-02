#include <clipcoach/ui/desktop-clip-action-service.hpp>

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>

#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QUrl>

#include <algorithm>

namespace clipcoach::ui {
namespace {

ClipActionResult openPath(const std::filesystem::path &path)
{
	const auto value = QString::fromStdString(path.u8string());
	if (value.isEmpty() || !QFileInfo::exists(value)) {
		return ClipActionResult::fail("the requested file does not exist");
	}
	return QDesktopServices::openUrl(QUrl::fromLocalFile(value))
		       ? ClipActionResult::ok()
		       : ClipActionResult::fail("the operating system rejected the action");
}

} // namespace

DesktopClipActionService::DesktopClipActionService(
	ExportManager *exportManager, SettingsManager *settingsManager,
	VerticalCanvasManager *verticalManager)
	: exportManager_(exportManager),
	  settingsManager_(settingsManager),
	  verticalManager_(verticalManager)
{
}

ClipActionResult
DesktopClipActionService::preview(const std::filesystem::path &clipPath)
{
	return openPath(clipPath);
}

ClipActionResult
DesktopClipActionService::openFolder(const std::filesystem::path &clipPath)
{
	if (clipPath.empty()) {
		return ClipActionResult::fail("the clip path is empty");
	}
	return openPath(clipPath.parent_path());
}

ClipActionResult
DesktopClipActionService::openTextAsset(const std::filesystem::path &assetPath)
{
	return openPath(assetPath);
}

ClipActionResult
DesktopClipActionService::presentCaption(const std::string &caption)
{
	if (caption.empty()) {
		return ClipActionResult::fail("caption is unavailable");
	}
	QMessageBox::information(nullptr, QStringLiteral("ClipXtudio"),
				 QString::fromStdString(caption));
	return ClipActionResult::ok();
}

ClipActionResult
DesktopClipActionService::requestExport(const ClipMetadata &clip,
					ExportOrientation orientation)
{
	if (exportManager_ == nullptr) {
		return ClipActionResult::fail("export service is unavailable");
	}
	std::string error;
	const auto ids = exportManager_->enqueue(
		makeRequest(clip, orientation), &error);
	return !ids.empty() ? ClipActionResult::ok()
			    : ClipActionResult::fail(
				      error.empty() ? "could not queue export"
						    : std::move(error));
}

ClipActionResult DesktopClipActionService::requestBatchExport(
	const std::vector<ClipMetadata> &clips, ExportOrientation orientation)
{
	if (exportManager_ == nullptr || clips.empty()) {
		return ClipActionResult::fail(
			clips.empty() ? "select at least one clip"
				      : "export service is unavailable");
	}
	std::vector<ExportRequest> requests;
	requests.reserve(clips.size());
	for (const auto &clip : clips) {
		requests.push_back(makeRequest(clip, orientation));
	}
	std::string error;
	const auto ids =
		exportManager_->enqueueBatch(std::move(requests), &error);
	return !ids.empty() ? ClipActionResult::ok()
			    : ClipActionResult::fail(
				      error.empty() ? "could not queue batch export"
						    : std::move(error));
}

ExportRequest DesktopClipActionService::makeRequest(
	const ClipMetadata &clip, ExportOrientation orientation) const
{
	ExportRequest request;
	request.clipId = clip.id;
	request.sourcePath = clip.filePath;
	request.outputBaseName =
		clip.title.empty() ? clip.filePath.stem().string() : clip.title;
	request.durationSeconds = std::max(clip.durationSeconds, 1);
	request.orientation = orientation;
	if (settingsManager_ != nullptr) {
		const auto &settings = settingsManager_->settings();
		request.outputDirectory =
			settings.exportDirectory.empty()
				? clip.filePath.parent_path() / "ClipXtudio Exports"
				: settings.exportDirectory;
		request.verticalWidth = settings.verticalWidth;
		request.verticalHeight = settings.verticalHeight;
		switch (settings.exportQuality) {
		case ExportQuality::Low:
			request.preset = ExportQualityPreset::Low;
			break;
		case ExportQuality::Medium:
			request.preset = ExportQualityPreset::Medium;
			break;
		case ExportQuality::High:
			request.preset = ExportQualityPreset::High;
			break;
		case ExportQuality::Maximum:
			request.preset = ExportQualityPreset::Maximum;
			break;
		}
	} else {
		request.outputDirectory =
			clip.filePath.parent_path() / "ClipXtudio Exports";
	}
	// A future libobs canvas renderer supplies verticalCanvasPath here.
	request.verticalSource = VerticalExportSource::CenterCrop;
	(void)verticalManager_;
	return request;
}

} // namespace clipcoach::ui
