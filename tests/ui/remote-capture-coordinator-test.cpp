#include <clipcoach/plugin/remote-capture-coordinator.hpp>
#include <clipcoach/core/replay-manager.hpp>
#include <clipcoach/core/settings-manager.hpp>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <cassert>
#include <fstream>
#include <limits>

namespace {

class Replay final : public clipcoach::ReplayManager {
public:
	clipcoach::ReplayState state_{clipcoach::ReplayState::Inactive};
	ReplaySavedCallback saved;
	clipcoach::ReplayState state() const noexcept override { return state_; }
	int bufferedDurationSeconds() const noexcept override { return std::numeric_limits<int>::max(); }
	clipcoach::ReplayOperationResult start() override { return {true, {}}; }
	clipcoach::ReplayOperationResult stop() override { return {true, {}}; }
	clipcoach::ReplayOperationResult save() override
	{
		return state_ == clipcoach::ReplayState::Active ? clipcoach::ReplayOperationResult{true, {}}
								: clipcoach::ReplayOperationResult{false, "inactive"};
	}
	void setStateChangedCallback(StateChangedCallback) override {}
	void setReplaySavedCallback(ReplaySavedCallback callback) override { saved = std::move(callback); }
};

class Backend final : public clipcoach::ExportBackend {
public:
	clipcoach::ExportRequest observed;
	clipcoach::ExportBackendResult execute(const clipcoach::ExportJob &job,
					       const std::filesystem::path &temporaryPath, ProgressCallback progress,
					       const std::atomic_bool &) override
	{
		observed = job.request;
		std::ofstream(temporaryPath).put('x');
		progress(100);
		return clipcoach::ExportBackendResult::ok();
	}
};

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	const auto root = std::filesystem::temp_directory_path() / "clipxtudio-remote-coordinator-test";
	std::filesystem::create_directories(root);
	clipcoach::SettingsManager settings(root / "settings.json");
	std::string error;
	assert(settings.load(&error));

	{
		Replay replay;
		clipcoach::ClipManager clips(replay);
		auto backend = std::make_unique<Backend>();
		clipcoach::ExportManager exports(std::move(backend));
		clipcoach::plugin::RemoteCaptureCoordinator coordinator(clips, exports, settings, [](int) {});
		bool failed = false;
		clipcoach::remote::RemoteCapturePlan plan{"123e4567-e89b-12d3-a456-426614174000",
							  clipcoach::remote::RemoteCommandType::SaveClip30, 30, 0, 30};
		coordinator.capture(plan, [&](auto result) {
			failed = !result.success && result.errorCode == "REPLAY_BUFFER_INACTIVE";
		});
		assert(failed);
	}

	{
		Replay replay;
		replay.state_ = clipcoach::ReplayState::Active;
		clipcoach::ClipManager clips(replay);
		auto backend = std::make_unique<Backend>();
		auto *backendView = backend.get();
		clipcoach::ExportManager exports(std::move(backend));
		clipcoach::plugin::RemoteCaptureCoordinator coordinator(clips, exports, settings,
									[](int seconds) { assert(seconds == 70); });
		std::optional<clipcoach::remote::RemoteCommandResult> result;
		clipcoach::remote::RemoteCapturePlan plan{"223e4567-e89b-12d3-a456-426614174000",
							  clipcoach::remote::RemoteCommandType::SaveVertical,
							  60,
							  10,
							  70,
							  true,
							  false,
							  {},
							  "editor@example.com"};
		coordinator.capture(plan, [&](auto value) { result = std::move(value); });
		const auto replayFile = root / "Replay.mp4";
		std::ofstream(replayFile).put('x');
		replay.saved(replayFile);
		assert(clips.sessionClips().size() == 1);
		assert(clips.sessionClips().front().triggerLabel == "remote_clipper");
		assert(clips.sessionClips().front().requestedBy == "editor@example.com");
		QEventLoop loop;
		QTimer timer;
		timer.setInterval(10);
		QObject::connect(&timer, &QTimer::timeout, &loop, [&] {
			if (result)
				loop.quit();
		});
		QTimer::singleShot(3000, &loop, &QEventLoop::quit);
		timer.start();
		loop.exec();
		assert(result && result->success && result->orientation == "vertical");
		assert(backendView->observed.durationSeconds == 60 && backendView->observed.endOffsetSeconds == 10);
	}

	std::error_code cleanup;
	std::filesystem::remove_all(root, cleanup);
	return 0;
}
