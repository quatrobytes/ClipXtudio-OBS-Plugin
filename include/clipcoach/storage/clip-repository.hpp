#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/core/ai-assistant.hpp>
#include <clipcoach/storage/repository-result.hpp>

#include <optional>
#include <string>
#include <vector>

namespace clipcoach::storage {

class SqliteDatabase;

class ClipRepository final {
public:
	explicit ClipRepository(SqliteDatabase &database);

	[[nodiscard]] StorageStatus insert(const ClipMetadata &clip);
	[[nodiscard]] RepositoryResult<std::optional<ClipMetadata>> findById(const std::string &id) const;
	[[nodiscard]] RepositoryResult<std::vector<ClipMetadata>> listBySession(const std::string &sessionId) const;
	[[nodiscard]] RepositoryResult<std::vector<ClipMetadata>> listFavorites() const;
	[[nodiscard]] RepositoryResult<std::vector<ClipMetadata>> listByOrientation(ClipOrientation orientation) const;
	[[nodiscard]] RepositoryResult<std::vector<ClipMetadata>> listRecent(int limit = 200) const;
	[[nodiscard]] StorageStatus remove(const std::string &id);
	[[nodiscard]] StorageStatus setFavorite(const std::string &id, bool favorite);
	[[nodiscard]] StorageStatus updateScore(const std::string &id, int score);
	[[nodiscard]] StorageStatus markExported(const std::string &id);
	[[nodiscard]] StorageStatus setExportStatus(const std::string &id, ExportStatus status);
	[[nodiscard]] StorageStatus updateThumbnail(
		const std::string &id, const std::filesystem::path &thumbnailPath);
	[[nodiscard]] StorageStatus updateAiResult(const AiPersistedClipResult &result);

private:
	SqliteDatabase &database_;
};

} // namespace clipcoach::storage
