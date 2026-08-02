#include "storage-test-support.hpp"
#include "test-support.hpp"

#include <clipcoach/storage/clip-repository.hpp>
#include <clipcoach/storage/export-job-repository.hpp>
#include <clipcoach/storage/session-repository.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

int main()
{
	using namespace clipcoach;
	using clipcoach::test::expect;
	clipcoach::test::TemporaryDatabase temporary;
	storage::SqliteDatabase database(temporary.path());
	expect(database.open().success, "export repository database must open");
	storage::SessionRepository sessions(database);
	storage::ClipRepository clips(database);
	const auto session = clipcoach::test::sampleSession();
	const auto clip = clipcoach::test::sampleClip();
	expect(sessions.insert(session).success, "export session must insert");
	expect(clips.insert(clip).success, "export clip must insert");

	ExportJob job;
	job.id = "export-1";
	job.request.clipId = clip.id;
	job.request.sourcePath = clip.filePath;
	job.request.preset = ExportQualityPreset::High;
	job.orientation = ExportOrientation::Vertical;
	job.state = ExportJobState::Pending;
	job.outputPath = temporary.path().parent_path() / "clip_vertical.mp4";
	job.createdAt = std::chrono::system_clock::now();
	job.updatedAt = job.createdAt;

	storage::ExportJobRepository exports(database);
	expect(exports.record(job).success, "pending export metadata must insert");
	job.state = ExportJobState::Exporting;
	job.progressPercent = 46;
	job.updatedAt += std::chrono::seconds(1);
	expect(exports.record(job).success,
	       "export progress metadata must update");
	job.state = ExportJobState::Done;
	job.progressPercent = 100;
	job.updatedAt += std::chrono::seconds(1);
	expect(exports.record(job).success, "done export metadata must update");

	const auto restored = exports.findById(job.id);
	expect(restored.success && restored.value.has_value() &&
		       restored.value->state == ExportJobState::Done &&
		       restored.value->progressPercent == 100 &&
		       restored.value->outputPath == job.outputPath,
	       "export metadata must reconstruct from SQLite");
	const auto byClip = exports.listByClip(clip.id);
	expect(byClip.success && byClip.value.size() == 1,
	       "exports must list by source clip");

	return clipcoach::test::pass("export-job-repository-test");
}

