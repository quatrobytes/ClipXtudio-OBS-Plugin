#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace clipcoach {

enum class ClipOrientation {
	Horizontal,
	Vertical,
	Both,
};

enum class TriggerType {
	Manual,
	Voice,
	AudioSpike,
	Chat,
	Scene,
	Ai,
};

enum class ExportStatus {
	Pending,
	Exporting,
	Exported,
	Failed,
};

struct ClipMetadata {
	std::string id;
	std::string sessionId;
	std::filesystem::path filePath;
	std::string fileName;
	std::chrono::system_clock::time_point createdAt;
	int durationSeconds{0};
	ClipOrientation orientation{ClipOrientation::Horizontal};
	TriggerType triggerType{TriggerType::Manual};
	std::string triggerLabel;
	std::string requestedBy;
	int score{0};
	std::string title;
	std::string caption;
	std::vector<std::string> suggestedTitles;
	std::vector<std::string> hashtags;
	std::string aiSummary;
	std::string aiLanguage;
	std::filesystem::path transcriptPath;
	std::filesystem::path subtitlePath;
	std::filesystem::path thumbnailPath;
	bool isFavorite{false};
	ExportStatus exportStatus{ExportStatus::Pending};
	std::string sourceScene;
	std::string appVersion;
};

} // namespace clipcoach
