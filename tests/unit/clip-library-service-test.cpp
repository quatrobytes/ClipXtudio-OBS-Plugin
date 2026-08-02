#include "storage-test-support.hpp"
#include "test-support.hpp"

#include <clipcoach/storage/clip-library-service.hpp>

#include <algorithm>
#include <future>
#include <fstream>
#include <thread>

int main()
{
	clipcoach::test::TemporaryDatabase temporary;
	const auto callerThread = std::this_thread::get_id();
	clipcoach::storage::ClipLibraryService service(temporary.path());

	std::promise<clipcoach::storage::StorageStatus> sessionPromise;
	service.storeSession(clipcoach::test::sampleSession(),
			     [&sessionPromise](auto status) {
				     sessionPromise.set_value(std::move(status));
			     });
	clipcoach::test::expect(sessionPromise.get_future().get().success,
			       "async session insert must succeed");

	auto clip = clipcoach::test::sampleClip();
	const auto assetDirectory = temporary.path().parent_path();
	clip.filePath = assetDirectory / "clip-test-001.mkv";
	clip.thumbnailPath = assetDirectory / "service-thumbnail.jpg";
	clip.transcriptPath = assetDirectory / "service-transcript.txt";
	clip.subtitlePath = assetDirectory / "service-subtitles.srt";
	for (const auto &path :
	     {clip.filePath, clip.thumbnailPath, clip.transcriptPath, clip.subtitlePath}) {
		std::ofstream asset(path, std::ios::binary);
		asset << "test";
	}
	std::promise<clipcoach::storage::StorageStatus> clipPromise;
	service.storeClip(clip, [&clipPromise](auto status) {
		clipPromise.set_value(std::move(status));
	});
	clipcoach::test::expect(clipPromise.get_future().get().success,
			       "async clip insert must succeed");
	std::promise<clipcoach::storage::StorageStatus> thumbnailPromise;
	service.updateThumbnail(
		"clip-test-001", clip.thumbnailPath,
		[&thumbnailPromise](auto status) {
			thumbnailPromise.set_value(std::move(status));
		});
	clipcoach::test::expect(
		thumbnailPromise.get_future().get().success,
		"async thumbnail update must succeed");

	struct QueryOutcome {
		clipcoach::storage::RepositoryResult<std::vector<clipcoach::ClipMetadata>> result;
		std::thread::id callbackThread;
	};
	std::promise<QueryOutcome> queryPromise;
	service.listRecentAsync(100, [&queryPromise](auto result) {
		queryPromise.set_value({std::move(result), std::this_thread::get_id()});
	});
	const auto query = queryPromise.get_future().get();
	clipcoach::test::expect(query.result.success && query.result.value.size() == 1,
			       "async library query must reconstruct the stored clip");
	clipcoach::test::expect(
		query.result.value.front().thumbnailPath.filename() ==
			"service-thumbnail.jpg",
		"async library query must return the generated thumbnail path");
	clipcoach::test::expect(query.callbackThread != callerThread,
			       "database callback must execute outside the caller/UI thread");

	std::promise<clipcoach::storage::RepositoryResult<
		clipcoach::storage::ClipLibrarySnapshot>>
		snapshotPromise;
	service.loadSnapshotAsync(
		"session-test-001", 100,
		[&snapshotPromise](auto result) {
			snapshotPromise.set_value(std::move(result));
		});
	const auto snapshot = snapshotPromise.get_future().get();
	clipcoach::test::expect(
		snapshot.success && snapshot.value.clips.size() == 1 &&
			snapshot.value.session.has_value(),
		"snapshot query must load clips and session on the worker");

	const auto verticalPath = assetDirectory / "clip-test-001_vertical.mp4";
	{
		std::ofstream verticalAsset(verticalPath, std::ios::binary);
		verticalAsset << "vertical";
	}
	clipcoach::ExportJob verticalJob;
	verticalJob.id = "export-vertical-001";
	verticalJob.request.clipId = clip.id;
	verticalJob.request.sourcePath = clip.filePath;
	verticalJob.request.durationSeconds = 30;
	verticalJob.orientation = clipcoach::ExportOrientation::Vertical;
	verticalJob.state = clipcoach::ExportJobState::Done;
	verticalJob.progressPercent = 100;
	verticalJob.outputPath = verticalPath;
	verticalJob.createdAt = std::chrono::system_clock::now();
	verticalJob.updatedAt = verticalJob.createdAt;
	std::promise<clipcoach::storage::StorageStatus> exportPromise;
	service.storeExportJob(verticalJob, [&exportPromise](auto status) {
		exportPromise.set_value(std::move(status));
	});
	clipcoach::test::expect(exportPromise.get_future().get().success,
		"a completed vertical export must persist as a selectable library variant");
	std::promise<clipcoach::storage::RepositoryResult<std::vector<clipcoach::ClipMetadata>>>
		variantsPromise;
	service.listRecentAsync(100, [&variantsPromise](auto result) {
		variantsPromise.set_value(std::move(result));
	});
	const auto variants = variantsPromise.get_future().get();
	const auto verticalVariant = std::find_if(
		variants.value.begin(), variants.value.end(), [](const auto &item) {
			return item.orientation == clipcoach::ClipOrientation::Vertical;
		});
	clipcoach::test::expect(
		variants.success && variants.value.size() == 2 &&
			verticalVariant != variants.value.end() &&
			verticalVariant->filePath == verticalPath &&
			verticalVariant->durationSeconds == 30,
		"the library must expose the horizontal original and its real vertical file separately");
	const auto verticalVariantId = verticalVariant->id;

	std::promise<clipcoach::storage::StorageStatus> deletePromise;
	service.deleteClips({"clip-test-001", verticalVariantId}, [&deletePromise](auto status) {
		deletePromise.set_value(std::move(status));
	});
	clipcoach::test::expect(deletePromise.get_future().get().success,
			       "async deletion must succeed");
	for (const auto &path :
	     {clip.filePath, clip.thumbnailPath, clip.transcriptPath, clip.subtitlePath,
	      verticalPath}) {
		clipcoach::test::expect(!std::filesystem::exists(path),
				       "deletion must remove every local clip asset");
	}
	std::promise<clipcoach::storage::RepositoryResult<std::vector<clipcoach::ClipMetadata>>>
		deletedQueryPromise;
	service.listRecentAsync(100, [&deletedQueryPromise](auto result) {
		deletedQueryPromise.set_value(std::move(result));
	});
	const auto deletedQuery = deletedQueryPromise.get_future().get();
	clipcoach::test::expect(deletedQuery.success && deletedQuery.value.empty(),
			       "deletion must remove clip metadata from SQLite");

	clipcoach::test::expect(service.flush().success,
			       "service must flush all queued storage work");
	return clipcoach::test::pass("clip-library-service-test");
}
