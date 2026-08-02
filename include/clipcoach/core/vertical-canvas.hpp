#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace clipcoach {

enum class CaptureOutputMode {
	Horizontal,
	Vertical,
	Both,
};

enum class VerticalResolution {
	// Keep the first three persisted values stable for existing installations.
	Portrait1080 = 0,
	Portrait720 = 1,
	Custom = 2,
	Portrait1440 = 3,
	Portrait2160 = 4,
	Portrait4320 = 5,
};

enum class VerticalTemplateId {
	GamingVertical,
	TalkingHead,
	TechReview,
	ProductReview,
};

enum class VerticalElementType {
	Gameplay,
	Camera,
	Subtitles,
	Title,
	Logo,
	Chat,
};

struct VerticalElement {
	VerticalElementType type{VerticalElementType::Gameplay};
	bool enabled{false};
	// Normalized geometry prepares the model for a future libobs renderer.
	double x{0.0};
	double y{0.0};
	double width{1.0};
	double height{1.0};
	int zOrder{0};
};

struct VerticalLayoutTemplate {
	VerticalTemplateId id{VerticalTemplateId::GamingVertical};
	std::string name;
	bool proOnly{false};
	std::array<VerticalElement, 6> elements;
};

struct VerticalCanvasSettings {
	CaptureOutputMode outputMode{CaptureOutputMode::Vertical};
	VerticalResolution resolution{VerticalResolution::Portrait1080};
	int width{1080};
	int height{1920};
	int zoomPercent{100};
	int panXPercent{0};
	int panYPercent{0};
	VerticalTemplateId selectedTemplate{
		VerticalTemplateId::GamingVertical};
	std::array<VerticalElement, 6> elements;

	[[nodiscard]] const VerticalElement &
	element(VerticalElementType type) const noexcept
	{
		return elements[static_cast<std::size_t>(type)];
	}
};

[[nodiscard]] constexpr bool isNineBySixteen(int width, int height) noexcept
{
	return width > 0 && height > 0 &&
	       static_cast<long long>(width) * 16 ==
		       static_cast<long long>(height) * 9;
}

} // namespace clipcoach
