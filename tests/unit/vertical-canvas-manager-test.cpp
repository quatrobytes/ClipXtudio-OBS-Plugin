#include "test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>

#include <chrono>
#include <filesystem>

int main()
{
	using namespace clipcoach;
	using clipcoach::test::expect;

	const auto directory =
		std::filesystem::temp_directory_path() /
		("clipcoach-vertical-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	std::filesystem::create_directories(directory);
	const auto path = directory / "settings.json";
	SettingsManager store(path);
	std::string error;
	expect(store.load(&error), "settings fixture must load");

	VerticalCanvasManager freeManager(store, false);
	const auto defaults = freeManager.settings();
	expect(defaults.width == 1080 && defaults.height == 1920 &&
		       defaults.resolution == VerticalResolution::Portrait1080,
	       "canvas defaults must be 1080x1920");
	expect(isNineBySixteen(defaults.width, defaults.height), "default canvas must be exact 9:16");
	expect(!freeManager.setResolution(VerticalResolution::Custom, 1000, 1000, &error),
	       "custom canvas must reject non-9:16 dimensions");
	expect(freeManager.setResolution(VerticalResolution::Custom, 900, 1600, &error) &&
		       freeManager.settings().width == 900 && freeManager.settings().height == 1600,
	       "custom canvas must accept exact 9:16 dimensions");
	expect(freeManager.setResolution(VerticalResolution::Portrait720, 0, 0, &error),
	       "720x1280 preset must be accepted");
	expect(freeManager.setResolution(VerticalResolution::Portrait1440, 0, 0, &error) &&
		       freeManager.settings().width == 1440 && freeManager.settings().height == 2560,
	       "2K 1440x2560 preset must be accepted");
	expect(freeManager.setResolution(VerticalResolution::Portrait2160, 0, 0, &error) &&
		       freeManager.settings().width == 2160 && freeManager.settings().height == 3840,
	       "4K 2160x3840 preset must be accepted");
	expect(freeManager.setResolution(VerticalResolution::Portrait4320, 0, 0, &error) &&
		       freeManager.settings().width == 4320 && freeManager.settings().height == 7680,
	       "8K 4320x7680 preset must be accepted");
	expect(freeManager.setResolution(VerticalResolution::Portrait720, 0, 0, &error),
	       "canvas must allow returning to the persisted 720p preset");

	expect(freeManager.setElementEnabled(VerticalElementType::Chat, true, &error),
	       "elements must be independently enabled");
	expect(freeManager.settings().element(VerticalElementType::Chat).enabled, "enabled element must be observable");
	expect(freeManager.setElementEnabled(VerticalElementType::Chat, false, &error) &&
		       !freeManager.settings().element(VerticalElementType::Chat).enabled,
	       "elements must be independently disabled");

	expect(freeManager.selectTemplate(VerticalTemplateId::GamingVertical, &error),
	       "Free must select the basic Gaming template");
	expect(!freeManager.selectTemplate(VerticalTemplateId::TalkingHead, &error),
	       "Free must not activate an advanced Pro template");
	expect(freeManager.setOutputMode(CaptureOutputMode::Vertical, &error) &&
		       freeManager.settings().outputMode == CaptureOutputMode::Vertical,
	       "Free must support the basic vertical clip output used by the default profile");
	expect(!freeManager.setOutputMode(CaptureOutputMode::Both, &error),
	       "Free must not activate Horizontal + Vertical output in core");

	VerticalCanvasManager proManager(store, true);
	expect(proManager.selectTemplate(VerticalTemplateId::TalkingHead, &error),
	       "Pro must select advanced templates");
	expect(proManager.settings().selectedTemplate == VerticalTemplateId::TalkingHead,
	       "selected template must update settings");
	expect(proManager.setOutputMode(CaptureOutputMode::Vertical, &error) &&
		       proManager.settings().outputMode == CaptureOutputMode::Vertical,
	       "output mode must support Vertical Canvas");
	expect(proManager.setOutputMode(CaptureOutputMode::Both, &error), "output mode must support Both");
	expect(proManager.setObsSelection("Gameplay", "Camera", &error) &&
		       proManager.selectedSceneName() == "Gameplay" && proManager.selectedSourceName() == "Camera",
	       "OBS scene and source selection must be stored through the core manager");
	expect(proManager.setFraming(140, 25, -10, &error) && proManager.settings().zoomPercent == 140 &&
		       proManager.settings().panXPercent == 25 && proManager.settings().panYPercent == -10,
	       "vertical source framing must accept and expose zoom and pan");
	expect(!proManager.setFraming(99, 0, 0, &error), "vertical framing must reject unsupported zoom");

	SettingsManager reloadedStore(path);
	expect(reloadedStore.load(&error), "saved vertical settings must reload");
	VerticalCanvasManager reloaded(reloadedStore, true);
	expect(reloaded.settings().resolution == VerticalResolution::Portrait720 &&
		       reloaded.settings().selectedTemplate == VerticalTemplateId::TalkingHead &&
		       reloaded.settings().outputMode == CaptureOutputMode::Both &&
		       reloaded.selectedSceneName() == "Gameplay" && reloaded.selectedSourceName() == "Camera" &&
		       reloaded.settings().zoomPercent == 140 && reloaded.settings().panXPercent == 25 &&
		       reloaded.settings().panYPercent == -10,
	       "canvas resolution, template, output mode and OBS selection must persist");

	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);
	return clipcoach::test::pass("vertical-canvas-manager-test");
}
