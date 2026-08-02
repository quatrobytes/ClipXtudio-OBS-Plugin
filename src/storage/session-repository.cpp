#include <clipcoach/storage/session-repository.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

#include "sqlite-helpers.hpp"

#include <sqlite3.h>

namespace clipcoach::storage {

SessionRepository::SessionRepository(SqliteDatabase &database) : database_(database) {}

StorageStatus SessionRepository::insert(const SessionMetadata &session)
{
	if (!database_.isOpen() || session.id.empty()) {
		return StorageStatus::fail("cannot insert session: database is closed or id is empty");
	}
	const char *sql = R"SQL(
INSERT INTO sessions
  (id, started_at, ended_at, profile_name, scene_collection, app_version)
VALUES (?1, ?2, ?3, ?4, ?5, ?6)
ON CONFLICT(id) DO UPDATE SET
  ended_at=excluded.ended_at,
  profile_name=excluded.profile_name,
  scene_collection=excluded.scene_collection,
  app_version=excluded.app_version;
)SQL";
	detail::Statement statement(database_.handle(), sql);
	if (!statement.valid()) {
		return detail::failure(database_.handle(), "could not prepare session insert");
	}

	const auto startedAt = detail::formatTime(session.startedAt);
	const auto endedAt = session.endedAt ? detail::formatTime(*session.endedAt) : std::string{};
	auto *query = statement.get();
	const bool bound =
		detail::bindText(query, 1, session.id) && detail::bindText(query, 2, startedAt) &&
		(session.endedAt ? detail::bindText(query, 3, endedAt) : sqlite3_bind_null(query, 3) == SQLITE_OK) &&
		detail::bindText(query, 4, session.profileName) &&
		detail::bindText(query, 5, session.sceneCollection) && detail::bindText(query, 6, session.appVersion);
	if (!bound || sqlite3_step(query) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not insert session");
	}
	return StorageStatus::ok();
}

RepositoryResult<std::optional<SessionMetadata>> SessionRepository::findById(const std::string &id) const
{
	if (!database_.isOpen()) {
		return RepositoryResult<std::optional<SessionMetadata>>::fail(
			"cannot query session: database is closed");
	}
	const char *sql = R"SQL(
SELECT id, started_at, ended_at, profile_name, scene_collection, app_version,
       ai_summary, ai_language
FROM sessions WHERE id=?1 LIMIT 1;
)SQL";
	detail::Statement statement(database_.handle(), sql);
	if (!statement.valid() || !detail::bindText(statement.get(), 1, id)) {
		return detail::queryFailure<std::optional<SessionMetadata>>(database_.handle(),
									    "could not prepare session query");
	}

	const auto result = sqlite3_step(statement.get());
	if (result == SQLITE_DONE) {
		return RepositoryResult<std::optional<SessionMetadata>>::ok(std::nullopt);
	}
	if (result != SQLITE_ROW) {
		return detail::queryFailure<std::optional<SessionMetadata>>(database_.handle(),
									    "could not query session");
	}

	SessionMetadata session;
	session.id = detail::columnText(statement.get(), 0);
	const auto startedAt = detail::parseTime(detail::columnText(statement.get(), 1));
	if (!startedAt) {
		return RepositoryResult<std::optional<SessionMetadata>>::fail(
			"stored session has an invalid started_at");
	}
	session.startedAt = *startedAt;
	if (sqlite3_column_type(statement.get(), 2) != SQLITE_NULL) {
		session.endedAt = detail::parseTime(detail::columnText(statement.get(), 2));
		if (!session.endedAt) {
			return RepositoryResult<std::optional<SessionMetadata>>::fail(
				"stored session has an invalid ended_at");
		}
	}
	session.profileName = detail::columnText(statement.get(), 3);
	session.sceneCollection = detail::columnText(statement.get(), 4);
	session.appVersion = detail::columnText(statement.get(), 5);
	session.aiSummary = detail::columnText(statement.get(), 6);
	session.aiLanguage = detail::columnText(statement.get(), 7);
	return RepositoryResult<std::optional<SessionMetadata>>::ok(std::move(session));
}

StorageStatus SessionRepository::updateAiSummary(const std::string &id, const std::string &summary,
						 const std::string &language)
{
	if (!database_.isOpen() || id.empty() || summary.size() > 20'000)
		return StorageStatus::fail("cannot update session AI summary: metadata is invalid");
	detail::Statement statement(database_.handle(),
				    "UPDATE sessions SET ai_summary=?1, ai_language=?2 WHERE id=?3;");
	if (!statement.valid() || !detail::bindText(statement.get(), 1, summary) ||
	    !detail::bindText(statement.get(), 2, language) || !detail::bindText(statement.get(), 3, id) ||
	    sqlite3_step(statement.get()) != SQLITE_DONE)
		return detail::failure(database_.handle(), "could not update session AI summary");
	return sqlite3_changes(database_.handle()) == 0
		       ? StorageStatus::fail("could not update session AI summary: id was not found")
		       : StorageStatus::ok();
}

StorageStatus SessionRepository::endSession(const std::string &id, std::chrono::system_clock::time_point endedAt)
{
	if (!database_.isOpen()) {
		return StorageStatus::fail("cannot end session: database is closed");
	}
	detail::Statement statement(database_.handle(), "UPDATE sessions SET ended_at=?1 WHERE id=?2;");
	if (!statement.valid() || !detail::bindText(statement.get(), 1, detail::formatTime(endedAt)) ||
	    !detail::bindText(statement.get(), 2, id) || sqlite3_step(statement.get()) != SQLITE_DONE) {
		return detail::failure(database_.handle(), "could not end session");
	}
	if (sqlite3_changes(database_.handle()) == 0) {
		return StorageStatus::fail("could not end session: id was not found");
	}
	return StorageStatus::ok();
}

} // namespace clipcoach::storage
