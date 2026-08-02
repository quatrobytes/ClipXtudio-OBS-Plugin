#include <clipcoach/core/settings.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <system_error>

namespace clipcoach {
namespace {

bool fail(std::string *error, const char *message)
{
	if (error != nullptr) {
		*error = message;
	}
	return false;
}

template<typename Enum> bool enumInRange(Enum value, int maximum)
{
	const auto numeric = static_cast<int>(value);
	return numeric >= 0 && numeric <= maximum;
}

} // namespace

bool isValidSettingsDirectory(const std::filesystem::path &path) noexcept
{
	if (path.empty()) {
		return true;
	}
	if (!path.is_absolute() || path.u8string().find('\0') != std::string::npos) {
		return false;
	}
	std::error_code error;
	return !std::filesystem::exists(path, error) || (!error && std::filesystem::is_directory(path, error));
}

bool isValidFileNameTemplate(const std::string &value) noexcept
{
	if (value.empty() ||
	    value.size() > static_cast<std::size_t>(settings_constraints::kMaxFileNameTemplateLength) ||
	    value.find_first_of("/\\:*?\"<>|") != std::string::npos || value == "." || value == "..") {
		return false;
	}
	static constexpr std::array<const char *, 7> allowed = {
		"date", "time", "title", "score", "trigger", "orientation", "session",
	};
	std::size_t position = 0;
	while ((position = value.find_first_of("{}", position)) != std::string::npos) {
		if (value[position] != '{') {
			return false;
		}
		const auto end = value.find('}', position + 1);
		if (end == std::string::npos) {
			return false;
		}
		const auto token = value.substr(position + 1, end - position - 1);
		if (std::none_of(allowed.begin(), allowed.end(),
				 [&token](const char *candidate) { return token == candidate; })) {
			return false;
		}
		position = end + 1;
	}
	return true;
}

bool validateSettings(const Settings &settings, std::string *error)
{
	if (error != nullptr) {
		error->clear();
	}
	if (settings.schemaVersion != settings_constraints::kSchemaVersion || settings.language.empty()) {
		return fail(error, "unsupported schema or empty language");
	}
	if (settings.defaultDurationSeconds < settings_constraints::kMinClipDurationSeconds ||
	    settings.defaultDurationSeconds > settings_constraints::kMaxClipDurationSeconds ||
	    settings.preRollSeconds < 0 || settings.preRollSeconds > settings_constraints::kMaxRollSeconds ||
	    settings.postRollSeconds < 0 || settings.postRollSeconds > settings_constraints::kMaxRollSeconds ||
	    settings.defaultDurationSeconds + settings.preRollSeconds + settings.postRollSeconds >
		    settings_constraints::kMaxClipDurationSeconds) {
		return fail(error, "capture timing is outside the supported range");
	}
	if (!enumInRange(settings.outputMode, 2) ||
	    settings.freeClipLimitPerSession != settings_constraints::kFreeClipLimitPerSession) {
		return fail(error, "capture output or Free limit is invalid");
	}
	if (settings.quickDurationsSeconds.size() != 5 ||
	    std::any_of(settings.quickDurationsSeconds.begin(), settings.quickDurationsSeconds.end(), [](int seconds) {
		    return seconds < settings_constraints::kMinClipDurationSeconds ||
			   seconds > settings_constraints::kMaxClipDurationSeconds;
	    })) {
		return fail(error, "quick durations are invalid");
	}
	if (!isValidSettingsDirectory(settings.clipDirectory) || !isValidSettingsDirectory(settings.exportDirectory) ||
	    !isValidSettingsDirectory(settings.thumbnailDirectory)) {
		return fail(error, "media paths must be empty or absolute directories");
	}
	if (!isValidFileNameTemplate(settings.fileNameTemplate)) {
		return fail(error, "file name template is invalid");
	}
	const std::array<std::string, 6> hotkeys = {
		settings.markMomentHotkey, settings.save15Hotkey,       settings.save30Hotkey,
		settings.save60Hotkey,     settings.saveVerticalHotkey, settings.openVerticalDesignerHotkey,
	};
	for (std::size_t index = 0; index < hotkeys.size(); ++index) {
		if (hotkeys[index].size() > static_cast<std::size_t>(settings_constraints::kMaxHotkeyLength) ||
		    std::any_of(hotkeys[index].begin(), hotkeys[index].end(),
				[](unsigned char value) { return std::iscntrl(value) != 0; })) {
			return fail(error, "hotkey value is invalid");
		}
		if (!hotkeys[index].empty() && std::find(hotkeys.begin() + static_cast<std::ptrdiff_t>(index + 1),
							 hotkeys.end(), hotkeys[index]) != hotkeys.end()) {
			return fail(error, "hotkeys must not conflict");
		}
	}
	if (!enumInRange(settings.notificationDetail, 2) || !enumInRange(settings.exportContainer, 1) ||
	    !enumInRange(settings.exportCodec, 2) || !enumInRange(settings.exportQuality, 3) ||
	    !isKnownVerticalResolution(settings.verticalResolution) || !enumInRange(settings.verticalTemplate, 3) ||
	    std::find(std::begin(settings_constraints::kSupportedExportFps),
		      std::end(settings_constraints::kSupportedExportFps),
		      settings.exportFps) == std::end(settings_constraints::kSupportedExportFps)) {
		return fail(error, "notification or export settings are invalid");
	}
	const auto validTriggerText = [](const std::string &value) {
		return value.size() <= static_cast<std::size_t>(settings_constraints::kMaxTriggerListLength) &&
		       std::none_of(value.begin(), value.end(), [](unsigned char character) {
			       return character != '\t' && character != '\n' && std::iscntrl(character) != 0;
		       });
	};
	if (settings.triggerSensitivity < 0 || settings.triggerSensitivity > 100 ||
	    settings.triggerCooldownSeconds < 0 || settings.triggerCooldownSeconds > 3600 ||
	    !enumInRange(settings.triggerAction, 4) || !validTriggerText(settings.triggerKeywordsCsv) ||
	    !validTriggerText(settings.triggerScenesCsv)) {
		return fail(error, "smart trigger settings are invalid");
	}
	const auto supportedVoiceLanguage =
		settings.voiceTriggerLanguage == "auto" || settings.voiceTriggerLanguage == "es" ||
		settings.voiceTriggerLanguage == "en" || settings.voiceTriggerLanguage.rfind("es-", 0) == 0 ||
		settings.voiceTriggerLanguage.rfind("en-", 0) == 0;
	if (settings.voiceTriggerPhrasesCsv.empty() ||
	    settings.voiceTriggerPhrasesCsv.size() >
		    static_cast<std::size_t>(settings_constraints::kMaxVoicePhraseListLength) ||
	    !validTriggerText(settings.voiceTriggerPhrasesCsv) || !supportedVoiceLanguage ||
	    settings.voiceAudioSourceName.size() > 512 ||
	    !validTriggerText(settings.voiceAudioSourceName) ||
	    settings.voiceSpeechMode != SpeechProcessingMode::Local ||
	    settings.voiceCloudConsent) {
		return fail(error, "voice trigger settings are invalid");
	}
	if (settings.aiLanguage != "auto" && settings.aiLanguage != "es" && settings.aiLanguage != "en")
		return fail(error, "AI Assistant language is invalid");
	if (settings.verticalSceneName.size() > 512 || settings.verticalSourceName.size() > 512 ||
	    !validTriggerText(settings.verticalSceneName) || !validTriggerText(settings.verticalSourceName))
		return fail(error, "vertical OBS scene or source selection is invalid");
	if (settings.verticalZoomPercent < 100 || settings.verticalZoomPercent > 300 ||
	    settings.verticalPanXPercent < -100 || settings.verticalPanXPercent > 100 ||
	    settings.verticalPanYPercent < -100 || settings.verticalPanYPercent > 100)
		return fail(error, "vertical framing settings are invalid");
	if (settings.verticalWidth <= 0 || settings.verticalHeight <= 0) {
		return fail(error, "vertical dimensions must be positive");
	}
	if (!isNineBySixteen(settings.verticalWidth, settings.verticalHeight)) {
		return fail(error, "vertical dimensions must use an exact 9:16 ratio");
	}
	const auto dimensions = verticalResolutionDimensions(settings.verticalResolution);
	if (settings.verticalResolution != VerticalResolution::Custom &&
	    (settings.verticalWidth != dimensions.width || settings.verticalHeight != dimensions.height)) {
		return fail(error, "vertical resolution and dimensions are inconsistent");
	}
	return true;
}

} // namespace clipcoach
