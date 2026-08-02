#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/core/session-metadata.hpp>

#include <chrono>
#include <filesystem>
#include <string>

namespace clipcoach::test {

class TemporaryDatabase final {
public:
	TemporaryDatabase()
	{
		const auto uniqueValue = std::chrono::steady_clock::now().time_since_epoch().count();
		directory_ = std::filesystem::temp_directory_path() /
			     ("clipcoach-storage-test-" + std::to_string(uniqueValue));
		std::filesystem::create_directories(directory_);
		path_ = directory_ / "clipcoach.db";
	}

	~TemporaryDatabase()
	{
		std::error_code error;
		std::filesystem::remove_all(directory_, error);
	}

	TemporaryDatabase(const TemporaryDatabase &) = delete;
	TemporaryDatabase &operator=(const TemporaryDatabase &) = delete;

	[[nodiscard]] const std::filesystem::path &path() const noexcept { return path_; }

private:
	std::filesystem::path directory_;
	std::filesystem::path path_;
};

inline SessionMetadata sampleSession()
{
	SessionMetadata session;
	session.id = "session-test-001";
	session.startedAt = std::chrono::system_clock::from_time_t(1785247200);
	session.profileName = "Streaming";
	session.sceneCollection = "Gaming";
	session.appVersion = "0.2.0";
	return session;
}

inline ClipMetadata sampleClip(const std::string &id = "clip-test-001")
{
	ClipMetadata clip;
	clip.id = id;
	clip.sessionId = "session-test-001";
	clip.filePath = std::filesystem::path("/tmp/clips") / (id + ".mkv");
	clip.fileName = id + ".mkv";
	clip.createdAt = std::chrono::system_clock::from_time_t(1785247509);
	clip.durationSeconds = 30;
	clip.orientation = ClipOrientation::Horizontal;
	clip.triggerType = TriggerType::Voice;
	clip.triggerLabel = "saca clip";
	clip.score = 42;
	clip.title = "First clutch";
	clip.caption = "A test caption";
	clip.transcriptPath = "/tmp/clips/transcript.txt";
	clip.subtitlePath = "/tmp/clips/subtitles.srt";
	clip.thumbnailPath = "/tmp/clips/thumb.jpg";
	clip.isFavorite = false;
	clip.exportStatus = ExportStatus::Pending;
	clip.sourceScene = "Gameplay";
	clip.appVersion = "0.2.0";
	return clip;
}

} // namespace clipcoach::test
