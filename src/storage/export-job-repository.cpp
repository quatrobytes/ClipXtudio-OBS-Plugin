#include <clipcoach/storage/export-job-repository.hpp>

#include <clipcoach/storage/sqlite-database.hpp>

#include "sqlite-helpers.hpp"

#include <sqlite3.h>

namespace clipcoach::storage {
namespace {

const char *toString(ExportOrientation value)
{
	return value == ExportOrientation::Vertical ? "vertical" : "horizontal";
}

const char *toString(ExportQualityPreset value)
{
	switch (value) {
	case ExportQualityPreset::Low:
		return "low";
	case ExportQualityPreset::Medium:
		return "medium";
	case ExportQualityPreset::High:
		return "high";
	case ExportQualityPreset::Maximum:
		return "maximum";
	}
	return "high";
}

const char *toString(ExportJobState value)
{
	switch (value) {
	case ExportJobState::Pending:
		return "pending";
	case ExportJobState::Exporting:
		return "exporting";
	case ExportJobState::Done:
		return "done";
	case ExportJobState::Error:
		return "error";
	case ExportJobState::Cancelled:
		return "cancelled";
	}
	return "error";
}

std::optional<ExportOrientation> orientationFromString(const std::string &value)
{
	if (value == "horizontal") {
		return ExportOrientation::Horizontal;
	}
	if (value == "vertical") {
		return ExportOrientation::Vertical;
	}
	return std::nullopt;
}

std::optional<ExportQualityPreset> presetFromString(const std::string &value)
{
	if (value == "low") {
		return ExportQualityPreset::Low;
	}
	if (value == "medium") {
		return ExportQualityPreset::Medium;
	}
	if (value == "high") {
		return ExportQualityPreset::High;
	}
	if (value == "maximum") {
		return ExportQualityPreset::Maximum;
	}
	return std::nullopt;
}

std::optional<ExportJobState> stateFromString(const std::string &value)
{
	if (value == "pending") {
		return ExportJobState::Pending;
	}
	if (value == "exporting") {
		return ExportJobState::Exporting;
	}
	if (value == "done") {
		return ExportJobState::Done;
	}
	if (value == "error") {
		return ExportJobState::Error;
	}
	if (value == "cancelled") {
		return ExportJobState::Cancelled;
	}
	return std::nullopt;
}

RepositoryResult<ExportJob> readJob(sqlite3_stmt *query)
{
	ExportJob job;
	job.id = detail::columnText(query, 0);
	job.request.clipId = detail::columnText(query, 1);
	job.request.sourcePath =
		std::filesystem::u8path(detail::columnText(query, 2));
	job.outputPath = std::filesystem::u8path(detail::columnText(query, 3));
	const auto orientation =
		orientationFromString(detail::columnText(query, 4));
	const auto preset = presetFromString(detail::columnText(query, 5));
	const auto state = stateFromString(detail::columnText(query, 6));
	const auto created = detail::parseTime(detail::columnText(query, 9));
	const auto updated = detail::parseTime(detail::columnText(query, 10));
	if (!orientation || !preset || !state || !created || !updated) {
		return RepositoryResult<ExportJob>::fail(
			"stored export contains invalid metadata");
	}
	job.orientation = *orientation;
	job.request.orientation = *orientation;
	job.request.preset = *preset;
	job.state = *state;
	job.progressPercent = sqlite3_column_int(query, 7);
	job.error = detail::columnText(query, 8);
	job.createdAt = *created;
	job.updatedAt = *updated;
	return RepositoryResult<ExportJob>::ok(std::move(job));
}

constexpr const char *kColumns =
	"id, clip_id, source_path, output_path, orientation, preset, state, "
	"progress_percent, error, created_at, updated_at";

} // namespace

ExportJobRepository::ExportJobRepository(SqliteDatabase &database)
	: database_(database)
{
}

StorageStatus ExportJobRepository::record(const ExportJob &job)
{
	if (!database_.isOpen() || job.id.empty() || job.request.clipId.empty() ||
	    job.outputPath.empty() || job.progressPercent < 0 ||
	    job.progressPercent > 100 ||
	    job.orientation == ExportOrientation::Both) {
		return StorageStatus::fail("cannot record export: metadata is invalid");
	}
	detail::Statement statement(database_.handle(), R"SQL(
INSERT INTO exports (
  id, clip_id, source_path, output_path, orientation, preset, state,
  progress_percent, error, created_at, updated_at
) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11)
ON CONFLICT(id) DO UPDATE SET
  output_path=excluded.output_path,
  state=excluded.state,
  progress_percent=excluded.progress_percent,
  error=excluded.error,
  updated_at=excluded.updated_at;
)SQL");
	if (!statement.valid()) {
		return detail::failure(database_.handle(),
				       "could not prepare export record");
	}
	auto *query = statement.get();
	const bool bound =
		detail::bindText(query, 1, job.id) &&
		detail::bindText(query, 2, job.request.clipId) &&
		detail::bindText(query, 3, job.request.sourcePath.u8string()) &&
		detail::bindText(query, 4, job.outputPath.u8string()) &&
		detail::bindText(query, 5, toString(job.orientation)) &&
		detail::bindText(query, 6, toString(job.request.preset)) &&
		detail::bindText(query, 7, toString(job.state)) &&
		sqlite3_bind_int(query, 8, job.progressPercent) == SQLITE_OK &&
		detail::bindText(query, 9, job.error) &&
		detail::bindText(query, 10, detail::formatTime(job.createdAt)) &&
		detail::bindText(query, 11, detail::formatTime(job.updatedAt));
	if (!bound || sqlite3_step(query) != SQLITE_DONE) {
		return detail::failure(database_.handle(),
				       "could not record export");
	}
	return StorageStatus::ok();
}

RepositoryResult<std::optional<ExportJob>>
ExportJobRepository::findById(const std::string &id) const
{
	const auto sql = std::string("SELECT ") + kColumns +
			 " FROM exports WHERE id=?1 LIMIT 1;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!database_.isOpen() || !statement.valid() ||
	    !detail::bindText(statement.get(), 1, id)) {
		return RepositoryResult<std::optional<ExportJob>>::fail(
			"could not prepare export query");
	}
	const auto result = sqlite3_step(statement.get());
	if (result == SQLITE_DONE) {
		return RepositoryResult<std::optional<ExportJob>>::ok(std::nullopt);
	}
	if (result != SQLITE_ROW) {
		return RepositoryResult<std::optional<ExportJob>>::fail(
			"could not query export");
	}
	auto job = readJob(statement.get());
	return job.success
		       ? RepositoryResult<std::optional<ExportJob>>::ok(
				 std::move(job.value))
		       : RepositoryResult<std::optional<ExportJob>>::fail(job.error);
}

RepositoryResult<std::vector<ExportJob>>
ExportJobRepository::listByClip(const std::string &clipId) const
{
	const auto sql = std::string("SELECT ") + kColumns +
			 " FROM exports WHERE clip_id=?1 ORDER BY created_at, id;";
	detail::Statement statement(database_.handle(), sql.c_str());
	if (!database_.isOpen() || !statement.valid() ||
	    !detail::bindText(statement.get(), 1, clipId)) {
		return RepositoryResult<std::vector<ExportJob>>::fail(
			"could not prepare clip exports query");
	}
	std::vector<ExportJob> jobs;
	int result = SQLITE_ROW;
	while ((result = sqlite3_step(statement.get())) == SQLITE_ROW) {
		auto job = readJob(statement.get());
		if (!job.success) {
			return RepositoryResult<std::vector<ExportJob>>::fail(
				job.error);
		}
		jobs.push_back(std::move(job.value));
	}
	return result == SQLITE_DONE
		       ? RepositoryResult<std::vector<ExportJob>>::ok(
				 std::move(jobs))
		       : RepositoryResult<std::vector<ExportJob>>::fail(
				 "could not list clip exports");
}

} // namespace clipcoach::storage

