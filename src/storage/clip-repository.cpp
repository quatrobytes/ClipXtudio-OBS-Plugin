#include <clipcoach/storage/clip-repository.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

#include "sqlite-helpers.hpp"

#include <sqlite3.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach::storage {
namespace {

constexpr const char *kSelectColumns = R"SQL(
id, session_id, file_path, file_name, created_at, duration_seconds,
orientation, trigger_type, score, title, caption, transcript_path,
subtitle_path, thumbnail_path, is_favorite, export_status, source_scene,
app_version
,
suggested_titles_blob, hashtags_blob, ai_summary, ai_language,
trigger_label, requested_by
)SQL";

std::string encodeList(const std::vector<std::string> &values)
{
	std::string result;
	for (const auto &value : values)
		result += std::to_string(value.size()) + ":" + value;
	return result;
}

std::optional<std::vector<std::string>> decodeList(const std::string &encoded)
{
	std::vector<std::string> result;
	std::size_t position = 0;
	while (position < encoded.size()) {
		const auto colon = encoded.find(':', position);
		if (colon == std::string::npos || colon == position)
			return std::nullopt;
		std::size_t length = 0;
		try {
			length = std::stoull(encoded.substr(position, colon - position));
		} catch (...) {
			return std::nullopt;
		}
		position = colon + 1;
		if (length > encoded.size() - position)
			return std::nullopt;
		result.push_back(encoded.substr(position, length));
		position += length;
	}
	return result;
}

RepositoryResult<ClipMetadata> readClip(sqlite3_stmt *statement)
{
	ClipMetadata clip;
	clip.id = detail::columnText(statement, 0);
	clip.sessionId = detail::columnText(statement, 1);
	clip.filePath = std::filesystem::u8path(detail::columnText(statement, 2));
	clip.fileName = detail::columnText(statement, 3);
	const auto createdAt = detail::parseTime(detail::columnText(statement, 4));
	const auto orientation = detail::orientationFromString(detail::columnText(statement, 6));
	const auto trigger = detail::triggerFromString(detail::columnText(statement, 7));
	const auto exportStatus = detail::exportStatusFromString(detail::columnText(statement, 15));
	if (!createdAt || !orientation || !trigger || !exportStatus) {
		return RepositoryResult<ClipMetadata>::fail("stored clip contains an invalid enum or timestamp");
	}

	clip.createdAt = *createdAt;
	clip.durationSeconds = sqlite3_column_int(statement, 5);
	clip.orientation = *orientation;
	clip.triggerType = *trigger;
	clip.score = sqlite3_column_int(statement, 8);
	clip.title = detail::columnText(statement, 9);
	clip.caption = detail::columnText(statement, 10);
	clip.transcriptPath = std::filesystem::u8path(detail::columnText(statement, 11));
	clip.subtitlePath = std::filesystem::u8path(detail::columnText(statement, 12));
	clip.thumbnailPath = std::filesystem::u8path(detail::columnText(statement, 13));
	clip.isFavorite = sqlite3_column_int(statement, 14) != 0;
	clip.exportStatus = *exportStatus;
	clip.sourceScene = detail::columnText(statement, 16);
	clip.appVersion = detail::columnText(statement, 17);
	const auto titles = decodeList(detail::columnText(statement, 18));
	const auto hashtags = decodeList(detail::columnText(statement, 19));
	if (!titles || !hashtags)
		return RepositoryResult<ClipMetadata>::fail("stored AI metadata is malformed");
	clip.suggestedTitles = *titles;
	clip.hashtags = *hashtags;
	clip.aiSummary = detail::columnText(statement, 20);
	clip.aiLanguage = detail::columnText(statement, 21);
	clip.triggerLabel = detail::columnText(statement, 22);
	clip.requestedBy = detail::columnText(statement, 23);
	return RepositoryResult<ClipMetadata>::ok(std::move(clip));
}

RepositoryResult<std::vector<ClipMetadata>> readClipList(sqlite3 *database, sqlite3_stmt *statement)
{
	std::vector<ClipMetadata> clips;
	int result = SQLITE_ROW;
	while ((result = sqlite3_step(statement)) == SQLITE_ROW) {
		auto clip = readClip(statement);
		if (!clip.success) {
			return RepositoryResult<std::vector<ClipMetadata>>::fail(clip.error);
		}
		clips.push_back(std::move(clip.value));
	}
	if (result != SQLITE_DONE) {
		return detail::queryFailure<std::vector<ClipMetadata>>(database, "could not list clips");
	}
	return RepositoryResult<std::vector<ClipMetadata>>::ok(std::move(clips));
}

} // namespace

ClipRepository::ClipRepository(SqliteDatabase &database) : database_(database) {}

StorageStatus ClipRepository::insert(const ClipMetadata &clip)
{
	if (!database_.isOpen() || clip.id.empty() || clip.sessionId.empty() || clip.filePath.empty() ||
	    clip.fileName.empty() || clip.durationSeconds < 0 || clip.score < 0 || clip.score > 100) {
		return StorageStatus::fail("cannot insert clip: metadata is invalid");
	}
	const char *sql = R"SQL(
INSERT INTO clips (
  id, session_id, file_path, file_name, created_at, duration_seconds,
  orientation, trigger_type, score, title, caption, transcript_path,
  subtitle_path, thumbnail_path, is_favorite, export_status, source_scene,
  app_version
  , suggested_titles_blob, hashtags_blob, ai_summary, ai_language,
  trigger_label, requested_by
) VALUES (
  ?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14,
  ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24
);
)SQL";
	detail::Statement statement(database_.handle(), sql);
	if (!statement.valid()) {
		return detail::failure(database_.handle(), "could not prepare clip insert");
	}

	auto *query = statement.get();
	const auto path = clip.filePath.u8string();
	const auto transcript = clip.transcriptPath.u8string();
	const auto subtitle = clip.subtitlePath.u8string();
	const auto thumbnail = clip.thumbnailPath.u8string();
	const bool bound =
		detail::bindText(query, 1, clip.id) && detail::bindText(query, 2, clip.sessionId) &&
		detail::bindText(query, 3, path) && detail::bindText(query, 4, clip.fileName) &&
		detail::bindText(query, 5, detail::formatTime(clip.createdAt)) &&
		sqlite3_bind_int(query, 6, clip.durationSeconds) == SQLITE_OK &&
		detail::bindText(query, 7, detail::toString(clip.orientation)) &&
		detail::bindText(query, 8, detail::toString(clip.triggerType)) &&
		sqlite3_bind_int(query, 9, clip.score) == SQLITE_OK && detail::bindText(query, 10, clip.title) &&
		detail::bindText(query, 11, clip.caption) && detail::bindText(query, 12, transcript) &&
		detail::bindText(query, 13, subtitle) && detail::bindText(query, 14, thumbnail) &&
		sqlite3_bind_int(query, 15, clip.isFavorite ? 1 : 0) == SQLITE_OK &&
		detail::bindText(query, 16, detail::toString(clip.exportStatus)) &&
		detail::bindText(query, 17, clip.sourceScene) && detail::bindText(query, 18, clip.appVersion) &&
		detail::bindText(query, 19, encodeList(clip.suggestedTitles)) &&
		detail::bindText(query, 20, encodeList(clip.hashtags)) && detail::bindText(query, 21, clip.aiSummary) &&
		detail::bindText(query, 22, clip.aiLanguage) &&
		detail::bindText(query, 23, clip.triggerLabel) &&
		detail::bindText(query, 24, clip.requestedBy);
	if (!bound || sqlite3_step(query) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not insert clip");
	}
	return StorageStatus::ok();
}

StorageStatus ClipRepository::updateThumbnail(
	const std::string &id, const std::filesystem::path &thumbnailPath)
{
	if (!database_.isOpen() || id.empty() || thumbnailPath.empty())
		return StorageStatus::fail(
			"cannot update thumbnail: clip id and path are required");
	detail::Statement statement(
		database_.handle(),
		"UPDATE clips SET thumbnail_path=?1 WHERE id=?2;");
	if (!statement.valid())
		return detail::failure(database_.handle(),
				       "could not prepare thumbnail update");
	const auto path = thumbnailPath.u8string();
	if (!detail::bindText(statement.get(), 1, path) ||
	    !detail::bindText(statement.get(), 2, id) ||
	    sqlite3_step(statement.get()) != SQLITE_DONE)
		return detail::failure(database_.handle(),
				       "could not update clip thumbnail");
	if (sqlite3_changes(database_.handle()) == 0)
		return StorageStatus::fail(
			"cannot update thumbnail: clip was not found");
	return StorageStatus::ok();
}

RepositoryResult<std::optional<ClipMetadata>> ClipRepository::findById(const std::string &id) const
{
	if (!database_.isOpen()) {
		return RepositoryResult<std::optional<ClipMetadata>>::fail("cannot query clip: database is closed");
	}
	const auto sql = std::string("SELECT ") + kSelectColumns + " FROM clips WHERE id=?1 LIMIT 1;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!statement.valid() || !detail::bindText(statement.get(), 1, id)) {
		return detail::queryFailure<std::optional<ClipMetadata>>(database_.handle(),
									 "could not prepare clip query");
	}
	const auto result = sqlite3_step(statement.get());
	if (result == SQLITE_DONE) {
		return RepositoryResult<std::optional<ClipMetadata>>::ok(std::nullopt);
	}
	if (result != SQLITE_ROW) {
		return detail::queryFailure<std::optional<ClipMetadata>>(database_.handle(), "could not query clip");
	}
	auto clip = readClip(statement.get());
	if (!clip.success) {
		return RepositoryResult<std::optional<ClipMetadata>>::fail(clip.error);
	}
	return RepositoryResult<std::optional<ClipMetadata>>::ok(std::move(clip.value));
}

RepositoryResult<std::vector<ClipMetadata>> ClipRepository::listBySession(const std::string &sessionId) const
{
	if (!database_.isOpen()) {
		return RepositoryResult<std::vector<ClipMetadata>>::fail("cannot list clips: database is closed");
	}
	const auto sql = std::string("SELECT ") + kSelectColumns +
			 " FROM clips WHERE session_id=?1 ORDER BY created_at DESC, id DESC;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!statement.valid() || !detail::bindText(statement.get(), 1, sessionId)) {
		return detail::queryFailure<std::vector<ClipMetadata>>(database_.handle(),
								       "could not prepare session clip query");
	}
	return readClipList(database_.handle(), statement.get());
}

RepositoryResult<std::vector<ClipMetadata>> ClipRepository::listFavorites() const
{
	if (!database_.isOpen()) {
		return RepositoryResult<std::vector<ClipMetadata>>::fail("cannot list favorites: database is closed");
	}
	const auto sql = std::string("SELECT ") + kSelectColumns +
			 " FROM clips WHERE is_favorite=1 ORDER BY created_at DESC, id DESC;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!statement.valid()) {
		return detail::queryFailure<std::vector<ClipMetadata>>(database_.handle(),
								       "could not prepare favorites query");
	}
	return readClipList(database_.handle(), statement.get());
}

RepositoryResult<std::vector<ClipMetadata>> ClipRepository::listByOrientation(ClipOrientation orientation) const
{
	if (!database_.isOpen()) {
		return RepositoryResult<std::vector<ClipMetadata>>::fail(
			"cannot list clips by orientation: database is closed");
	}
	const auto sql = std::string("SELECT ") + kSelectColumns +
			 (orientation == ClipOrientation::Both
				  ? " FROM clips WHERE orientation='both'"
				  : " FROM clips WHERE orientation=?1 OR orientation='both'") +
			 " ORDER BY created_at DESC, id DESC;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!statement.valid() || (orientation != ClipOrientation::Both &&
				   !detail::bindText(statement.get(), 1, detail::toString(orientation)))) {
		return detail::queryFailure<std::vector<ClipMetadata>>(database_.handle(),
								       "could not prepare orientation query");
	}
	return readClipList(database_.handle(), statement.get());
}

RepositoryResult<std::vector<ClipMetadata>> ClipRepository::listRecent(int limit) const
{
	if (!database_.isOpen() || limit <= 0 || limit > 1000) {
		return RepositoryResult<std::vector<ClipMetadata>>::fail(
			"cannot list recent clips: invalid database or limit");
	}
	const auto sql =
		std::string("SELECT ") + kSelectColumns + " FROM clips ORDER BY created_at DESC, id DESC LIMIT ?1;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!statement.valid() || sqlite3_bind_int(statement.get(), 1, limit) != SQLITE_OK) {
		return detail::queryFailure<std::vector<ClipMetadata>>(database_.handle(),
								       "could not prepare recent clips query");
	}
	return readClipList(database_.handle(), statement.get());
}

StorageStatus ClipRepository::remove(const std::string &id)
{
	if (!database_.isOpen() || id.empty()) {
		return StorageStatus::fail("cannot delete clip: invalid database or clip id");
	}
	detail::Statement statement(database_.handle(), "DELETE FROM clips WHERE id=?1;");
	if (!statement.valid() || !detail::bindText(statement.get(), 1, id) ||
	    sqlite3_step(statement.get()) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not delete clip");
	}
	if (sqlite3_changes(database_.handle()) == 0) {
		return StorageStatus::fail("could not delete clip: id was not found");
	}
	return StorageStatus::ok();
}

StorageStatus ClipRepository::setFavorite(const std::string &id, bool favorite)
{
	if (!database_.isOpen() || id.empty()) {
		return StorageStatus::fail("cannot update favorite: invalid database or clip id");
	}
	detail::Statement statement(database_.handle(), "UPDATE clips SET is_favorite=?1 WHERE id=?2;");
	if (!statement.valid() || sqlite3_bind_int(statement.get(), 1, favorite ? 1 : 0) != SQLITE_OK ||
	    !detail::bindText(statement.get(), 2, id) || sqlite3_step(statement.get()) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not update clip favorite");
	}
	if (sqlite3_changes(database_.handle()) == 0) {
		return StorageStatus::fail("could not update clip favorite: id was not found");
	}
	return StorageStatus::ok();
}

StorageStatus ClipRepository::updateScore(const std::string &id, int score)
{
	if (!database_.isOpen() || score < 0 || score > 100) {
		return StorageStatus::fail("cannot update score: invalid database or score");
	}
	detail::Statement statement(database_.handle(), "UPDATE clips SET score=?1 WHERE id=?2;");
	if (!statement.valid() || sqlite3_bind_int(statement.get(), 1, score) != SQLITE_OK ||
	    !detail::bindText(statement.get(), 2, id) || sqlite3_step(statement.get()) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not update clip score");
	}
	if (sqlite3_changes(database_.handle()) == 0) {
		return StorageStatus::fail("could not update clip score: id was not found");
	}
	return StorageStatus::ok();
}

StorageStatus ClipRepository::markExported(const std::string &id)
{
	return setExportStatus(id, ExportStatus::Exported);
}

StorageStatus ClipRepository::updateAiResult(const AiPersistedClipResult &result)
{
	if (!database_.isOpen() || result.clipId.empty() || result.suggestedTitles.size() > 10 ||
	    result.hashtags.size() > 30 || result.score < -1 || result.score > 100)
		return StorageStatus::fail("cannot update AI result: metadata is invalid");
	detail::Statement statement(database_.handle(), R"SQL(
UPDATE clips SET title=?1, caption=?2, suggested_titles_blob=?3,
 hashtags_blob=?4, ai_summary=?5, subtitle_path=?6, ai_language=?7,
 score=CASE WHEN ?8 BETWEEN 0 AND 100 THEN ?8 ELSE score END
WHERE id=?9;
)SQL");
	if (!statement.valid() || !detail::bindText(statement.get(), 1, result.selectedTitle) ||
	    !detail::bindText(statement.get(), 2, result.caption) ||
	    !detail::bindText(statement.get(), 3, encodeList(result.suggestedTitles)) ||
	    !detail::bindText(statement.get(), 4, encodeList(result.hashtags)) ||
	    !detail::bindText(statement.get(), 5, result.summary) ||
	    !detail::bindText(statement.get(), 6, result.subtitlePath.u8string()) ||
	    !detail::bindText(statement.get(), 7, AiAssistantService::languageCode(result.language)) ||
	    sqlite3_bind_int(statement.get(), 8, result.score) != SQLITE_OK ||
	    !detail::bindText(statement.get(), 9, result.clipId) || sqlite3_step(statement.get()) != SQLITE_DONE)
		return detail::failure(database_.handle(), "could not update clip AI metadata");
	return sqlite3_changes(database_.handle()) == 0
		       ? StorageStatus::fail("could not update clip AI metadata: id was not found")
		       : StorageStatus::ok();
}

StorageStatus ClipRepository::setExportStatus(const std::string &id, ExportStatus status)
{
	if (!database_.isOpen()) {
		return StorageStatus::fail("cannot update clip export status: database is closed");
	}
	detail::Statement statement(database_.handle(), "UPDATE clips SET export_status=?1 WHERE id=?2;");
	if (!statement.valid() || !detail::bindText(statement.get(), 1, detail::toString(status)) ||
	    !detail::bindText(statement.get(), 2, id) || sqlite3_step(statement.get()) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not update clip export status");
	}
	if (sqlite3_changes(database_.handle()) == 0) {
		return StorageStatus::fail("could not update clip export status: id was not found");
	}
	return StorageStatus::ok();
}

} // namespace clipcoach::storage
