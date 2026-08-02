#pragma once

#include <clipcoach/core/settings.hpp>

#include <filesystem>
#include <string>

namespace clipcoach {

class SettingsManager final {
public:
	explicit SettingsManager(std::filesystem::path settingsPath);

	[[nodiscard]] bool load(std::string *error = nullptr);
	[[nodiscard]] bool save(const Settings &settings, std::string *error = nullptr);
	[[nodiscard]] bool exportProfile(const std::filesystem::path &profilePath, std::string *error = nullptr) const;
	[[nodiscard]] bool importProfile(const std::filesystem::path &profilePath, std::string *error = nullptr);
	[[nodiscard]] const Settings &settings() const noexcept;
	[[nodiscard]] const std::filesystem::path &settingsPath() const noexcept;

private:
	std::filesystem::path settingsPath_;
	Settings settings_{Settings::defaults()};
};

} // namespace clipcoach
