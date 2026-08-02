#include "test-support.hpp"

#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <chrono>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

namespace {

class MockBackend final : public clipcoach::ExportBackend {
public:
	clipcoach::ExportBackendResult execute(const clipcoach::ExportJob &job,
					       const std::filesystem::path &temporaryPath, ProgressCallback progress,
					       const std::atomic_bool &cancelRequested) override
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			orientations.push_back(job.orientation);
			verticalDimensions.emplace_back(
				job.request.verticalWidth, job.request.verticalHeight);
		}
		progress(25);
		if (fail) {
			return clipcoach::ExportBackendResult::fail("encoder fixture failed");
		}
		if (slow) {
			for (int index = 0; index < 40; ++index) {
				if (cancelRequested.load()) {
					return clipcoach::ExportBackendResult::cancelledResult();
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
		}
		std::ofstream output(temporaryPath, std::ios::binary);
		output << "mp4";
		progress(90);
		return clipcoach::ExportBackendResult::ok();
	}

	bool fail{false};
	bool slow{false};
	std::mutex mutex;
	std::vector<clipcoach::ExportOrientation> orientations;
	std::vector<std::pair<int, int>> verticalDimensions;
};

clipcoach::ExportRequest request(const std::filesystem::path &directory, const std::string &id)
{
	clipcoach::ExportRequest result;
	result.clipId = id;
	result.sourcePath = directory / (id + ".mkv");
	result.outputDirectory = directory;
	result.outputBaseName = "My clip: clutch!";
	result.durationSeconds = 30;
	result.orientation = clipcoach::ExportOrientation::Vertical;
	result.outputFps = 60;
	return result;
}

} // namespace

int main()
{
	using namespace clipcoach;
	using clipcoach::test::expect;
	const auto directory =
		std::filesystem::temp_directory_path() /
		("clipcoach-export-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);

	expect(ExportManager::cleanBaseName("  My clip: clutch!  ") == "My_clip_clutch",
	       "export file name must be filesystem-safe and readable");
	auto firstPath = ExportManager::createAvailableOutputPath(directory, "My clip", ExportOrientation::Vertical);
	expect(firstPath.filename() == "My_clip_vertical.mp4",
	       "vertical output path must have a clean orientation suffix");
	{
		std::ofstream existing(firstPath);
		existing << "existing";
	}
	auto collisionPath =
		ExportManager::createAvailableOutputPath(directory, "My clip", ExportOrientation::Vertical);
	expect(collisionPath.filename() == "My_clip_vertical_2.mp4",
	       "an existing export must produce a non-overwriting suffix");
	expect(ExportManager::isValidPreset(ExportQualityPreset::Low) &&
		       ExportManager::isValidPreset(ExportQualityPreset::Maximum) &&
		       !ExportManager::isValidPreset(static_cast<ExportQualityPreset>(99)),
	       "quality presets must reject values outside Low..Maximum");

	FeatureGateService freeGates;
	auto gatedBackend = std::make_unique<MockBackend>();
	ExportManager gatedExports(std::move(gatedBackend), ExportManager::JobCallback{}, ExportManager::LogCallback{},
				   &freeGates);
	std::string gateError;
	auto verticalCanvas = request(directory, "clip-gated-canvas");
	verticalCanvas.verticalSource = VerticalExportSource::VerticalCanvas;
	verticalCanvas.verticalCanvasPath = directory / "canvas.mp4";
	expect(gatedExports.enqueue(verticalCanvas, &gateError).empty() && gateError.find("Pro") != std::string::npos,
	       "Free must not bypass Vertical Canvas through ExportManager");
	gateError.clear();
	expect(gatedExports.enqueueBatch({request(directory, "clip-gated-batch")}, &gateError).empty() &&
		       gateError.find("Pro") != std::string::npos,
	       "Free must not bypass batch export through ExportManager");
	auto limitedVertical = gatedExports.enqueue(request(directory, "clip-free-vertical"), &gateError);
	expect(limitedVertical.size() == 1, "Free must retain limited vertical export");
	expect(gatedExports.waitUntilIdle(std::chrono::seconds(2)), "Free vertical export must complete");
	freeGates.setEntitlementState(EntitlementState::ProActive);
	expect(gatedExports.enqueueBatch({request(directory, "clip-pro-batch")}, &gateError).size() == 1,
	       "Pro must allow batch export");
	expect(gatedExports.waitUntilIdle(std::chrono::seconds(2)), "Pro batch export must complete");

	auto backend = std::make_unique<MockBackend>();
	auto *backendProbe = backend.get();
	std::vector<ExportJobState> states;
	std::mutex statesMutex;
	ExportManager manager(std::move(backend), [&](const ExportJob &job) {
		std::lock_guard<std::mutex> lock(statesMutex);
		states.push_back(job.state);
	});
	auto eightK = request(directory, "clip-8k");
	eightK.verticalWidth = 4320;
	eightK.verticalHeight = 7680;
	const auto eightKIds = manager.enqueue(eightK);
	expect(eightKIds.size() == 1 && manager.waitUntilIdle(std::chrono::seconds(2)),
	       "8K vertical export must pass validation and reach the backend");
	{
		std::lock_guard<std::mutex> lock(backendProbe->mutex);
		expect(std::find(backendProbe->verticalDimensions.begin(),
				 backendProbe->verticalDimensions.end(),
				 std::pair<int, int>{4320, 7680}) !=
			       backendProbe->verticalDimensions.end(),
		       "export backend must receive the persisted 4320x7680 target");
	}
	auto single = manager.enqueue(request(directory, "clip-1"));
	expect(single.size() == 1, "single export must enqueue one job");
	expect(manager.waitUntilIdle(std::chrono::seconds(2)), "single export fixture must finish");
	const auto completed = manager.job(single.front());
	expect(completed.state == ExportJobState::Done && completed.progressPercent == 100 &&
		       std::filesystem::exists(completed.outputPath),
	       "pending export must transition through exporting to done");
	{
		std::lock_guard<std::mutex> lock(statesMutex);
		expect(std::find(states.begin(), states.end(), ExportJobState::Pending) != states.end() &&
			       std::find(states.begin(), states.end(), ExportJobState::Exporting) != states.end() &&
			       std::find(states.begin(), states.end(), ExportJobState::Done) != states.end(),
		       "callbacks must expose pending, exporting and done");
	}

	std::vector<ExportRequest> batch;
	auto both = request(directory, "clip-2");
	both.orientation = ExportOrientation::Both;
	batch.push_back(both);
	batch.push_back(request(directory, "clip-3"));
	const auto batchIds = manager.enqueueBatch(std::move(batch));
	expect(batchIds.size() == 3, "batch queue must expand Both into horizontal and vertical jobs");
	expect(manager.waitUntilIdle(std::chrono::seconds(2)), "batch fixture must drain");
	expect(manager.job(batchIds[0]).state == ExportJobState::Done &&
		       manager.job(batchIds[1]).state == ExportJobState::Done &&
		       manager.job(batchIds[2]).state == ExportJobState::Done,
	       "every batch job must complete independently");

	auto failingBackend = std::make_unique<MockBackend>();
	failingBackend->fail = true;
	ExportManager failing(std::move(failingBackend));
	auto failureId = failing.enqueue(request(directory, "clip-fail")).front();
	expect(failing.waitUntilIdle(std::chrono::seconds(2)), "failure fixture must finish");
	expect(failing.job(failureId).state == ExportJobState::Error &&
		       failing.job(failureId).error == "encoder fixture failed",
	       "backend errors must become understandable error states");

	auto slowBackend = std::make_unique<MockBackend>();
	slowBackend->slow = true;
	ExportManager cancellable(std::move(slowBackend));
	auto cancelId = cancellable.enqueue(request(directory, "clip-cancel")).front();
	std::this_thread::sleep_for(std::chrono::milliseconds(10));
	expect(cancellable.cancel(cancelId), "active export must accept cancel");
	expect(cancellable.waitUntilIdle(std::chrono::seconds(2)) &&
		       cancellable.job(cancelId).state == ExportJobState::Cancelled,
	       "cancelled export must reach cancelled state");

	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);
	return clipcoach::test::pass("export-manager-test");
}
