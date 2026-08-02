#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/core/ai-assistant.hpp>
#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/core/session-metadata.hpp>
#include <clipcoach/storage/repository-result.hpp>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach::storage {

struct ClipLibrarySnapshot {
	std::vector<ClipMetadata> clips;
	std::optional<SessionMetadata> session;
};

class ClipLibraryService final {
public:
	using StatusCallback = std::function<void(StorageStatus)>;
	using ClipsCallback = std::function<void(RepositoryResult<std::vector<ClipMetadata>>)>;
	using SnapshotCallback = std::function<void(RepositoryResult<ClipLibrarySnapshot>)>;
	using LogCallback = std::function<void(bool error, const std::string &message)>;

	explicit ClipLibraryService(std::filesystem::path databasePath, LogCallback logCallback = {});
	~ClipLibraryService();

	ClipLibraryService(const ClipLibraryService &) = delete;
	ClipLibraryService &operator=(const ClipLibraryService &) = delete;

	void storeSession(SessionMetadata session, StatusCallback callback = {});
	void endSession(std::string sessionId, std::chrono::system_clock::time_point endedAt,
			StatusCallback callback = {});
	void storeClip(ClipMetadata clip, StatusCallback callback = {});
	void updateThumbnail(std::string clipId, std::filesystem::path thumbnailPath, StatusCallback callback = {});
	void storeAiResult(AiPersistedClipResult result, StatusCallback callback = {});
	void storeSessionAiSummary(std::string sessionId, std::string summary, AiLanguage language,
				   StatusCallback callback = {});
	void setFavorite(std::string clipId, bool favorite, StatusCallback callback = {});
	void deleteClips(std::vector<std::string> clipIds, StatusCallback callback = {});
	void storeExportJob(ExportJob job, StatusCallback callback = {});
	void listRecentAsync(int limit, ClipsCallback callback);
	void listBySessionAsync(std::string sessionId, ClipsCallback callback);
	void loadSnapshotAsync(std::string sessionId, int recentLimit, SnapshotCallback callback);

	// Intended for orderly plugin shutdown and deterministic tests, never UI actions.
	[[nodiscard]] StorageStatus flush();
	[[nodiscard]] const std::filesystem::path &databasePath() const noexcept;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace clipcoach::storage
