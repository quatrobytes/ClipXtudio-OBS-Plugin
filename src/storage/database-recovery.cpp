#include <clipcoach/storage/database-recovery.hpp>

#include <clipcoach/storage/sqlite-database.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>

namespace clipcoach::storage {
namespace {

bool indicatesCorruption(const std::string &message)
{
	std::string normalized = message;
	std::transform(normalized.begin(), normalized.end(), normalized.begin(),
		       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return normalized.find("not a database") != std::string::npos ||
	       normalized.find("database disk image is malformed") != std::string::npos ||
	       normalized.find("file is encrypted") != std::string::npos;
}

std::filesystem::path recoveryPath(const std::filesystem::path &databasePath)
{
	const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
				       std::chrono::system_clock::now().time_since_epoch())
				       .count();
	return databasePath.string() + ".corrupt-" + std::to_string(timestamp);
}

void preserveCompanion(const std::filesystem::path &source, const std::filesystem::path &backup, const char *suffix)
{
	const auto companion = std::filesystem::path(source.string() + suffix);
	if (!std::filesystem::exists(companion))
		return;
	std::error_code ignored;
	std::filesystem::rename(companion, std::filesystem::path(backup.string() + suffix), ignored);
}

} // namespace

DatabaseRecoveryResult openDatabaseWithRecovery(SqliteDatabase &database)
{
	auto status = database.open();
	if (status.success)
		return {std::move(status), false, {}};
	std::error_code fileStatusError;
	const bool regularDatabaseFile = std::filesystem::is_regular_file(database.path(), fileStatusError);
	if (!indicatesCorruption(status.error) || fileStatusError || !regularDatabaseFile)
		return {std::move(status), false, {}};

	database.close();
	const auto backup = recoveryPath(database.path());
	std::error_code moveError;
	std::filesystem::rename(database.path(), backup, moveError);
	if (moveError) {
		return {StorageStatus::fail("database is corrupt but could not be preserved: " + moveError.message()),
			false,
			{}};
	}
	preserveCompanion(database.path(), backup, "-wal");
	preserveCompanion(database.path(), backup, "-shm");

	status = database.open();
	if (!status.success) {
		return {StorageStatus::fail("database recovery failed; corrupt copy preserved at " + backup.string() +
					    ": " + status.error),
			false, backup};
	}
	return {StorageStatus::ok(), true, backup};
}

} // namespace clipcoach::storage
