#include <clipcoach/remote/remote-command-executor.hpp>

#include <algorithm>
#include <cctype>

namespace clipcoach::remote {

RemoteCommandType remoteCommandTypeFromString(const std::string &value) noexcept
{
	if (value == "mark_moment")
		return RemoteCommandType::MarkMoment;
	if (value == "save_clip_30")
		return RemoteCommandType::SaveClip30;
	if (value == "save_clip_60")
		return RemoteCommandType::SaveClip60;
	if (value == "save_custom")
		return RemoteCommandType::SaveCustom;
	if (value == "save_vertical")
		return RemoteCommandType::SaveVertical;
	if (value == "save_both")
		return RemoteCommandType::SaveBoth;
	return RemoteCommandType::Unknown;
}

const char *remoteCommandTypeName(RemoteCommandType type) noexcept
{
	switch (type) {
	case RemoteCommandType::MarkMoment:
		return "mark_moment";
	case RemoteCommandType::SaveClip30:
		return "save_clip_30";
	case RemoteCommandType::SaveClip60:
		return "save_clip_60";
	case RemoteCommandType::SaveCustom:
		return "save_custom";
	case RemoteCommandType::SaveVertical:
		return "save_vertical";
	case RemoteCommandType::SaveBoth:
		return "save_both";
	case RemoteCommandType::Unknown:
		return "unknown";
	}
	return "unknown";
}

bool isValidCommandUuid(const std::string &value) noexcept
{
	if (value.size() != 36 || value[8] != '-' || value[13] != '-' || value[18] != '-' || value[23] != '-')
		return false;
	for (std::size_t index = 0; index < value.size(); ++index) {
		if (index == 8 || index == 13 || index == 18 || index == 23)
			continue;
		if (std::isxdigit(static_cast<unsigned char>(value[index])) == 0)
			return false;
	}
	return true;
}

RemoteCommandExecutor::RemoteCommandExecutor(CaptureAction captureAction, MarkMomentAction markMomentAction,
					     ProcessedCallback processedCallback)
	: captureAction_(std::move(captureAction)),
	  markMomentAction_(std::move(markMomentAction)),
	  processedCallback_(std::move(processedCallback))
{
}

void RemoteCommandExecutor::submit(RemoteCommand command, Completion completion)
{
	if (!isValidCommandUuid(command.uuid)) {
		completion({command.uuid,
			    false,
			    {},
			    {},
			    0,
			    {},
			    {},
			    "INVALID_COMMAND_UUID",
			    "Remote command UUID is invalid"});
		return;
	}
	if (processed_.count(command.uuid) != 0 ||
	    std::any_of(queue_.begin(), queue_.end(),
			[&command](const Pending &pending) { return pending.command.uuid == command.uuid; })) {
		completion({command.uuid,
			    false,
			    {},
			    {},
			    0,
			    {},
			    {},
			    "DUPLICATE_COMMAND",
			    "Remote command was already handled"});
		return;
	}
	queue_.push_back({std::move(command), std::move(completion)});
	executeNext();
}

void RemoteCommandExecutor::preloadProcessed(std::set<std::string> commandUuids)
{
	processed_ = std::move(commandUuids);
}

bool RemoteCommandExecutor::busy() const noexcept
{
	return busy_;
}
std::size_t RemoteCommandExecutor::queuedCount() const noexcept
{
	return queue_.size();
}

std::optional<RemoteCapturePlan> RemoteCommandExecutor::planFor(const RemoteCommand &command, std::string *errorCode,
								std::string *errorMessage)
{
	auto fail = [&](const char *code, const char *message) -> std::optional<RemoteCapturePlan> {
		if (errorCode)
			*errorCode = code;
		if (errorMessage)
			*errorMessage = message;
		return std::nullopt;
	};
	if (command.type == RemoteCommandType::Unknown)
		return fail("UNSUPPORTED_COMMAND", "Remote command type is not allowed");
	if (command.delayCompensationSeconds < 0 || command.delayCompensationSeconds > 120)
		return fail("INVALID_DELAY", "Delay compensation must be between 0 and 120 seconds");
	if (command.type == RemoteCommandType::MarkMoment)
		return RemoteCapturePlan{
			command.uuid, command.type,       0, command.delayCompensationSeconds, 0, false, false,
			command.note, command.requestedBy};

	int duration = command.durationSeconds;
	if (command.type == RemoteCommandType::SaveClip30)
		duration = 30;
	if (command.type == RemoteCommandType::SaveClip60)
		duration = 60;
	if ((command.type == RemoteCommandType::SaveVertical || command.type == RemoteCommandType::SaveBoth) &&
	    duration == 0)
		duration = 60;
	if (duration < 5 || duration > 120)
		return fail("INVALID_DURATION", "Remote clip duration must be between 5 and 120 seconds");
	return RemoteCapturePlan{command.uuid,
				 command.type,
				 duration,
				 command.delayCompensationSeconds,
				 duration + command.delayCompensationSeconds,
				 command.type == RemoteCommandType::SaveVertical,
				 command.type == RemoteCommandType::SaveBoth,
				 command.note,
				 command.requestedBy};
}

void RemoteCommandExecutor::executeNext()
{
	if (busy_ || queue_.empty())
		return;
	busy_ = true;
	auto pending = std::move(queue_.front());
	queue_.pop_front();
	std::string code;
	std::string message;
	auto plan = planFor(pending.command, &code, &message);
	if (!plan) {
		finish({pending.command.uuid, false, {}, {}, 0, {}, {}, std::move(code), std::move(message)},
		       std::move(pending.completion));
		return;
	}
	auto done = [this, completion = std::move(pending.completion)](RemoteCommandResult result) mutable {
		finish(std::move(result), std::move(completion));
	};
	if (pending.command.type == RemoteCommandType::MarkMoment) {
		if (!markMomentAction_) {
			done({pending.command.uuid,
			      false,
			      {},
			      {},
			      0,
			      {},
			      {},
			      "MARK_MOMENT_UNAVAILABLE",
			      "Mark moment service is unavailable"});
			return;
		}
		markMomentAction_(pending.command, std::move(done));
		return;
	}
	if (!captureAction_) {
		done({pending.command.uuid,
		      false,
		      {},
		      {},
		      0,
		      {},
		      {},
		      "CAPTURE_UNAVAILABLE",
		      "Capture service is unavailable"});
		return;
	}
	captureAction_(*plan, std::move(done));
}

void RemoteCommandExecutor::finish(RemoteCommandResult result, Completion completion)
{
	processed_.insert(result.commandUuid);
	if (processedCallback_)
		processedCallback_(result.commandUuid);
	busy_ = false;
	if (completion)
		completion(std::move(result));
	executeNext();
}

} // namespace clipcoach::remote
