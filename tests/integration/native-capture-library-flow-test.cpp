#include "../unit/test-support.hpp"

#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/replay-manager.hpp>
#include <clipcoach/storage/clip-library-service.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <utility>

namespace {

class MockObsReplayAdapter final : public clipcoach::ReplayManager {
public:
	[[nodiscard]] clipcoach::ReplayState state() const noexcept override { return replayState; }
	clipcoach::ReplayOperationResult start() override { return {true, {}}; }
	clipcoach::ReplayOperationResult stop() override { return {true, {}}; }
	clipcoach::ReplayOperationResult save() override
	{
		++saveCalls;
		return {true, {}};
	}
	void setStateChangedCallback(StateChangedCallback callback) override { stateChanged = std::move(callback); }
	void setReplaySavedCallback(ReplaySavedCallback callback) override { replaySaved = std::move(callback); }

	void emitSaved(const std::filesystem::path &path) const
	{
		if (replaySaved)
			replaySaved(path);
	}

	clipcoach::ReplayState replayState{clipcoach::ReplayState::Inactive};
	int saveCalls{0};
	StateChangedCallback stateChanged;
	ReplaySavedCallback replaySaved;
};

} // namespace

int main()
{
	using clipcoach::CaptureError;
	using clipcoach::ClipManager;
	using clipcoach::ReplayState;
	using clipcoach::storage::ClipLibraryService;
	using clipcoach::test::expect;

	const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
	const auto directory =
		std::filesystem::temp_directory_path() / ("clipcoach-native-flow-" + std::to_string(stamp));
	std::filesystem::create_directories(directory);

	{
		ClipLibraryService library(directory / "clipcoach.db");
		clipcoach::SessionMetadata session;
		session.id = "integration-session";
		session.startedAt = std::chrono::system_clock::now();
		library.storeSession(session);
		expect(library.flush().success, "session must reach SQLite before capture");

		MockObsReplayAdapter replay;
		ClipManager manager(replay);
		manager.setCaptureContext(session.id, "test", [] { return std::string("Gameplay"); });
		manager.setClipPersistenceCallback(
			[&library](const clipcoach::ClipMetadata &clip) { library.storeClip(clip); });

		const auto inactive = manager.captureManual(30);
		expect(!inactive.accepted && inactive.error == CaptureError::ReplayBufferInactive,
		       "inactive mock OBS adapter must return a controlled error");
		expect(replay.saveCalls == 0, "inactive capture must not call the OBS adapter");

		replay.replayState = ReplayState::Active;
		expect(manager.captureManual(30).accepted, "active mock OBS adapter must accept capture");
		expect(replay.saveCalls == 1, "capture must invoke the OBS adapter once");

		const auto replayPath = directory / "Replay.mp4";
		std::ofstream(replayPath, std::ios::binary).put('\0');
		replay.emitSaved(replayPath);
		expect(library.flush().success, "clip persistence queue must drain");

		auto promise = std::make_shared<
			std::promise<clipcoach::storage::RepositoryResult<std::vector<clipcoach::ClipMetadata>>>>();
		auto future = promise->get_future();
		library.listBySessionAsync(session.id,
					   [promise](auto result) mutable { promise->set_value(std::move(result)); });
		expect(future.wait_for(std::chrono::seconds(2)) == std::future_status::ready,
		       "async library query must complete");
		const auto listed = future.get();
		expect(listed.success && listed.value.size() == 1, "saved replay must be reconstructable from SQLite");
		expect(listed.value.front().triggerType == clipcoach::TriggerType::Manual,
		       "integrated metadata must preserve manual trigger");

		expect(library.flush().success, "library must stop with an empty queue");
	}
	std::filesystem::remove_all(directory);
	return clipcoach::test::pass("native-capture-library-flow-test");
}
