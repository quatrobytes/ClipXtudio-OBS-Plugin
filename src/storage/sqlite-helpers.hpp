#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/storage/repository-result.hpp>

#include <sqlite3.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace clipcoach::storage::detail {

class Statement final {
public:
	Statement(sqlite3 *database, const char *sql)
	{
		result_ = sqlite3_prepare_v2(database, sql, -1, &statement_, nullptr);
	}
	~Statement()
	{
		if (statement_ != nullptr) {
			sqlite3_finalize(statement_);
		}
	}
	Statement(const Statement &) = delete;
	Statement &operator=(const Statement &) = delete;

	[[nodiscard]] bool valid() const noexcept { return result_ == SQLITE_OK; }
	[[nodiscard]] sqlite3_stmt *get() const noexcept { return statement_; }

private:
	sqlite3_stmt *statement_{nullptr};
	int result_{SQLITE_ERROR};
};

inline StorageStatus failure(sqlite3 *database, const std::string &operation)
{
	return StorageStatus::fail(operation + ": " + sqlite3_errmsg(database));
}

template<typename T> RepositoryResult<T> queryFailure(sqlite3 *database, const std::string &operation)
{
	return RepositoryResult<T>::fail(operation + ": " + sqlite3_errmsg(database));
}

inline bool bindText(sqlite3_stmt *statement, int index, const std::string &value)
{
	return sqlite3_bind_text(statement, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

inline std::string columnText(sqlite3_stmt *statement, int index)
{
	const auto *value = sqlite3_column_text(statement, index);
	return value != nullptr ? reinterpret_cast<const char *>(value) : "";
}

inline std::string formatTime(std::chrono::system_clock::time_point value)
{
	const auto time = std::chrono::system_clock::to_time_t(value);
	std::tm utc{};
#ifdef _WIN32
	gmtime_s(&utc, &time);
#else
	gmtime_r(&time, &utc);
#endif
	std::ostringstream output;
	output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
	return output.str();
}

inline std::optional<std::chrono::system_clock::time_point> parseTime(const std::string &value)
{
	std::tm utc{};
	std::istringstream input(value);
	input >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
	if (input.fail()) {
		return std::nullopt;
	}
#ifdef _WIN32
	const auto time = _mkgmtime(&utc);
#else
	const auto time = timegm(&utc);
#endif
	if (time == static_cast<std::time_t>(-1)) {
		return std::nullopt;
	}
	return std::chrono::system_clock::from_time_t(time);
}

inline const char *toString(ClipOrientation value)
{
	switch (value) {
	case ClipOrientation::Vertical:
		return "vertical";
	case ClipOrientation::Both:
		return "both";
	case ClipOrientation::Horizontal:
		return "horizontal";
	}
	return "horizontal";
}

inline std::optional<ClipOrientation> orientationFromString(const std::string &value)
{
	if (value == "horizontal") {
		return ClipOrientation::Horizontal;
	}
	if (value == "vertical") {
		return ClipOrientation::Vertical;
	}
	if (value == "both") {
		return ClipOrientation::Both;
	}
	return std::nullopt;
}

inline const char *toString(TriggerType value)
{
	switch (value) {
	case TriggerType::Manual:
		return "manual";
	case TriggerType::Voice:
		return "voice";
	case TriggerType::AudioSpike:
		return "audio_spike";
	case TriggerType::Chat:
		return "chat";
	case TriggerType::Scene:
		return "scene";
	case TriggerType::Ai:
		return "ai";
	}
	return "manual";
}

inline std::optional<TriggerType> triggerFromString(const std::string &value)
{
	if (value == "manual") {
		return TriggerType::Manual;
	}
	if (value == "voice") {
		return TriggerType::Voice;
	}
	if (value == "audio_spike") {
		return TriggerType::AudioSpike;
	}
	if (value == "chat") {
		return TriggerType::Chat;
	}
	if (value == "scene") {
		return TriggerType::Scene;
	}
	if (value == "ai") {
		return TriggerType::Ai;
	}
	return std::nullopt;
}

inline const char *toString(ExportStatus value)
{
	switch (value) {
	case ExportStatus::Pending:
		return "pending";
	case ExportStatus::Exporting:
		return "exporting";
	case ExportStatus::Exported:
		return "exported";
	case ExportStatus::Failed:
		return "failed";
	}
	return "pending";
}

inline std::optional<ExportStatus> exportStatusFromString(const std::string &value)
{
	if (value == "pending") {
		return ExportStatus::Pending;
	}
	if (value == "exporting") {
		return ExportStatus::Exporting;
	}
	if (value == "exported") {
		return ExportStatus::Exported;
	}
	if (value == "failed") {
		return ExportStatus::Failed;
	}
	return std::nullopt;
}

} // namespace clipcoach::storage::detail
