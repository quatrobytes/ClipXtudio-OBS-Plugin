#pragma once

#include <clipcoach/core/export-manager.hpp>

#include <filesystem>

namespace clipcoach::exporting {

class FfmpegExportBackend final : public ExportBackend {
public:
	explicit FfmpegExportBackend(
		std::filesystem::path executable = std::filesystem::path("ffmpeg"));

	[[nodiscard]] ExportBackendResult
	execute(const ExportJob &job, const std::filesystem::path &temporaryPath,
		ProgressCallback progress,
		const std::atomic_bool &cancelRequested) override;

private:
	std::filesystem::path executable_;
};

} // namespace clipcoach::exporting

