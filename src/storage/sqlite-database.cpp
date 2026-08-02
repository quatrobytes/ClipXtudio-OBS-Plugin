#include <clipcoach/storage/sqlite-database.hpp>

#include <sqlite3.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

namespace clipcoach::storage {
namespace {

constexpr const char *kMigrationOne = R"SQL(
CREATE TABLE sessions (
  id TEXT PRIMARY KEY NOT NULL,
  started_at TEXT NOT NULL,
  ended_at TEXT,
  profile_name TEXT NOT NULL DEFAULT '',
  scene_collection TEXT NOT NULL DEFAULT ''
);
CREATE TABLE clips (
  id TEXT PRIMARY KEY NOT NULL,
  session_id TEXT NOT NULL,
  file_path TEXT NOT NULL,
  file_name TEXT NOT NULL,
  created_at TEXT NOT NULL,
  duration_seconds INTEGER NOT NULL CHECK(duration_seconds >= 0),
  orientation TEXT NOT NULL CHECK(orientation IN ('horizontal', 'vertical', 'both')),
  trigger_type TEXT NOT NULL CHECK(trigger_type IN
    ('manual', 'voice', 'audio_spike', 'chat', 'scene', 'ai')),
  score INTEGER NOT NULL DEFAULT 0 CHECK(score BETWEEN 0 AND 100),
  title TEXT NOT NULL DEFAULT '',
  is_favorite INTEGER NOT NULL DEFAULT 0 CHECK(is_favorite IN (0, 1)),
  export_status TEXT NOT NULL DEFAULT 'exported' CHECK(export_status IN
    ('pending', 'exporting', 'exported', 'failed')),
  source_scene TEXT NOT NULL DEFAULT '',
  FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE
);
)SQL";

constexpr const char *kMigrationTwo = R"SQL(
ALTER TABLE sessions ADD COLUMN app_version TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN caption TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN transcript_path TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN subtitle_path TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN thumbnail_path TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN app_version TEXT NOT NULL DEFAULT '';
CREATE INDEX idx_clips_session_created ON clips(session_id, created_at DESC);
CREATE INDEX idx_clips_favorite_created ON clips(is_favorite, created_at DESC);
CREATE INDEX idx_clips_orientation_created ON clips(orientation, created_at DESC);
)SQL";

constexpr const char *kMigrationThree = R"SQL(
CREATE TABLE exports (
  id TEXT PRIMARY KEY NOT NULL,
  clip_id TEXT NOT NULL,
  source_path TEXT NOT NULL,
  output_path TEXT NOT NULL,
  orientation TEXT NOT NULL CHECK(orientation IN ('horizontal', 'vertical')),
  preset TEXT NOT NULL CHECK(preset IN ('low', 'medium', 'high', 'maximum')),
  state TEXT NOT NULL CHECK(state IN
    ('pending', 'exporting', 'done', 'error', 'cancelled')),
  progress_percent INTEGER NOT NULL DEFAULT 0 CHECK(progress_percent BETWEEN 0 AND 100),
  error TEXT NOT NULL DEFAULT '',
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY(clip_id) REFERENCES clips(id) ON DELETE CASCADE
);
CREATE INDEX idx_exports_clip_created ON exports(clip_id, created_at DESC);
CREATE INDEX idx_exports_state_updated ON exports(state, updated_at DESC);
)SQL";

constexpr const char *kMigrationFour = R"SQL(
ALTER TABLE clips ADD COLUMN suggested_titles_blob TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN hashtags_blob TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN ai_summary TEXT NOT NULL DEFAULT '';
ALTER TABLE clips ADD COLUMN ai_language TEXT NOT NULL DEFAULT '';
ALTER TABLE sessions ADD COLUMN ai_summary TEXT NOT NULL DEFAULT '';
ALTER TABLE sessions ADD COLUMN ai_language TEXT NOT NULL DEFAULT '';
)SQL";

constexpr const char *kMigrationFive = R"SQL(
ALTER TABLE clips ADD COLUMN trigger_label TEXT NOT NULL DEFAULT '';
)SQL";

constexpr const char *kMigrationSix = R"SQL(
UPDATE clips
SET export_status = 'exported'
WHERE export_status = 'pending'
  AND NOT EXISTS (
    SELECT 1
    FROM exports
    WHERE exports.clip_id = clips.id
      AND exports.state IN ('pending', 'exporting')
  );
)SQL";

constexpr const char *kMigrationSeven = R"SQL(
ALTER TABLE clips ADD COLUMN requested_by TEXT NOT NULL DEFAULT '';
UPDATE clips
SET trigger_label = 'remote_clipper'
WHERE trigger_label = 'manual'
  AND (
    lower(file_path) LIKE '%remote clipper%'
    OR lower(file_name) LIKE '%_remote_%'
  );
)SQL";

StorageStatus sqliteFailure(sqlite3 *database, const std::string &operation)
{
	return StorageStatus::fail(operation + ": " +
				   (database != nullptr ? sqlite3_errmsg(database) : "database is not open"));
}

} // namespace

SqliteDatabase::SqliteDatabase(std::filesystem::path path) : path_(std::move(path)) {}

SqliteDatabase::~SqliteDatabase()
{
	close();
}

StorageStatus SqliteDatabase::open()
{
	if (database_ != nullptr) {
		return StorageStatus::ok();
	}

	std::error_code fileError;
	const auto parent = path_.parent_path();
	if (!parent.empty()) {
		std::filesystem::create_directories(parent, fileError);
		if (fileError) {
			return StorageStatus::fail("could not create database directory: " + fileError.message());
		}
	}

	const auto utf8Path = path_.u8string();
	const auto result = sqlite3_open_v2(utf8Path.c_str(), &database_,
					    SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
					    nullptr);
	if (result != SQLITE_OK) {
		const auto error = sqliteFailure(database_, "could not open SQLite database");
		close();
		return error;
	}

	sqlite3_busy_timeout(database_, 3000);
	auto status = configure();
	if (!status.success) {
		close();
		return status;
	}
	status = migrate();
	if (!status.success) {
		close();
		return status;
	}
	return StorageStatus::ok();
}

void SqliteDatabase::close() noexcept
{
	if (database_ != nullptr) {
		sqlite3_close_v2(database_);
		database_ = nullptr;
	}
	schemaVersion_ = 0;
}

bool SqliteDatabase::isOpen() const noexcept
{
	return database_ != nullptr;
}

int SqliteDatabase::schemaVersion() const noexcept
{
	return schemaVersion_;
}

bool SqliteDatabase::hasTable(const std::string &tableName) const
{
	if (database_ == nullptr) {
		return false;
	}
	sqlite3_stmt *statement = nullptr;
	const char *sql = "SELECT 1 FROM sqlite_master WHERE type='table' AND name=?1 LIMIT 1";
	if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) {
		return false;
	}
	sqlite3_bind_text(statement, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);
	const bool found = sqlite3_step(statement) == SQLITE_ROW;
	sqlite3_finalize(statement);
	return found;
}

bool SqliteDatabase::hasColumn(const std::string &tableName, const std::string &columnName) const
{
	if (database_ == nullptr) {
		return false;
	}
	sqlite3_stmt *statement = nullptr;
	const char *sql = "SELECT name FROM pragma_table_info(?1);";
	if (sqlite3_prepare_v2(database_, sql, -1, &statement, nullptr) != SQLITE_OK) {
		return false;
	}
	sqlite3_bind_text(statement, 1, tableName.c_str(), -1, SQLITE_TRANSIENT);

	bool found = false;
	while (sqlite3_step(statement) == SQLITE_ROW) {
		const auto *name = sqlite3_column_text(statement, 0);
		if (name != nullptr && columnName == reinterpret_cast<const char *>(name)) {
			found = true;
			break;
		}
	}
	sqlite3_finalize(statement);
	return found;
}

const std::filesystem::path &SqliteDatabase::path() const noexcept
{
	return path_;
}

sqlite3 *SqliteDatabase::handle() const noexcept
{
	return database_;
}

StorageStatus SqliteDatabase::configure()
{
	auto status = execute("PRAGMA foreign_keys = ON;");
	if (!status.success) {
		return status;
	}
	status = execute("PRAGMA journal_mode = WAL;");
	if (!status.success) {
		return status;
	}
	return execute("PRAGMA synchronous = NORMAL;");
}

StorageStatus SqliteDatabase::migrate()
{
	const auto version = readUserVersion();
	if (version < 0) {
		return sqliteFailure(database_, "could not read schema version");
	}
	if (version > kCurrentSchemaVersion) {
		return StorageStatus::fail("database schema is newer than this plugin");
	}
	if (version == kCurrentSchemaVersion) {
		schemaVersion_ = version;
		return StorageStatus::ok();
	}

	auto status = execute("BEGIN IMMEDIATE;");
	if (!status.success) {
		return status;
	}

	int appliedVersion = version;
	if (appliedVersion < 1) {
		status = execute(kMigrationOne);
		if (status.success) {
			appliedVersion = 1;
		}
	}
	if (status.success && appliedVersion < 2) {
		status = execute(kMigrationTwo);
		if (status.success) {
			appliedVersion = 2;
		}
	}
	if (status.success && appliedVersion < 3) {
		status = execute(kMigrationThree);
		if (status.success) {
			appliedVersion = 3;
		}
	}
	if (status.success && appliedVersion < 4) {
		status = execute(kMigrationFour);
		if (status.success) {
			appliedVersion = 4;
		}
	}
	if (status.success && appliedVersion < 5) {
		status = execute(kMigrationFive);
		if (status.success) {
			appliedVersion = 5;
		}
	}
	if (status.success && appliedVersion < 6) {
		status = execute(kMigrationSix);
		if (status.success) {
			appliedVersion = 6;
		}
	}
	if (status.success && appliedVersion < 7) {
		status = execute(kMigrationSeven);
		if (status.success) {
			appliedVersion = 7;
		}
	}
	if (status.success) {
		status = execute("PRAGMA user_version = 7;");
	}
	if (status.success) {
		status = execute("COMMIT;");
	} else {
		(void)execute("ROLLBACK;");
	}

	if (!status.success) {
		return StorageStatus::fail("database migration failed: " + status.error);
	}
	schemaVersion_ = appliedVersion;
	return StorageStatus::ok();
}

StorageStatus SqliteDatabase::execute(const char *sql)
{
	char *rawError = nullptr;
	const auto result = sqlite3_exec(database_, sql, nullptr, nullptr, &rawError);
	if (result == SQLITE_OK) {
		return StorageStatus::ok();
	}
	const std::string message = rawError != nullptr ? rawError : sqlite3_errmsg(database_);
	sqlite3_free(rawError);
	return StorageStatus::fail(message);
}

int SqliteDatabase::readUserVersion() const noexcept
{
	sqlite3_stmt *statement = nullptr;
	if (sqlite3_prepare_v2(database_, "PRAGMA user_version;", -1, &statement, nullptr) != SQLITE_OK) {
		return -1;
	}
	const auto result = sqlite3_step(statement);
	const int version = result == SQLITE_ROW ? sqlite3_column_int(statement, 0) : -1;
	sqlite3_finalize(statement);
	return version;
}

} // namespace clipcoach::storage
