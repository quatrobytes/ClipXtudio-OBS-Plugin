#pragma once

#include <clipcoach/core/hotkey-manager.hpp>
#include <clipcoach/core/settings.hpp>

#include <memory>
#include <cstddef>
#include <vector>

struct obs_data;
typedef struct obs_data obs_data_t;
typedef size_t obs_hotkey_id;
struct obs_hotkey;
typedef struct obs_hotkey obs_hotkey_t;

namespace clipcoach::plugin {

class ObsHotkeyAdapter final : public HotkeyRegistrar {
public:
	ObsHotkeyAdapter();
	~ObsHotkeyAdapter() override;

	[[nodiscard]] HotkeyRegistration
	registerHotkey(std::string name, std::string description,
		       Callback callback) override;
	void unregisterHotkey(HotkeyHandle handle) noexcept override;

	void startPersistence();
	void stopPersistence() noexcept;
	void notifyFrontendShutdown() noexcept;
	[[nodiscard]] bool applySettings(const Settings &settings);

private:
	struct Entry;

	static void hotkeyCallback(void *data, obs_hotkey_id id,
				   obs_hotkey_t *hotkey, bool pressed);
	static void frontendSaveCallback(obs_data_t *saveData, bool saving,
					 void *privateData);
	void save(obs_data_t *saveData);
	void load(obs_data_t *saveData);

	std::vector<std::unique_ptr<Entry>> entries_;
	bool persistenceRegistered_{false};
	bool frontendAvailable_{true};
};

} // namespace clipcoach::plugin
