#pragma once

#include <clipcoach/core/vertical-canvas.hpp>
#include <clipcoach/core/trigger-types.hpp>
#include <clipcoach/core/speech-engine.hpp>

#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace clipcoach {

enum class NotificationDetail {
	Compact,
	Standard,
	Detailed,
};

enum class ExportContainer {
	Mp4,
	Mov,
};

enum class ExportCodec {
	H264,
	Hevc,
	Av1,
};

enum class ExportQuality {
	Medium = 0,
	High = 1,
	Maximum = 2,
	Low = 3,
};

namespace settings_constraints {
inline constexpr int kSchemaVersion = 14;
inline constexpr char kDefaultVerticalSceneName[] = "ClipXtudio Vertical";
inline constexpr int kMinClipDurationSeconds = 5;
inline constexpr int kMaxClipDurationSeconds = 300;
inline constexpr int kDefaultClipDurationSeconds = 30;
inline constexpr int kMaxRollSeconds = 120;
inline constexpr int kFreeClipLimitPerSession = 20;
inline constexpr int kMaxFileNameTemplateLength = 160;
inline constexpr int kMaxHotkeyLength = 64;
inline constexpr int kMaxTriggerListLength = 2048;
inline constexpr int kMaxVoicePhraseListLength = 4096;
inline constexpr int kDefaultTriggerSensitivity = 70;
inline constexpr int kDefaultTriggerCooldownSeconds = 15;
inline constexpr int kDefaultExportFps = 30;
inline constexpr int kSupportedExportFps[] = {30, 60};
inline constexpr int kPortrait1080Width = 1080;
inline constexpr int kPortrait1080Height = 1920;
inline constexpr int kPortrait720Width = 720;
inline constexpr int kPortrait720Height = 1280;
inline constexpr int kPortrait1440Width = 1440;
inline constexpr int kPortrait1440Height = 2560;
inline constexpr int kPortrait2160Width = 2160;
inline constexpr int kPortrait2160Height = 3840;
inline constexpr int kPortrait4320Width = 4320;
inline constexpr int kPortrait4320Height = 7680;
} // namespace settings_constraints

struct ResolutionDimensions {
	int width;
	int height;
};

[[nodiscard]] constexpr ResolutionDimensions verticalResolutionDimensions(VerticalResolution resolution) noexcept
{
	switch (resolution) {
	case VerticalResolution::Portrait720:
		return {settings_constraints::kPortrait720Width, settings_constraints::kPortrait720Height};
	case VerticalResolution::Portrait1440:
		return {settings_constraints::kPortrait1440Width, settings_constraints::kPortrait1440Height};
	case VerticalResolution::Portrait2160:
		return {settings_constraints::kPortrait2160Width, settings_constraints::kPortrait2160Height};
	case VerticalResolution::Portrait4320:
		return {settings_constraints::kPortrait4320Width, settings_constraints::kPortrait4320Height};
	case VerticalResolution::Custom:
		return {0, 0};
	case VerticalResolution::Portrait1080:
		return {settings_constraints::kPortrait1080Width, settings_constraints::kPortrait1080Height};
	}
	return {settings_constraints::kPortrait1080Width, settings_constraints::kPortrait1080Height};
}

[[nodiscard]] constexpr bool isKnownVerticalResolution(VerticalResolution resolution) noexcept
{
	switch (resolution) {
	case VerticalResolution::Portrait1080:
	case VerticalResolution::Portrait720:
	case VerticalResolution::Custom:
	case VerticalResolution::Portrait1440:
	case VerticalResolution::Portrait2160:
	case VerticalResolution::Portrait4320:
		return true;
	}
	return false;
}

struct Settings {
	int schemaVersion{settings_constraints::kSchemaVersion};
	std::string language{"system"};

	bool startWithObs{false};
	bool openDockAtStartup{true};
	bool initialSetupCompleted{false};
	std::string initialSetupCompletedVersion;
	bool autoStartReplayBuffer{false};
	bool remoteCommandsEnabled{true};

	int defaultDurationSeconds{settings_constraints::kDefaultClipDurationSeconds};
	int preRollSeconds{0};
	int postRollSeconds{0};
	CaptureOutputMode outputMode{CaptureOutputMode::Vertical};
	int freeClipLimitPerSession{settings_constraints::kFreeClipLimitPerSession};
	bool confirmBeforeDelete{true};
	std::vector<int> quickDurationsSeconds{15, 30, 60, 120, 300};

	std::filesystem::path clipDirectory;
	std::filesystem::path exportDirectory;
	std::filesystem::path thumbnailDirectory;

	bool includeDateInFileName{true};
	bool includeScoreInFileName{false};
	bool includeTriggerInFileName{true};
	bool includeOrientationInFileName{false};
	std::string fileNameTemplate{"{date}_{time}_{title}_{trigger}"};

	bool notificationsEnabled{true};
	bool soundOnSave{false};
	bool systemToastEnabled{false};
	NotificationDetail notificationDetail{NotificationDetail::Standard};

	std::string markMomentHotkey{"Ctrl+Shift+M"};
	std::string save15Hotkey;
	std::string save30Hotkey;
	std::string save60Hotkey;
	std::string saveVerticalHotkey;
	std::string openVerticalDesignerHotkey;

	ExportContainer exportContainer{ExportContainer::Mp4};
	ExportCodec exportCodec{ExportCodec::H264};
	int exportFps{settings_constraints::kDefaultExportFps};
	ExportQuality exportQuality{ExportQuality::High};
	VerticalResolution verticalResolution{VerticalResolution::Portrait1080};

	int verticalWidth{settings_constraints::kPortrait1080Width};
	int verticalHeight{settings_constraints::kPortrait1080Height};
	VerticalTemplateId verticalTemplate{VerticalTemplateId::GamingVertical};
	std::array<bool, 6> verticalElementsEnabled{true, true, true, false, false, false};
	std::string verticalSceneName{settings_constraints::kDefaultVerticalSceneName};
	std::string verticalSourceName;
	int verticalZoomPercent{100};
	int verticalPanXPercent{0};
	int verticalPanYPercent{0};

	bool triggerManualEnabled{true};
	bool triggerVoiceEnabled{false};
	bool triggerAudioSpikeEnabled{false};
	bool triggerChatPulseEnabled{false};
	bool triggerSceneEnabled{false};
	bool triggerKeywordEnabled{false};
	bool triggerAiHookEnabled{false};
	int triggerSensitivity{settings_constraints::kDefaultTriggerSensitivity};
	TriggerAction triggerAction{TriggerAction::MarkMoment};
	std::string triggerKeywordsCsv;
	std::string triggerScenesCsv;
	int triggerCooldownSeconds{settings_constraints::kDefaultTriggerCooldownSeconds};
	std::string voiceTriggerPhrasesCsv{"clip,saca clip,guarda eso,eso va pa TikTok,modo grinch,no ombe no,"
					   "eso fue duro,pa que aprendan,save clip,save that"};
	std::string voiceAudioSourceName;
	std::string voiceTriggerLanguage{"auto"};
	SpeechProcessingMode voiceSpeechMode{SpeechProcessingMode::Local};
	bool voiceCloudConsent{false};
	bool aiAssistantEnabled{false};
	bool aiPrivacyConsent{false};
	std::string aiLanguage{"auto"};

	[[nodiscard]] static Settings defaults() { return {}; }
};

[[nodiscard]] bool isValidSettingsDirectory(const std::filesystem::path &path) noexcept;
[[nodiscard]] bool isValidFileNameTemplate(const std::string &value) noexcept;
[[nodiscard]] bool validateSettings(const Settings &settings, std::string *error = nullptr);

} // namespace clipcoach
