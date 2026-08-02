#pragma once

#include <clipcoach/core/trigger-types.hpp>

namespace clipcoach {

class ScoreEngine final {
public:
	[[nodiscard]] int calculate(const TriggerSignal &signal) const noexcept;
	[[nodiscard]] int calculate(const std::vector<TriggerSignal> &inputSignals) const noexcept;

	[[nodiscard]] static double durationQuality(int durationSeconds) noexcept;
};

} // namespace clipcoach
