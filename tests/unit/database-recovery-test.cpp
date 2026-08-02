#include "storage-test-support.hpp"
#include "test-support.hpp"

#include <clipcoach/storage/database-recovery.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

int main()
{
	using clipcoach::storage::SqliteDatabase;
	using clipcoach::storage::openDatabaseWithRecovery;
	using clipcoach::test::expect;

	clipcoach::test::TemporaryDatabase temporary;
	const auto path = temporary.path();
	const std::string corruptBytes = "this is not a sqlite database";
	{
		std::ofstream output(path, std::ios::binary);
		output << corruptBytes;
	}

	SqliteDatabase database(path);
	const auto result = openDatabaseWithRecovery(database);
	expect(result.status.success, "corrupt DB must fall back to a clean database");
	expect(result.recovered, "recovery result must report the fallback");
	expect(database.isOpen(), "fallback database must be open");
	expect(database.schemaVersion() == SqliteDatabase::kCurrentSchemaVersion, "fallback database must be migrated");
	expect(std::filesystem::exists(result.backupPath), "corrupt source must be preserved");

	std::ifstream backup(result.backupPath, std::ios::binary);
	const std::string preserved((std::istreambuf_iterator<char>(backup)), std::istreambuf_iterator<char>());
	expect(preserved == corruptBytes, "fallback must not modify or delete corrupt user data");

	database.close();
	return clipcoach::test::pass("database-recovery-test");
}
