#include <clipcoach/remote/remote-command-executor.hpp>

#include "test-support.hpp"

#include <array>
#include <optional>
#include <vector>

using namespace clipcoach::remote;

namespace {
constexpr auto kUuid = "123e4567-e89b-12d3-a456-426614174000";
}

int main()
{
	using clipcoach::test::expect;
	std::vector<RemoteCapturePlan> captures;
	std::vector<RemoteCommandResult> results;
	RemoteCommandExecutor executor(
		[&](const RemoteCapturePlan &plan, RemoteCommandExecutor::Completion done) {
			captures.push_back(plan);
			done({plan.commandUuid, true, "clip-id", "clip.mp4", plan.durationSeconds,
			      plan.vertical ? "vertical" : plan.both ? "both" : "horizontal", "Clip saved"});
		});

	RemoteCommand vertical{kUuid, RemoteCommandType::SaveVertical, 60, 10, {}, "editor@example.com"};
	executor.submit(vertical, [&](auto result) { results.push_back(std::move(result)); });
	expect(captures.size() == 1 && captures.front().vertical, "save_vertical must use the vertical path");
	expect(captures.front().durationSeconds == 60 && captures.front().replayWindowSeconds == 70,
	       "delay compensation must extend the Replay Buffer source window");
	expect(captures.front().requestedBy == "editor@example.com",
	       "remote requester identity must be preserved in the capture plan");
	expect(results.size() == 1 && results.front().success, "successful remote capture must complete");

	const std::array<int, 5> delays{0, 5, 10, 20, 37};
	const std::array<const char *, 5> delayUuids{
		"523e4567-e89b-12d3-a456-426614174000", "623e4567-e89b-12d3-a456-426614174000",
		"723e4567-e89b-12d3-a456-426614174000", "823e4567-e89b-12d3-a456-426614174000",
		"923e4567-e89b-12d3-a456-426614174000"};
	for (std::size_t index = 0; index < delays.size(); ++index) {
		RemoteCommand delayed{delayUuids[index], RemoteCommandType::SaveCustom, 40, delays[index]};
		executor.submit(delayed, [&](auto result) { results.push_back(std::move(result)); });
		expect(captures.back().delayCompensationSeconds == delays[index],
		       "preset and custom delay must be preserved in the capture plan");
		expect(captures.back().replayWindowSeconds == 40 + delays[index],
		       "delay must extend the source window for every supported value");
	}

	RemoteCommand clip30{"a23e4567-e89b-12d3-a456-426614174000", RemoteCommandType::SaveClip30, 99, 5};
	executor.submit(clip30, [&](auto result) { results.push_back(std::move(result)); });
	expect(captures.back().durationSeconds == 30 && !captures.back().vertical && !captures.back().both,
	       "save_clip_30 must force a 30-second horizontal plan");
	RemoteCommand clip60{"b23e4567-e89b-12d3-a456-426614174000", RemoteCommandType::SaveClip60, 30, 5};
	executor.submit(clip60, [&](auto result) { results.push_back(std::move(result)); });
	expect(captures.back().durationSeconds == 60 && !captures.back().vertical && !captures.back().both,
	       "save_clip_60 must force a 60-second horizontal plan");
	RemoteCommand both{"c23e4567-e89b-12d3-a456-426614174000", RemoteCommandType::SaveBoth, 60, 20};
	executor.submit(both, [&](auto result) { results.push_back(std::move(result)); });
	expect(captures.back().both && captures.back().replayWindowSeconds == 80,
	       "save_both must request both orientations with delay compensation");

	executor.submit(vertical, [&](auto result) { results.push_back(std::move(result)); });
	expect(captures.size() == 9, "duplicate commands must never execute twice");
	expect(results.back().errorCode == "DUPLICATE_COMMAND", "duplicate command must be identified safely");

	RemoteCommand unknown{"223e4567-e89b-12d3-a456-426614174000", RemoteCommandType::Unknown, 60, 0};
	executor.submit(unknown, [&](auto result) { results.push_back(std::move(result)); });
	expect(results.back().errorCode == "UNSUPPORTED_COMMAND", "unknown commands must be rejected");

	RemoteCommand invalidDelay{"323e4567-e89b-12d3-a456-426614174000", RemoteCommandType::SaveClip30, 30, 121};
	executor.submit(invalidDelay, [&](auto result) { results.push_back(std::move(result)); });
	expect(results.back().errorCode == "INVALID_DELAY", "out-of-range delay must be rejected");
	RemoteCommand invalidMarkDelay{"d23e4567-e89b-12d3-a456-426614174000", RemoteCommandType::MarkMoment, 0, 121};
	executor.submit(invalidMarkDelay, [&](auto result) { results.push_back(std::move(result)); });
	expect(results.back().errorCode == "INVALID_DELAY", "mark moment must reject an out-of-range delay");

	bool markCalled = false;
	RemoteCommandExecutor marker({}, [&](const RemoteCommand &command, auto done) {
		markCalled = true;
		done({command.uuid, true, {}, {}, 0, {}, "Moment marked"});
	});
	RemoteCommand mark{"423e4567-e89b-12d3-a456-426614174000", RemoteCommandType::MarkMoment, 0, 20};
	marker.submit(mark, [&](auto result) { expect(result.success, "mark moment must complete"); });
	expect(markCalled, "mark moment must use only its allowlisted action");

	return clipcoach::test::pass("remote-command-executor-test");
}
