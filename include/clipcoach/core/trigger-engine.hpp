#pragma once

#include <clipcoach/core/score-engine.hpp>

#include <map>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>

namespace clipcoach {

enum class TriggerRejection {
	None,
	Disabled,
	ProRequired,
	BelowThreshold,
	NotConfigured,
	Duplicate,
	Cooldown,
	InvalidSignal,
};

struct TriggerResult {
	std::optional<TriggerEvent> event;
	TriggerRejection rejection{TriggerRejection::None};
};

class TriggerEngine final {
public:
	using EventCallback = std::function<void(const TriggerEvent &)>;

	explicit TriggerEngine(bool proUnlocked = false);

	[[nodiscard]] bool
	setConfiguration(SmartTriggerType type,
			 const TriggerConfiguration &configuration,
			 std::string *error = nullptr);
	[[nodiscard]] TriggerConfiguration
	configuration(SmartTriggerType type) const;

	void setProUnlocked(bool unlocked) noexcept;
	[[nodiscard]] bool proUnlocked() const noexcept;
	[[nodiscard]] static bool
	requiresPro(SmartTriggerType type) noexcept;

	[[nodiscard]] TriggerResult process(const TriggerSignal &signal);
	[[nodiscard]] TriggerResult
	evaluateMoment(const std::vector<TriggerSignal> &inputSignals);
	[[nodiscard]] std::vector<TriggerEvent>
	recentEvents(std::size_t limit = 20) const;
	void clearRecentEvents();
	void setEventCallback(EventCallback callback);

private:
	[[nodiscard]] bool isEligible(const TriggerSignal &signal,
				      const TriggerConfiguration &config,
				      TriggerRejection &rejection) const;
	[[nodiscard]] TriggerResult
	evaluateLocked(const std::vector<TriggerSignal> &inputSignals);

	mutable std::mutex mutex_;
	std::map<SmartTriggerType, TriggerConfiguration> configurations_;
	std::vector<TriggerEvent> recentEvents_;
	std::optional<std::chrono::system_clock::time_point> lastEventAt_;
	std::optional<std::chrono::system_clock::time_point> lastClipAt_;
	ScoreEngine scoreEngine_;
	EventCallback eventCallback_;
	bool proUnlocked_{false};
	std::uint64_t nextEventId_{1};
};

} // namespace clipcoach
