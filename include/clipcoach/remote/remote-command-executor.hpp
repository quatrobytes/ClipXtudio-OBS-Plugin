#pragma once

#include <clipcoach/remote/remote-clipper-types.hpp>

#include <deque>
#include <functional>
#include <set>

namespace clipcoach::remote {

class RemoteCommandExecutor final {
public:
	using Completion = std::function<void(RemoteCommandResult)>;
	using CaptureAction = std::function<void(const RemoteCapturePlan &, Completion)>;
	using MarkMomentAction = std::function<void(const RemoteCommand &, Completion)>;
	using ProcessedCallback = std::function<void(const std::string &)>;

	RemoteCommandExecutor(CaptureAction captureAction, MarkMomentAction markMomentAction = {},
			      ProcessedCallback processedCallback = {});

	void submit(RemoteCommand command, Completion completion);
	void preloadProcessed(std::set<std::string> commandUuids);
	[[nodiscard]] bool busy() const noexcept;
	[[nodiscard]] std::size_t queuedCount() const noexcept;

	[[nodiscard]] static std::optional<RemoteCapturePlan> planFor(const RemoteCommand &command,
								std::string *errorCode = nullptr,
								std::string *errorMessage = nullptr);

private:
	struct Pending {
		RemoteCommand command;
		Completion completion;
	};

	void executeNext();
	void finish(RemoteCommandResult result, Completion completion);

	CaptureAction captureAction_;
	MarkMomentAction markMomentAction_;
	ProcessedCallback processedCallback_;
	std::deque<Pending> queue_;
	std::set<std::string> processed_;
	bool busy_{false};
};

} // namespace clipcoach::remote
