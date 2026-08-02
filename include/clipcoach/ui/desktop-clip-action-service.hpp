#pragma once

#include <clipcoach/core/clip-action-service.hpp>

namespace clipcoach {
class SettingsManager;
class VerticalCanvasManager;
} // namespace clipcoach

namespace clipcoach::ui {

class DesktopClipActionService final : public ClipActionService {
public:
	DesktopClipActionService(ExportManager *exportManager = nullptr, SettingsManager *settingsManager = nullptr,
				 VerticalCanvasManager *verticalManager = nullptr);
	[[nodiscard]] ClipActionResult preview(const std::filesystem::path &clipPath) override;
	[[nodiscard]] ClipActionResult openFolder(const std::filesystem::path &clipPath) override;
	[[nodiscard]] ClipActionResult openTextAsset(const std::filesystem::path &assetPath) override;
	[[nodiscard]] ClipActionResult presentCaption(const std::string &caption) override;
	[[nodiscard]] ClipActionResult requestExport(const ClipMetadata &clip, ExportOrientation orientation) override;
	[[nodiscard]] ClipActionResult requestBatchExport(const std::vector<ClipMetadata> &clips,
							  ExportOrientation orientation) override;

private:
	[[nodiscard]] ExportRequest makeRequest(const ClipMetadata &clip, ExportOrientation orientation) const;

	ExportManager *exportManager_{nullptr};
	SettingsManager *settingsManager_{nullptr};
	VerticalCanvasManager *verticalManager_{nullptr};
};

} // namespace clipcoach::ui
