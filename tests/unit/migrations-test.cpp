#include "storage-test-support.hpp"
#include "test-support.hpp"

#include <clipcoach/storage/clip-repository.hpp>
#include <clipcoach/storage/session-repository.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

#include <sqlite3.h>

#include <string>

namespace {

void createVersionOneDatabase(const std::filesystem::path &path)
{
	sqlite3 *database = nullptr;
	clipcoach::test::expect(sqlite3_open(path.string().c_str(), &database) == SQLITE_OK,
				"legacy database must open");
	const char *sql = R"SQL(
PRAGMA foreign_keys = ON;
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
  duration_seconds INTEGER NOT NULL,
  orientation TEXT NOT NULL,
  trigger_type TEXT NOT NULL,
  score INTEGER NOT NULL DEFAULT 0,
  title TEXT NOT NULL DEFAULT '',
  is_favorite INTEGER NOT NULL DEFAULT 0,
  export_status TEXT NOT NULL DEFAULT 'pending',
  source_scene TEXT NOT NULL DEFAULT '',
  FOREIGN KEY(session_id) REFERENCES sessions(id) ON DELETE CASCADE
);
INSERT INTO sessions (id, started_at) VALUES ('legacy-session', '2026-08-01T00:00:00Z');
INSERT INTO clips (
  id, session_id, file_path, file_name, created_at, duration_seconds,
  orientation, trigger_type, export_status
) VALUES (
  'legacy-pending', 'legacy-session', 'legacy.mp4', 'legacy.mp4',
  '2026-08-01T00:00:00Z', 30, 'horizontal', 'manual', 'pending'
);
PRAGMA user_version = 1;
)SQL";
	char *error = nullptr;
	const auto result = sqlite3_exec(database, sql, nullptr, nullptr, &error);
	const std::string message = error != nullptr ? error : "";
	sqlite3_free(error);
	sqlite3_close(database);
	clipcoach::test::expect(result == SQLITE_OK, "legacy schema creation failed: " + message);
}

} // namespace

int main()
{
	clipcoach::test::TemporaryDatabase temporary;
	createVersionOneDatabase(temporary.path());

	clipcoach::storage::SqliteDatabase database(temporary.path());
	clipcoach::test::expect(database.open().success, "legacy database must migrate");
	clipcoach::test::expect(database.schemaVersion() == 7, "legacy database must reach schema version 7");
	clipcoach::test::expect(database.hasColumn("clips", "suggested_titles_blob") &&
					database.hasColumn("clips", "hashtags_blob") &&
					database.hasColumn("sessions", "ai_summary"),
				"AI metadata migration must add clip and session columns");
	clipcoach::test::expect(database.hasColumn("clips", "caption"), "migration 2 must add caption");
	clipcoach::test::expect(database.hasColumn("clips", "transcript_path"), "migration 2 must add transcript path");
	clipcoach::test::expect(database.hasColumn("clips", "app_version"), "migration 2 must add app version");
	clipcoach::test::expect(database.hasTable("exports"), "migration 3 must add exports");
	clipcoach::test::expect(database.hasColumn("clips", "trigger_label"),
				"migration 5 must add the matched trigger label");
	clipcoach::test::expect(database.hasColumn("clips", "requested_by"),
				"migration 7 must add the remote requester attribution");
	clipcoach::storage::ClipRepository migratedClips(database);
	const auto repaired = migratedClips.findById("legacy-pending");
	clipcoach::test::expect(repaired.success && repaired.value.has_value() &&
					repaired.value->exportStatus == clipcoach::ExportStatus::Exported,
				"migration 6 must repair historical source clips falsely left pending");

	clipcoach::storage::SessionRepository sessions(database);
	clipcoach::storage::ClipRepository clips(database);
	clipcoach::test::expect(sessions.insert(clipcoach::test::sampleSession()).success,
				"session insert must work after migration");
	auto sample = clipcoach::test::sampleClip();
	sample.requestedBy = "editor@example.com";
	clipcoach::test::expect(clips.insert(sample).success, "full clip insert must work after migration");
	const auto restored = clips.findById(sample.id);
	clipcoach::test::expect(restored.success && restored.value.has_value() &&
					restored.value->triggerLabel == "saca clip" &&
					restored.value->requestedBy == "editor@example.com",
				"trigger label and requester must survive SQLite persistence");

	return clipcoach::test::pass("migrations-test");
}
