#pragma once

#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/storage/repository-result.hpp>

#include <optional>
#include <string>
#include <vector>

namespace clipcoach::storage {

class SqliteDatabase;

class ExportJobRepository final {
public:
	explicit ExportJobRepository(SqliteDatabase &database);

	[[nodiscard]] StorageStatus record(const ExportJob &job);
	[[nodiscard]] RepositoryResult<std::optional<ExportJob>> findById(const std::string &id) const;
	[[nodiscard]] RepositoryResult<std::vector<ExportJob>> listByClip(const std::string &clipId) const;

private:
	SqliteDatabase &database_;
};

} // namespace clipcoach::storage
