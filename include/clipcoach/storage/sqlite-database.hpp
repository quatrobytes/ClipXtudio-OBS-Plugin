#pragma once

#include <clipcoach/storage/repository-result.hpp>

#include <filesystem>
#include <string>

struct sqlite3;

namespace clipcoach::storage {

class SqliteDatabase final {
public:
	static constexpr int kCurrentSchemaVersion = 7;

	explicit SqliteDatabase(std::filesystem::path path);
	~SqliteDatabase();

	SqliteDatabase(const SqliteDatabase &) = delete;
	SqliteDatabase &operator=(const SqliteDatabase &) = delete;

	[[nodiscard]] StorageStatus open();
	void close() noexcept;
	[[nodiscard]] bool isOpen() const noexcept;
	[[nodiscard]] int schemaVersion() const noexcept;
	[[nodiscard]] bool hasTable(const std::string &tableName) const;
	[[nodiscard]] bool hasColumn(const std::string &tableName,
				     const std::string &columnName) const;
	[[nodiscard]] const std::filesystem::path &path() const noexcept;
	[[nodiscard]] sqlite3 *handle() const noexcept;

private:
	[[nodiscard]] StorageStatus configure();
	[[nodiscard]] StorageStatus migrate();
	[[nodiscard]] StorageStatus execute(const char *sql);
	[[nodiscard]] int readUserVersion() const noexcept;

	std::filesystem::path path_;
	sqlite3 *database_{nullptr};
	int schemaVersion_{0};
};

} // namespace clipcoach::storage
