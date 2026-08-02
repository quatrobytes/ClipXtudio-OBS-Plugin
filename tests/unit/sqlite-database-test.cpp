#include "storage-test-support.hpp"
#include "test-support.hpp"

#include <clipcoach/storage/sqlite-database.hpp>

#include <filesystem>

int main()
{
	clipcoach::test::TemporaryDatabase temporary;
	clipcoach::storage::SqliteDatabase database(temporary.path());

	const auto opened = database.open();
	clipcoach::test::expect(opened.success, "temporary SQLite database must open");
	clipcoach::test::expect(opened.error.empty(), "successful open must not expose an error");
	clipcoach::test::expect(std::filesystem::exists(temporary.path()), "opening must create the database file");
	clipcoach::test::expect(database.schemaVersion() == clipcoach::storage::SqliteDatabase::kCurrentSchemaVersion,
				"fresh database must migrate to the current schema");
	clipcoach::test::expect(database.hasTable("sessions"), "sessions table must exist");
	clipcoach::test::expect(database.hasTable("clips"), "clips table must exist");
	clipcoach::test::expect(database.hasTable("exports"), "exports table must exist");

	database.close();
	const auto reopened = database.open();
	clipcoach::test::expect(reopened.success, "database must reopen after close");
	clipcoach::test::expect(database.schemaVersion() == clipcoach::storage::SqliteDatabase::kCurrentSchemaVersion,
				"schema version must persist after reopen");

	return clipcoach::test::pass("sqlite-database-test");
}
