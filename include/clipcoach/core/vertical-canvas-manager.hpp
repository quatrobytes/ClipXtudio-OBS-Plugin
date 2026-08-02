#pragma once

#include <clipcoach/core/vertical-canvas.hpp>

#include <vector>

namespace clipcoach {

class SettingsManager;

class VerticalCanvasManager final {
public:
	explicit VerticalCanvasManager(SettingsManager &settingsManager, bool proUnlocked = false);

	[[nodiscard]] VerticalCanvasSettings settings() const;
	[[nodiscard]] const std::vector<VerticalLayoutTemplate> &templates() const noexcept;
	[[nodiscard]] bool proUnlocked() const noexcept;
	void setProUnlocked(bool unlocked) noexcept;

	[[nodiscard]] bool setOutputMode(CaptureOutputMode mode, std::string *error = nullptr);
	[[nodiscard]] bool setResolution(VerticalResolution resolution, int customWidth = 0, int customHeight = 0,
					 std::string *error = nullptr);
	[[nodiscard]] bool selectTemplate(VerticalTemplateId id, std::string *error = nullptr);
	[[nodiscard]] bool setElementEnabled(VerticalElementType type, bool enabled, std::string *error = nullptr);
	[[nodiscard]] bool setObsSelection(std::string sceneName, std::string sourceName,
					   std::string *error = nullptr);
	[[nodiscard]] bool setFraming(int zoomPercent, int panXPercent, int panYPercent,
				      std::string *error = nullptr);
	[[nodiscard]] std::string selectedSceneName() const;
	[[nodiscard]] std::string selectedSourceName() const;

private:
	[[nodiscard]] bool persist(const VerticalCanvasSettings &settings, std::string *error);

	SettingsManager &settingsManager_;
	bool proUnlocked_{false};
	std::vector<VerticalLayoutTemplate> templates_;
};

} // namespace clipcoach
