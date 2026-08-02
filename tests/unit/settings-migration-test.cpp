#include "test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

int main()
{
	const auto directory = std::filesystem::temp_directory_path() /
			       ("clipcoach-settings-migration-" +
				std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	const auto path = directory / "settings.json";
	{
		std::ofstream legacy(path);
		legacy << R"JSON({
  "schema_version": 1,
  "language": "es-ES",
  "notifications_enabled": false,
  "vertical_width": 1080,
  "vertical_height": 1920,
  "quick_durations_seconds": [15, 30, 60, 120, 300]
})JSON";
	}

	clipcoach::SettingsManager manager(path);
	std::string error;
	clipcoach::test::expect(manager.load(&error), "schema v1 settings must migrate automatically");
	clipcoach::test::expect(manager.settings().schemaVersion == 14 && manager.settings().language == "es-ES" &&
					!manager.settings().notificationsEnabled &&
					manager.settings().openDockAtStartup &&
					manager.settings().voiceTriggerLanguage == "auto" &&
					manager.settings().voiceSpeechMode ==
						clipcoach::SpeechProcessingMode::Local &&
					!manager.settings().voiceCloudConsent,
				"migration must preserve legacy values and supply v2 defaults");

	std::ifstream migrated(path);
	std::ostringstream document;
	document << migrated.rdbuf();
	clipcoach::test::expect(document.str().find("\"schema_version\": 14") != std::string::npos,
				"migration must publish the current settings schema");

	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);
	return clipcoach::test::pass("settings-migration-test");
}
