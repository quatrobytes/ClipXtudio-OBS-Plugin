#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/core/export-manager.hpp>

#include <filesystem>
#include <string>
#include <utility>

namespace clipcoach {

struct ClipActionResult {
	bool success{false};
	std::string error;

	[[nodiscard]] static ClipActionResult ok() { return {true, {}}; }
	[[nodiscard]] static ClipActionResult fail(std::string message)
	{
		return {false, std::move(message)};
	}
};

class ClipActionService {
public:
	virtual ~ClipActionService() = default;

	[[nodiscard]] virtual ClipActionResult
	preview(const std::filesystem::path &clipPath) = 0;
	[[nodiscard]] virtual ClipActionResult
	openFolder(const std::filesystem::path &clipPath) = 0;
	[[nodiscard]] virtual ClipActionResult
	openTextAsset(const std::filesystem::path &assetPath) = 0;
	[[nodiscard]] virtual ClipActionResult
	presentCaption(const std::string &caption) = 0;
	[[nodiscard]] virtual ClipActionResult
	requestExport(const ClipMetadata &clip, ExportOrientation orientation) = 0;
	[[nodiscard]] virtual ClipActionResult requestBatchExport(
		const std::vector<ClipMetadata> &clips,
		ExportOrientation orientation) = 0;
};

} // namespace clipcoach
