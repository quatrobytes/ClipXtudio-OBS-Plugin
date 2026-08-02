#include <clipcoach/core/hotkey-manager.hpp>

#include <array>
#include <utility>

namespace clipcoach {
namespace {

struct HotkeyDefinition {
	HotkeyAction action;
	const char *name;
	const char *description;
};

constexpr std::array<HotkeyDefinition, 9> kDefinitions = {{
	{HotkeyAction::MarkMoment, "clipcoach.mark_moment",
	 "ClipXtudio: Mark moment"},
	{HotkeyAction::Save15Seconds, "clipcoach.save_15_seconds",
	 "ClipXtudio: Save 15 seconds"},
	{HotkeyAction::Save30Seconds, "clipcoach.save_30_seconds",
	 "ClipXtudio: Save 30 seconds"},
	{HotkeyAction::Save60Seconds, "clipcoach.save_60_seconds",
	 "ClipXtudio: Save 60 seconds"},
	{HotkeyAction::Save2Minutes, "clipcoach.save_2_minutes",
	 "ClipXtudio: Save 2 minutes"},
	{HotkeyAction::Save5Minutes, "clipcoach.save_5_minutes",
	 "ClipXtudio: Save 5 minutes"},
	{HotkeyAction::SaveVertical, "clipcoach.save_vertical",
	 "ClipXtudio: Save vertical clip"},
	{HotkeyAction::CycleOutputMode, "clipcoach.cycle_output_mode",
	 "ClipXtudio: Cycle output mode"},
	{HotkeyAction::ToggleDock, "clipcoach.toggle_dock",
	 "ClipXtudio: Open or close dock"},
}};

} // namespace

HotkeyManager::HotkeyManager(HotkeyRegistrar &registrar,
			     HotkeyServices services,
			     DescriptionProvider descriptionProvider)
	: registrar_(registrar),
	  services_(std::move(services)),
	  descriptionProvider_(std::move(descriptionProvider))
{
}

HotkeyManager::~HotkeyManager()
{
	unregisterAll();
}

bool HotkeyManager::registerAll()
{
	if (!registrations_.empty()) {
		return registrations_.size() == kDefinitions.size();
	}
	for (const auto &definition : kDefinitions) {
		auto description =
			descriptionProvider_
				? descriptionProvider_(definition.action)
				: std::string(definition.description);
		if (description.empty()) {
			description = definition.description;
		}
		auto registration = registrar_.registerHotkey(
			definition.name, std::move(description),
			[this, action = definition.action](bool pressed) {
				(void)execute(action, pressed);
			});
		if (!registration.success) {
			unregisterAll();
			return false;
		}
		registrations_.push_back(
			{definition.action, registration.handle});
	}
	return true;
}

void HotkeyManager::unregisterAll() noexcept
{
	for (auto iterator = registrations_.rbegin();
	     iterator != registrations_.rend(); ++iterator) {
		registrar_.unregisterHotkey(iterator->handle);
	}
	registrations_.clear();
}

HotkeyActionResult HotkeyManager::execute(HotkeyAction action, bool pressed)
{
	if (!pressed) {
		return HotkeyActionResult::ok();
	}
	auto result = executePressed(action);
	reportFailure(action, result);
	return result;
}

void HotkeyManager::setErrorCallback(ErrorCallback callback)
{
	errorCallback_ = std::move(callback);
}

bool HotkeyManager::registered() const noexcept
{
	return registrations_.size() == kDefinitions.size();
}

HotkeyActionResult HotkeyManager::executePressed(HotkeyAction action)
{
	switch (action) {
	case HotkeyAction::MarkMoment:
		if (!services_.defaultDurationSeconds) {
			return HotkeyActionResult::fail(
				"default duration service is unavailable");
		}
		return capture(services_.defaultDurationSeconds());
	case HotkeyAction::Save15Seconds:
		return capture(15);
	case HotkeyAction::Save30Seconds:
		return capture(30);
	case HotkeyAction::Save60Seconds:
		return capture(60);
	case HotkeyAction::Save2Minutes:
		return capture(120);
	case HotkeyAction::Save5Minutes:
		return capture(300);
	case HotkeyAction::SaveVertical:
		return services_.captureVertical
			       ? services_.captureVertical()
			       : HotkeyActionResult::fail(
					 "vertical capture service is unavailable");
	case HotkeyAction::CycleOutputMode:
		return services_.cycleOutputMode
			       ? services_.cycleOutputMode()
			       : HotkeyActionResult::fail(
					 "output mode service is unavailable");
	case HotkeyAction::ToggleDock:
		return services_.toggleDock
			       ? services_.toggleDock()
			       : HotkeyActionResult::fail(
					 "dock visibility service is unavailable");
	}
	return HotkeyActionResult::fail("unknown hotkey action");
}

HotkeyActionResult HotkeyManager::capture(int seconds)
{
	return services_.capture
		       ? services_.capture(seconds)
		       : HotkeyActionResult::fail("capture service is unavailable");
}

void HotkeyManager::reportFailure(HotkeyAction action,
				  const HotkeyActionResult &result) const
{
	if (!result.success && errorCallback_) {
		errorCallback_(action, result.error);
	}
}

} // namespace clipcoach
