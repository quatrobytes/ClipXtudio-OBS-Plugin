#include "test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>

#include <chrono>
#include <filesystem>
#include <string>

int main()
{
	const auto root = std::filesystem::temp_directory_path() /
			  ("clipcoach-settings-validation-" +
			   std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(root);
	clipcoach::SettingsManager manager(root / "settings.json");
	std::string error;
	clipcoach::test::expect(manager.load(&error), "validation fixture must load defaults");

	auto valid = manager.settings();
	valid.clipDirectory = root / "clips";
	valid.exportDirectory = root / "exports";
	valid.thumbnailDirectory = root / "thumbnails";
	valid.fileNameTemplate = "{date}_{time}_{score}_{trigger}_{orientation}_{title}";
	clipcoach::test::expect(manager.save(valid, &error),
				"absolute future directories and supported tokens must save");

	auto invalid = valid;
	invalid.clipDirectory = std::filesystem::path("relative/clips");
	clipcoach::test::expect(!manager.save(invalid, &error), "relative media directories must be rejected");

	invalid = valid;
	invalid.fileNameTemplate = "{date}_{unknown}";
	clipcoach::test::expect(!manager.save(invalid, &error), "unknown filename tokens must be rejected");

	invalid = valid;
	invalid.fileNameTemplate = "../{date}";
	clipcoach::test::expect(!manager.save(invalid, &error), "filename templates must reject path traversal");

	invalid = valid;
	invalid.defaultDurationSeconds = 301;
	clipcoach::test::expect(!manager.save(invalid, &error),
				"capture durations above Replay Buffer limits must fail");

	invalid = valid;
	invalid.preRollSeconds = -1;
	clipcoach::test::expect(!manager.save(invalid, &error), "negative pre-roll must fail");

	invalid = valid;
	invalid.defaultDurationSeconds = 250;
	invalid.preRollSeconds = 30;
	invalid.postRollSeconds = 30;
	clipcoach::test::expect(!manager.save(invalid, &error),
				"duration plus pre/post-roll must fit Replay Buffer limits");

	invalid = valid;
	invalid.exportFps = 25;
	clipcoach::test::expect(!manager.save(invalid, &error), "unsupported export FPS must fail");

	invalid = valid;
	invalid.verticalResolution = clipcoach::VerticalResolution::Portrait720;
	clipcoach::test::expect(!manager.save(invalid, &error), "vertical preset and dimensions must stay consistent");

	invalid = valid;
	invalid.save15Hotkey = invalid.markMomentHotkey;
	clipcoach::test::expect(!manager.save(invalid, &error), "duplicate non-empty hotkeys must fail");

	invalid = valid;
	invalid.voiceAudioSourceName = std::string(513, 'x');
	clipcoach::test::expect(!manager.save(invalid, &error), "OBS audio source names must be bounded");

	clipcoach::test::expect(manager.settings().fileNameTemplate == valid.fileNameTemplate,
				"failed saves must not mutate active settings");
	std::error_code cleanupError;
	std::filesystem::remove_all(root, cleanupError);
	return clipcoach::test::pass("settings-validation-test");
}
