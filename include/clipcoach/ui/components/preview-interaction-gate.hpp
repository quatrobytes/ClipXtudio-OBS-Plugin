#pragma once

namespace clipcoach::ui {

class PreviewInteractionGate final {
public:
	void activate() noexcept { active_ = true; }
	void deactivate() noexcept { active_ = false; }

	[[nodiscard]] bool isActive() const noexcept { return active_; }
	[[nodiscard]] bool acceptsWheel() const noexcept { return active_; }

private:
	bool active_{false};
};

} // namespace clipcoach::ui
