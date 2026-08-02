#pragma once

#include <clipcoach/core/session-metadata.hpp>
#include <clipcoach/storage/repository-result.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace clipcoach::storage {

class SqliteDatabase;

class SessionRepository final {
public:
	explicit SessionRepository(SqliteDatabase &database);

	[[nodiscard]] StorageStatus insert(const SessionMetadata &session);
	[[nodiscard]] RepositoryResult<std::optional<SessionMetadata>> findById(const std::string &id) const;
	[[nodiscard]] StorageStatus endSession(const std::string &id, std::chrono::system_clock::time_point endedAt);
	[[nodiscard]] StorageStatus updateAiSummary(const std::string &id, const std::string &summary,
						    const std::string &language);

private:
	SqliteDatabase &database_;
};

} // namespace clipcoach::storage
