#include <clipcoach/plugin/obs-hotkey-adapter.hpp>

#include <obs-frontend-api.h>
#include <obs-interaction.h>
#include <obs-module.h>

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>
#include <utility>

namespace clipcoach::plugin {
namespace {

constexpr const char *kStorageKey = "clipcoach_studio_hotkeys";
constexpr const char *kLogPrefix = "[ClipXtudio]";

std::string trim(std::string value)
{
	const auto first = value.find_first_not_of(" \t\r\n");
	if (first == std::string::npos)
		return {};
	const auto last = value.find_last_not_of(" \t\r\n");
	return value.substr(first, last - first + 1);
}

std::string upper(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		       [](unsigned char character) {
			       return static_cast<char>(std::toupper(character));
		       });
	return value;
}

obs_key_t keyFromText(const std::string &text)
{
	const auto cleaned = trim(text);
	if (cleaned.empty())
		return OBS_KEY_NONE;

	const auto normalized = upper(cleaned);
	if (normalized.size() == 1 && normalized[0] >= 'A' &&
	    normalized[0] <= 'Z') {
		return static_cast<obs_key_t>(
			OBS_KEY_A + (normalized[0] - 'A'));
	}
	if (normalized.size() == 1 && normalized[0] >= '0' &&
	    normalized[0] <= '9') {
		return static_cast<obs_key_t>(
			OBS_KEY_0 + (normalized[0] - '0'));
	}
	if (normalized.size() >= 2 && normalized[0] == 'F') {
		try {
			const auto number = std::stoi(normalized.substr(1));
			if (number >= 1 && number <= 35) {
				return static_cast<obs_key_t>(
					OBS_KEY_F1 + (number - 1));
			}
		} catch (...) {
			// Fall through to OBS' name parser.
		}
	}
	const std::pair<std::string_view, obs_key_t> aliases[] = {
		{"ESC", OBS_KEY_ESCAPE},       {"ESCAPE", OBS_KEY_ESCAPE},
		{"ENTER", OBS_KEY_ENTER},      {"RETURN", OBS_KEY_RETURN},
		{"SPACE", OBS_KEY_SPACE},      {"TAB", OBS_KEY_TAB},
		{"BACKSPACE", OBS_KEY_BACKSPACE},
		{"DELETE", OBS_KEY_DELETE},    {"INSERT", OBS_KEY_INSERT},
		{"HOME", OBS_KEY_HOME},        {"END", OBS_KEY_END},
		{"PAGEUP", OBS_KEY_PAGEUP},    {"PAGEDOWN", OBS_KEY_PAGEDOWN},
		{"LEFT", OBS_KEY_LEFT},        {"RIGHT", OBS_KEY_RIGHT},
		{"UP", OBS_KEY_UP},            {"DOWN", OBS_KEY_DOWN},
		{"PLUS", OBS_KEY_PLUS},        {"+", OBS_KEY_PLUS},
		{"MINUS", OBS_KEY_MINUS},      {"-", OBS_KEY_MINUS},
		{"COMMA", OBS_KEY_COMMA},      {",", OBS_KEY_COMMA},
		{"PERIOD", OBS_KEY_PERIOD},    {".", OBS_KEY_PERIOD},
	};
	for (const auto &[name, key] : aliases) {
		if (normalized == name)
			return key;
	}

	for (const auto &candidate :
	     {cleaned, normalized, std::string("OBS_KEY_") + normalized}) {
		const auto key = obs_key_from_name(candidate.c_str());
		if (key != OBS_KEY_NONE)
			return key;
	}
	return OBS_KEY_NONE;
}

bool parseCombination(const std::string &text,
		      obs_key_combination_t &combination)
{
	combination = {};
	const auto cleaned = trim(text);
	if (cleaned.empty())
		return true;

	std::stringstream stream(cleaned);
	std::string part;
	std::string keyText;
	while (std::getline(stream, part, '+')) {
		part = trim(part);
		const auto normalized = upper(part);
		if (normalized == "CTRL" || normalized == "CONTROL") {
			combination.modifiers |= INTERACT_CONTROL_KEY;
		} else if (normalized == "SHIFT") {
			combination.modifiers |= INTERACT_SHIFT_KEY;
		} else if (normalized == "ALT") {
			combination.modifiers |= INTERACT_ALT_KEY;
		} else if (normalized == "META" || normalized == "CMD" ||
			   normalized == "COMMAND" || normalized == "WIN" ||
			   normalized == "WINDOWS") {
			combination.modifiers |= INTERACT_COMMAND_KEY;
		} else {
			keyText = part;
		}
	}
	combination.key = keyFromText(keyText);
	return combination.key != OBS_KEY_NONE;
}

} // namespace

struct ObsHotkeyAdapter::Entry {
	ObsHotkeyAdapter *owner{nullptr};
	obs_hotkey_id id{OBS_INVALID_HOTKEY_ID};
	std::string name;
	Callback callback;
};

ObsHotkeyAdapter::ObsHotkeyAdapter() = default;

ObsHotkeyAdapter::~ObsHotkeyAdapter()
{
	stopPersistence();
	while (!entries_.empty()) {
		unregisterHotkey(
			static_cast<HotkeyHandle>(entries_.back()->id));
	}
}

HotkeyRegistration ObsHotkeyAdapter::registerHotkey(
	std::string name, std::string description, Callback callback)
{
	if (name.empty() || !callback) {
		return {false, 0, "hotkey registration metadata is invalid"};
	}
	auto entry = std::make_unique<Entry>();
	entry->owner = this;
	entry->name = std::move(name);
	entry->callback = std::move(callback);
	entry->id = obs_hotkey_register_frontend(
		entry->name.c_str(), description.c_str(), hotkeyCallback,
		entry.get());
	if (entry->id == OBS_INVALID_HOTKEY_ID) {
		blog(LOG_ERROR, "%s Could not register hotkey %s", kLogPrefix,
		     entry->name.c_str());
		return {false, 0, "OBS rejected frontend hotkey registration"};
	}
	const auto handle = static_cast<HotkeyHandle>(entry->id);
	entries_.push_back(std::move(entry));
	return {true, handle, {}};
}

void ObsHotkeyAdapter::unregisterHotkey(HotkeyHandle handle) noexcept
{
	const auto found = std::find_if(
		entries_.begin(), entries_.end(),
		[handle](const auto &entry) {
			return static_cast<HotkeyHandle>(entry->id) == handle;
		});
	if (found == entries_.end()) {
		return;
	}
	if ((*found)->id != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister((*found)->id);
		(*found)->id = OBS_INVALID_HOTKEY_ID;
	}
	entries_.erase(found);
}

void ObsHotkeyAdapter::startPersistence()
{
	if (!persistenceRegistered_ && frontendAvailable_) {
		obs_frontend_add_save_callback(frontendSaveCallback, this);
		persistenceRegistered_ = true;
	}
}

void ObsHotkeyAdapter::stopPersistence() noexcept
{
	if (persistenceRegistered_ && frontendAvailable_) {
		obs_frontend_remove_save_callback(frontendSaveCallback, this);
	}
	persistenceRegistered_ = false;
}

void ObsHotkeyAdapter::notifyFrontendShutdown() noexcept
{
	frontendAvailable_ = false;
	persistenceRegistered_ = false;
}

bool ObsHotkeyAdapter::applySettings(const Settings &settings)
{
	const std::pair<const char *, const std::string *> configured[] = {
		{"clipcoach.mark_moment", &settings.markMomentHotkey},
		{"clipcoach.save_15_seconds", &settings.save15Hotkey},
		{"clipcoach.save_30_seconds", &settings.save30Hotkey},
		{"clipcoach.save_60_seconds", &settings.save60Hotkey},
		{"clipcoach.save_vertical", &settings.saveVerticalHotkey},
		{"clipcoach.toggle_dock", &settings.openVerticalDesignerHotkey},
	};

	bool success = true;
	for (const auto &[name, bindingText] : configured) {
		const auto found = std::find_if(
			entries_.begin(), entries_.end(),
			[name](const auto &entry) { return entry->name == name; });
		if (found == entries_.end()) {
			blog(LOG_WARNING, "%s Hotkey %s is not registered",
			     kLogPrefix, name);
			success = false;
			continue;
		}

		obs_key_combination_t combination{};
		if (!parseCombination(*bindingText, combination)) {
			blog(LOG_WARNING,
			     "%s Ignored invalid shortcut for %s", kLogPrefix,
			     name);
			success = false;
			continue;
		}
		if (bindingText->empty()) {
			obs_hotkey_load_bindings((*found)->id, nullptr, 0);
		} else {
			obs_hotkey_load_bindings((*found)->id, &combination, 1);
		}
	}
	if (success && frontendAvailable_)
		obs_frontend_save();
	return success;
}

void ObsHotkeyAdapter::hotkeyCallback(void *data, obs_hotkey_id,
				      obs_hotkey_t *, bool pressed)
{
	auto *entry = static_cast<Entry *>(data);
	if (entry != nullptr && entry->owner != nullptr && entry->callback) {
		entry->callback(pressed);
	}
}

void ObsHotkeyAdapter::frontendSaveCallback(obs_data_t *saveData, bool saving,
					    void *privateData)
{
	auto *adapter = static_cast<ObsHotkeyAdapter *>(privateData);
	if (adapter == nullptr || saveData == nullptr) {
		return;
	}
	if (saving) {
		adapter->save(saveData);
	} else {
		adapter->load(saveData);
	}
}

void ObsHotkeyAdapter::save(obs_data_t *saveData)
{
	obs_data_t *hotkeys = obs_data_create();
	for (const auto &entry : entries_) {
		obs_data_array_t *bindings = obs_hotkey_save(entry->id);
		if (bindings != nullptr) {
			obs_data_set_array(hotkeys, entry->name.c_str(),
					   bindings);
			obs_data_array_release(bindings);
		}
	}
	obs_data_set_obj(saveData, kStorageKey, hotkeys);
	obs_data_release(hotkeys);
}

void ObsHotkeyAdapter::load(obs_data_t *saveData)
{
	obs_data_t *hotkeys = obs_data_get_obj(saveData, kStorageKey);
	if (hotkeys == nullptr) {
		return;
	}
	for (const auto &entry : entries_) {
		obs_data_array_t *bindings =
			obs_data_get_array(hotkeys, entry->name.c_str());
		if (bindings != nullptr) {
			obs_hotkey_load(entry->id, bindings);
			obs_data_array_release(bindings);
		}
	}
	obs_data_release(hotkeys);
}

} // namespace clipcoach::plugin
