#include <clipcoach/core/vertical-canvas-manager.hpp>

#include <clipcoach/core/settings-manager.hpp>

#include <algorithm>
#include <utility>

namespace clipcoach {
namespace {

VerticalElement element(VerticalElementType type, bool enabled, double x, double y, double width, double height, int z)
{
	return {type, enabled, x, y, width, height, z};
}

std::array<VerticalElement, 6> gamingElements()
{
	return {{
		element(VerticalElementType::Gameplay, true, 0.0, 0.18, 1.0, 0.56, 0),
		element(VerticalElementType::Camera, true, 0.58, 0.02, 0.38, 0.21, 3),
		element(VerticalElementType::Subtitles, true, 0.08, 0.78, 0.84, 0.10, 4),
		element(VerticalElementType::Title, false, 0.08, 0.04, 0.48, 0.10, 2),
		element(VerticalElementType::Logo, false, 0.80, 0.90, 0.14, 0.07, 5),
		element(VerticalElementType::Chat, false, 0.05, 0.76, 0.40, 0.20, 1),
	}};
}

std::array<VerticalElement, 6> talkingHeadElements()
{
	auto result = gamingElements();
	result[0].enabled = false;
	result[1] = element(VerticalElementType::Camera, true, 0.05, 0.08, 0.90, 0.68, 1);
	result[2].y = 0.80;
	result[3].enabled = true;
	return result;
}

std::vector<VerticalLayoutTemplate> makeTemplates()
{
	auto tech = gamingElements();
	tech[3].enabled = true;
	tech[4].enabled = true;
	auto product = talkingHeadElements();
	product[0] = element(VerticalElementType::Gameplay, true, 0.06, 0.42, 0.88, 0.34, 0);
	product[1] = element(VerticalElementType::Camera, true, 0.62, 0.05, 0.32, 0.28, 2);
	product[4].enabled = true;
	return {
		{VerticalTemplateId::GamingVertical, "Gaming Vertical", false, gamingElements()},
		{VerticalTemplateId::TalkingHead, "Talking Head", true, talkingHeadElements()},
		{VerticalTemplateId::TechReview, "Tech Review", true, tech},
		{VerticalTemplateId::ProductReview, "Product Review", true, product},
	};
}

bool fail(std::string *error, const char *message)
{
	if (error != nullptr) {
		*error = message;
	}
	return false;
}

} // namespace

VerticalCanvasManager::VerticalCanvasManager(SettingsManager &settingsManager, bool proUnlocked)
	: settingsManager_(settingsManager),
	  proUnlocked_(proUnlocked),
	  templates_(makeTemplates())
{
}

VerticalCanvasSettings VerticalCanvasManager::settings() const
{
	const auto &stored = settingsManager_.settings();
	VerticalCanvasSettings result;
	result.outputMode = stored.outputMode;
	result.resolution = stored.verticalResolution;
	result.width = stored.verticalWidth;
	result.height = stored.verticalHeight;
	result.zoomPercent = stored.verticalZoomPercent;
	result.panXPercent = stored.verticalPanXPercent;
	result.panYPercent = stored.verticalPanYPercent;
	result.selectedTemplate = stored.verticalTemplate;
	const auto selected = std::find_if(templates_.begin(), templates_.end(), [&result](const auto &candidate) {
		return candidate.id == result.selectedTemplate;
	});
	result.elements = selected != templates_.end() ? selected->elements : gamingElements();
	for (std::size_t index = 0; index < result.elements.size(); ++index) {
		result.elements[index].enabled = stored.verticalElementsEnabled[index];
	}
	return result;
}

const std::vector<VerticalLayoutTemplate> &VerticalCanvasManager::templates() const noexcept
{
	return templates_;
}

bool VerticalCanvasManager::proUnlocked() const noexcept
{
	return proUnlocked_;
}

void VerticalCanvasManager::setProUnlocked(bool unlocked) noexcept
{
	proUnlocked_ = unlocked;
}

bool VerticalCanvasManager::setOutputMode(CaptureOutputMode mode, std::string *error)
{
	// Basic 9:16 export is available in Free. Only generating both variants
	// from one capture remains a Pro entitlement.
	if (mode == CaptureOutputMode::Both && !proUnlocked_) {
		return fail(error, "Horizontal + Vertical output requires ClipXtudio Pro");
	}
	auto next = settings();
	next.outputMode = mode;
	return persist(next, error);
}

bool VerticalCanvasManager::setResolution(VerticalResolution resolution, int customWidth, int customHeight,
					  std::string *error)
{
	auto next = settings();
	next.resolution = resolution;
	if (!isKnownVerticalResolution(resolution))
		return fail(error, "vertical resolution preset is unknown");
	if (resolution == VerticalResolution::Custom) {
		if (!isNineBySixteen(customWidth, customHeight)) {
			return fail(error, "custom vertical resolution must use an exact 9:16 ratio");
		}
		next.width = customWidth;
		next.height = customHeight;
	} else {
		const auto dimensions = verticalResolutionDimensions(resolution);
		next.width = dimensions.width;
		next.height = dimensions.height;
	}
	return persist(next, error);
}

bool VerticalCanvasManager::selectTemplate(VerticalTemplateId id, std::string *error)
{
	const auto selected = std::find_if(templates_.begin(), templates_.end(),
					   [id](const auto &candidate) { return candidate.id == id; });
	if (selected == templates_.end()) {
		return fail(error, "vertical template is unknown");
	}
	if (selected->proOnly && !proUnlocked_) {
		return fail(error, "vertical template requires ClipXtudio Pro");
	}
	auto next = settings();
	next.selectedTemplate = id;
	next.elements = selected->elements;
	return persist(next, error);
}

bool VerticalCanvasManager::setElementEnabled(VerticalElementType type, bool enabled, std::string *error)
{
	const auto index = static_cast<std::size_t>(type);
	if (index >= 6) {
		return fail(error, "vertical element is unknown");
	}
	auto next = settings();
	next.elements[index].enabled = enabled;
	return persist(next, error);
}

bool VerticalCanvasManager::setObsSelection(std::string sceneName, std::string sourceName, std::string *error)
{
	if (sceneName.empty())
		return fail(error, "vertical OBS scene name cannot be empty");
	if (sceneName.size() > 512 || sourceName.size() > 512) {
		return fail(error, "vertical OBS scene or source name is too long");
	}
	auto stored = settingsManager_.settings();
	stored.verticalSceneName = std::move(sceneName);
	stored.verticalSourceName = std::move(sourceName);
	return settingsManager_.save(stored, error);
}

bool VerticalCanvasManager::setFraming(int zoomPercent, int panXPercent, int panYPercent, std::string *error)
{
	if (zoomPercent < 100 || zoomPercent > 300 || panXPercent < -100 || panXPercent > 100 || panYPercent < -100 ||
	    panYPercent > 100)
		return fail(error, "vertical framing values are outside the supported range");
	auto stored = settingsManager_.settings();
	stored.verticalZoomPercent = zoomPercent;
	stored.verticalPanXPercent = panXPercent;
	stored.verticalPanYPercent = panYPercent;
	return settingsManager_.save(stored, error);
}

std::string VerticalCanvasManager::selectedSceneName() const
{
	return settingsManager_.settings().verticalSceneName;
}

std::string VerticalCanvasManager::selectedSourceName() const
{
	return settingsManager_.settings().verticalSourceName;
}

bool VerticalCanvasManager::persist(const VerticalCanvasSettings &canvas, std::string *error)
{
	auto stored = settingsManager_.settings();
	stored.outputMode = canvas.outputMode;
	stored.verticalResolution = canvas.resolution;
	stored.verticalWidth = canvas.width;
	stored.verticalHeight = canvas.height;
	stored.verticalTemplate = canvas.selectedTemplate;
	for (std::size_t index = 0; index < canvas.elements.size(); ++index) {
		stored.verticalElementsEnabled[index] = canvas.elements[index].enabled;
	}
	return settingsManager_.save(stored, error);
}

} // namespace clipcoach
