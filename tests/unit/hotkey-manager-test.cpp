#include "test-support.hpp"

#include <clipcoach/core/hotkey-manager.hpp>

#include <map>
#include <string>
#include <vector>

namespace {

class MockRegistrar final : public clipcoach::HotkeyRegistrar {
public:
	clipcoach::HotkeyRegistration registerHotkey(std::string name, std::string description,
						     Callback callback) override
	{
		++registerCalls;
		if (failAtCall > 0 && registerCalls == failAtCall) {
			return {false, 0, "forced registration failure"};
		}
		const auto handle = nextHandle++;
		names.push_back(std::move(name));
		descriptions.push_back(std::move(description));
		callbacks.emplace(handle, std::move(callback));
		return {true, handle, {}};
	}

	void unregisterHotkey(clipcoach::HotkeyHandle handle) noexcept override
	{
		unregistered.push_back(handle);
		callbacks.erase(handle);
	}

	void press(std::size_t index, bool pressed = true) { callbacks.at(index + 1)(pressed); }

	int failAtCall{0};
	int registerCalls{0};
	clipcoach::HotkeyHandle nextHandle{1};
	std::vector<std::string> names;
	std::vector<std::string> descriptions;
	std::vector<clipcoach::HotkeyHandle> unregistered;
	std::map<clipcoach::HotkeyHandle, Callback> callbacks;
};

} // namespace

int main()
{
	using clipcoach::HotkeyActionResult;
	using clipcoach::test::expect;

	MockRegistrar registrar;
	std::vector<int> captureDurations;
	int verticalCalls = 0;
	int outputCalls = 0;
	int dockCalls = 0;
	int errorCalls = 0;

	clipcoach::HotkeyServices services;
	services.defaultDurationSeconds = [] {
		return 45;
	};
	services.capture = [&captureDurations](int seconds) {
		captureDurations.push_back(seconds);
		return HotkeyActionResult::ok();
	};
	services.captureVertical = [&verticalCalls] {
		++verticalCalls;
		return HotkeyActionResult::ok();
	};
	services.cycleOutputMode = [&outputCalls] {
		++outputCalls;
		return HotkeyActionResult::ok();
	};
	services.toggleDock = [&dockCalls] {
		++dockCalls;
		return HotkeyActionResult::ok();
	};

	clipcoach::HotkeyManager manager(registrar, std::move(services));
	manager.setErrorCallback([&errorCalls](clipcoach::HotkeyAction, const std::string &) { ++errorCalls; });
	expect(manager.registerAll(), "all hotkeys must register");
	expect(registrar.registerCalls == 9 && registrar.callbacks.size() == 9,
	       "exactly nine native actions must register");
	expect(registrar.names == std::vector<std::string>({
					  "clipcoach.mark_moment",
					  "clipcoach.save_15_seconds",
					  "clipcoach.save_30_seconds",
					  "clipcoach.save_60_seconds",
					  "clipcoach.save_2_minutes",
					  "clipcoach.save_5_minutes",
					  "clipcoach.save_vertical",
					  "clipcoach.cycle_output_mode",
					  "clipcoach.toggle_dock",
				  }),
	       "native hotkey names must remain stable for persisted bindings");
	expect(manager.registerAll() && registrar.registerCalls == 9, "re-registering must be idempotent");

	registrar.press(0);
	registrar.press(1);
	registrar.press(2);
	registrar.press(3);
	registrar.press(4);
	registrar.press(5);
	expect(captureDurations == std::vector<int>({45, 15, 30, 60, 120, 300}),
	       "capture hotkeys must dispatch their exact duration");

	registrar.press(6);
	registrar.press(7);
	registrar.press(8);
	expect(verticalCalls == 1 && outputCalls == 1 && dockCalls == 1,
	       "vertical, output and dock actions must reach their services");

	registrar.press(1, false);
	expect(captureDurations.size() == 6, "key release must not execute an action");

	manager.unregisterAll();
	expect(registrar.unregistered.size() == 9 && registrar.callbacks.empty(),
	       "unregister must release every native registration");

	MockRegistrar inactiveRegistrar;
	clipcoach::HotkeyServices inactiveServices;
	inactiveServices.defaultDurationSeconds = [] {
		return 30;
	};
	inactiveServices.capture = [](int) {
		return HotkeyActionResult::fail("Replay Buffer is not active");
	};
	clipcoach::HotkeyManager inactive(inactiveRegistrar, std::move(inactiveServices));
	inactive.setErrorCallback([&errorCalls](clipcoach::HotkeyAction, const std::string &) { ++errorCalls; });
	expect(inactive.registerAll(), "inactive-buffer fixture must register");
	inactiveRegistrar.press(0);
	expect(errorCalls == 1, "Replay Buffer failure must be reported without throwing");

	MockRegistrar failingRegistrar;
	failingRegistrar.failAtCall = 4;
	clipcoach::HotkeyManager failing(failingRegistrar, {});
	expect(!failing.registerAll(), "partial native registration must fail atomically");
	expect(failingRegistrar.unregistered.size() == 3 && failingRegistrar.callbacks.empty(),
	       "failed registration must roll back earlier handles");

	return clipcoach::test::pass("hotkey-manager-test");
}
