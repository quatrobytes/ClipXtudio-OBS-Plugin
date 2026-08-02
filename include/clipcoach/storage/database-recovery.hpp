#pragma once

#include <clipcoach/storage/repository-result.hpp>

#include <filesystem>

namespace clipcoach::storage {

class SqliteDatabase;

struct DatabaseRecoveryResult {
	StorageStatus status;
	bool recovered{false};
	std::filesystem::path backupPath;
};

// Opens the library and, only for recognizable SQLite corruption, preserves the
// original file under a .corrupt-* name before creating a clean fallback DB.
[[nodiscard]] DatabaseRecoveryResult openDatabaseWithRecovery(SqliteDatabase &database);

} // namespace clipcoach::storage
