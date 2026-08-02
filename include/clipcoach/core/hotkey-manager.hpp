#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace clipcoach {

enum class HotkeyAction {
	MarkMoment,
	Save15Seconds,
	Save30Seconds,
	Save60Seconds,
	Save2Minutes,
	Save5Minutes,
	SaveVertical,
	CycleOutputMode,
	ToggleDock,
};

using HotkeyHandle = std::uint64_t;

struct HotkeyActionResult {
	bool success{false};
	std::string error;

	[[nodiscard]] static HotkeyActionResult ok() { return {true, {}}; }
	[[nodiscard]] static HotkeyActionResult fail(std::string message)
	{
		return {false, std::move(message)};
	}
};

struct HotkeyRegistration {
	bool success{false};
	HotkeyHandle handle{0};
	std::string error;
};

class HotkeyRegistrar {
public:
	using Callback = std::function<void(bool pressed)>;

	virtual ~HotkeyRegistrar() = default;
	[[nodiscard]] virtual HotkeyRegistration
	registerHotkey(std::string name, std::string description,
		       Callback callback) = 0;
	virtual void unregisterHotkey(HotkeyHandle handle) noexcept = 0;
};

struct HotkeyServices {
	std::function<int()> defaultDurationSeconds;
	std::function<HotkeyActionResult(int seconds)> capture;
	std::function<HotkeyActionResult()> captureVertical;
	std::function<HotkeyActionResult()> cycleOutputMode;
	std::function<HotkeyActionResult()> toggleDock;
};

class HotkeyManager final {
public:
	using DescriptionProvider =
		std::function<std::string(HotkeyAction action)>;
	using ErrorCallback =
		std::function<void(HotkeyAction action, const std::string &error)>;

	HotkeyManager(HotkeyRegistrar &registrar, HotkeyServices services,
		      DescriptionProvider descriptionProvider = {});
	~HotkeyManager();

	HotkeyManager(const HotkeyManager &) = delete;
	HotkeyManager &operator=(const HotkeyManager &) = delete;

	[[nodiscard]] bool registerAll();
	void unregisterAll() noexcept;
	[[nodiscard]] HotkeyActionResult execute(HotkeyAction action,
						 bool pressed = true);
	void setErrorCallback(ErrorCallback callback);
	[[nodiscard]] bool registered() const noexcept;

private:
	struct ActiveRegistration {
		HotkeyAction action;
		HotkeyHandle handle;
	};

	[[nodiscard]] HotkeyActionResult executePressed(HotkeyAction action);
	[[nodiscard]] HotkeyActionResult capture(int seconds);
	void reportFailure(HotkeyAction action,
			   const HotkeyActionResult &result) const;

	HotkeyRegistrar &registrar_;
	HotkeyServices services_;
	DescriptionProvider descriptionProvider_;
	ErrorCallback errorCallback_;
	std::vector<ActiveRegistration> registrations_;
};

} // namespace clipcoach
