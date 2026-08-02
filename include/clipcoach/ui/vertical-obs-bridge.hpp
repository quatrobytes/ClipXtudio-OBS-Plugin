#pragma once

#include <clipcoach/core/vertical-canvas.hpp>

#include <functional>
#include <string>
#include <vector>

class QWidget;

namespace clipcoach::ui {

struct ReplayEncoderOption {
	std::string id;
	std::string displayName;
	bool hardware{false};
};

struct ReplayProfileSettings {
	std::string outputMode;
	std::string encoderId;
	std::string encoderDisplayName;
	bool hardwareEncoder{false};
	bool replayBufferEnabled{false};
};

struct ReplayProfileApplyResult {
	bool success{false};
	bool restartRequired{false};
	std::string message;
};

// UI-facing boundary for the OBS-specific vertical compositor. The Qt library
// remains testable without linking libobs; the plugin supplies these callbacks.
struct VerticalObsBridge {
	using NameProvider = std::function<std::vector<std::string>()>;
	using SourceProvider = std::function<std::vector<std::string>(const std::string &)>;
	using PreviewFactory = std::function<QWidget *(QWidget *)>;
	using PreviewUpdater =
		std::function<void(QWidget *, const std::string &, const VerticalCanvasSettings &)>;
	using FramingChanged = std::function<void(int, int, int)>;
	using PreviewInteractionBinder =
		std::function<void(QWidget *, FramingChanged)>;
	using SceneCreator = std::function<bool(const std::string &, const std::string &,
					       const VerticalCanvasSettings &, std::string *)>;
	using ReplayEncoderProvider =
		std::function<std::vector<ReplayEncoderOption>()>;
	using ReplayProfileProvider = std::function<ReplayProfileSettings()>;
	using ReplayProfileApplier =
		std::function<ReplayProfileApplyResult(const std::string &, bool)>;
	using ObsRestartRequester = std::function<bool(std::string *)>;
	using UpdateInstaller =
		std::function<bool(const std::string &, std::string *)>;

	NameProvider scenes;
	std::function<std::string()> activeScene;
	SourceProvider sourcesForScene;
	PreviewFactory createPreview;
	PreviewUpdater updatePreview;
	PreviewInteractionBinder bindPreviewInteraction;
	SceneCreator createVerticalScene;
	ReplayEncoderProvider replayEncoders;
	ReplayProfileProvider replayProfile;
	ReplayProfileApplier applyReplayProfile;
	ObsRestartRequester restartObs;
	UpdateInstaller installUpdate;

	[[nodiscard]] bool available() const noexcept
	{
		return static_cast<bool>(scenes) && static_cast<bool>(createPreview) &&
		       static_cast<bool>(updatePreview);
	}
};

} // namespace clipcoach::ui
