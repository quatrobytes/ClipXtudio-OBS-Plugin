#pragma once

#include <filesystem>
#include <functional>
#include <limits>
#include <string>

namespace clipcoach {

enum class ReplayState {
	Inactive,
	Starting,
	Active,
	Stopping,
	Error,
};

struct ReplayOperationResult {
	bool accepted{false};
	std::string errorMessage;
};

class ReplayManager {
public:
	using StateChangedCallback = std::function<void(ReplayState)>;
	using ReplaySavedCallback = std::function<void(const std::filesystem::path &)>;

	virtual ~ReplayManager() = default;

	[[nodiscard]] virtual ReplayState state() const noexcept = 0;
	[[nodiscard]] virtual int bufferedDurationSeconds() const noexcept { return std::numeric_limits<int>::max(); }
	virtual ReplayOperationResult start() = 0;
	virtual ReplayOperationResult stop() = 0;
	virtual ReplayOperationResult save() = 0;
	virtual void setStateChangedCallback(StateChangedCallback callback) = 0;
	virtual void setReplaySavedCallback(ReplaySavedCallback callback) = 0;
};

} // namespace clipcoach
