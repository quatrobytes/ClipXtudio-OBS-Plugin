#include <clipcoach/core/settings-manager.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>

namespace clipcoach {
namespace {

void clearError(std::string *error)
{
	if (error != nullptr) {
		error->clear();
	}
}

bool fail(std::string *error, std::string message)
{
	if (error != nullptr) {
		*error = std::move(message);
	}
	return false;
}

void skipWhitespace(const std::string &document, std::size_t &position)
{
	while (position < document.size() && std::isspace(static_cast<unsigned char>(document[position])) != 0) {
		++position;
	}
}

std::optional<std::size_t> valuePosition(const std::string &document, const std::string &key)
{
	const auto keyPosition = document.find('"' + key + '"');
	if (keyPosition == std::string::npos) {
		return std::nullopt;
	}

	const auto colonPosition = document.find(':', keyPosition + key.size() + 2);
	if (colonPosition == std::string::npos) {
		return std::nullopt;
	}

	auto position = colonPosition + 1;
	skipWhitespace(document, position);
	return position;
}

std::optional<int> readInteger(const std::string &document, const std::string &key)
{
	auto position = valuePosition(document, key);
	if (!position.has_value() || *position >= document.size()) {
		return std::nullopt;
	}

	const auto start = *position;
	auto end = start;
	if (document[end] == '-') {
		++end;
	}
	while (end < document.size() && std::isdigit(static_cast<unsigned char>(document[end])) != 0) {
		++end;
	}
	if (end == start || (document[start] == '-' && end == start + 1)) {
		return std::nullopt;
	}

	try {
		std::size_t parsedCharacters = 0;
		const auto value = std::stoi(document.substr(start, end - start), &parsedCharacters);
		if (parsedCharacters != end - start) {
			return std::nullopt;
		}
		return value;
	} catch (...) {
		return std::nullopt;
	}
}

std::optional<bool> readBoolean(const std::string &document, const std::string &key)
{
	const auto position = valuePosition(document, key);
	if (!position.has_value()) {
		return std::nullopt;
	}
	if (document.compare(*position, 4, "true") == 0) {
		return true;
	}
	if (document.compare(*position, 5, "false") == 0) {
		return false;
	}
	return std::nullopt;
}

std::optional<std::vector<int>> readIntegerArray(const std::string &document, const std::string &key)
{
	auto position = valuePosition(document, key);
	if (!position.has_value() || *position >= document.size() || document[*position] != '[') {
		return std::nullopt;
	}

	std::vector<int> values;
	++(*position);
	while (*position < document.size()) {
		skipWhitespace(document, *position);
		if (*position < document.size() && document[*position] == ']') {
			return values;
		}

		const auto start = *position;
		while (*position < document.size() &&
		       std::isdigit(static_cast<unsigned char>(document[*position])) != 0) {
			++(*position);
		}
		if (start == *position) {
			return std::nullopt;
		}

		try {
			values.push_back(std::stoi(document.substr(start, *position - start)));
		} catch (...) {
			return std::nullopt;
		}

		skipWhitespace(document, *position);
		if (*position < document.size() && document[*position] == ',') {
			++(*position);
			continue;
		}
		if (*position < document.size() && document[*position] == ']') {
			return values;
		}
		return std::nullopt;
	}
	return std::nullopt;
}

std::optional<std::string> readString(const std::string &document, const std::string &key)
{
	auto position = valuePosition(document, key);
	if (!position.has_value() || *position >= document.size() || document[*position] != '"') {
		return std::nullopt;
	}

	++(*position);
	std::string value;
	bool escaped = false;
	for (; *position < document.size(); ++(*position)) {
		const auto character = document[*position];
		if (escaped) {
			switch (character) {
			case '"':
			case '\\':
			case '/':
				value.push_back(character);
				break;
			case 'b':
				value.push_back('\b');
				break;
			case 'f':
				value.push_back('\f');
				break;
			case 'n':
				value.push_back('\n');
				break;
			case 'r':
				value.push_back('\r');
				break;
			case 't':
				value.push_back('\t');
				break;
			default:
				return std::nullopt;
			}
			escaped = false;
			continue;
		}
		if (character == '\\') {
			escaped = true;
			continue;
		}
		if (character == '"') {
			return value;
		}
		value.push_back(character);
	}
	return std::nullopt;
}

std::string escapeJsonString(const std::string &value)
{
	std::string escaped;
	escaped.reserve(value.size());
	for (const auto character : value) {
		switch (character) {
		case '"':
			escaped += "\\\"";
			break;
		case '\\':
			escaped += "\\\\";
			break;
		case '\b':
			escaped += "\\b";
			break;
		case '\f':
			escaped += "\\f";
			break;
		case '\n':
			escaped += "\\n";
			break;
		case '\r':
			escaped += "\\r";
			break;
		case '\t':
			escaped += "\\t";
			break;
		default:
			escaped.push_back(character);
			break;
		}
	}
	return escaped;
}

std::string serialize(const Settings &settings)
{
	std::ostringstream output;
	output << "{\n"
	       << "  \"schema_version\": " << settings.schemaVersion << ",\n"
	       << "  \"language\": \"" << escapeJsonString(settings.language) << "\",\n"
	       << "  \"start_with_obs\": " << (settings.startWithObs ? "true" : "false") << ",\n"
	       << "  \"open_dock_at_startup\": " << (settings.openDockAtStartup ? "true" : "false") << ",\n"
	       << "  \"initial_setup_completed\": " << (settings.initialSetupCompleted ? "true" : "false") << ",\n"
	       << "  \"initial_setup_completed_version\": \"" << escapeJsonString(settings.initialSetupCompletedVersion)
	       << "\",\n"
	       << "  \"auto_start_replay_buffer\": " << (settings.autoStartReplayBuffer ? "true" : "false") << ",\n"
	       << "  \"remote_commands_enabled\": " << (settings.remoteCommandsEnabled ? "true" : "false") << ",\n"
	       << "  \"default_duration_seconds\": " << settings.defaultDurationSeconds << ",\n"
	       << "  \"pre_roll_seconds\": " << settings.preRollSeconds << ",\n"
	       << "  \"post_roll_seconds\": " << settings.postRollSeconds << ",\n"
	       << "  \"output_mode\": " << static_cast<int>(settings.outputMode) << ",\n"
	       << "  \"free_clip_limit_per_session\": " << settings.freeClipLimitPerSession << ",\n"
	       << "  \"confirm_before_delete\": " << (settings.confirmBeforeDelete ? "true" : "false") << ",\n"
	       << "  \"clip_directory\": \"" << escapeJsonString(settings.clipDirectory.u8string()) << "\",\n"
	       << "  \"export_directory\": \"" << escapeJsonString(settings.exportDirectory.u8string()) << "\",\n"
	       << "  \"thumbnail_directory\": \"" << escapeJsonString(settings.thumbnailDirectory.u8string()) << "\",\n"
	       << "  \"include_date_in_file_name\": " << (settings.includeDateInFileName ? "true" : "false") << ",\n"
	       << "  \"include_score_in_file_name\": " << (settings.includeScoreInFileName ? "true" : "false") << ",\n"
	       << "  \"include_trigger_in_file_name\": " << (settings.includeTriggerInFileName ? "true" : "false")
	       << ",\n"
	       << "  \"include_orientation_in_file_name\": "
	       << (settings.includeOrientationInFileName ? "true" : "false") << ",\n"
	       << "  \"file_name_template\": \"" << escapeJsonString(settings.fileNameTemplate) << "\",\n"
	       << "  \"notifications_enabled\": " << (settings.notificationsEnabled ? "true" : "false") << ",\n"
	       << "  \"sound_on_save\": " << (settings.soundOnSave ? "true" : "false") << ",\n"
	       << "  \"system_toast_enabled\": " << (settings.systemToastEnabled ? "true" : "false") << ",\n"
	       << "  \"notification_detail\": " << static_cast<int>(settings.notificationDetail) << ",\n"
	       << "  \"mark_moment_hotkey\": \"" << escapeJsonString(settings.markMomentHotkey) << "\",\n"
	       << "  \"save_15_hotkey\": \"" << escapeJsonString(settings.save15Hotkey) << "\",\n"
	       << "  \"save_30_hotkey\": \"" << escapeJsonString(settings.save30Hotkey) << "\",\n"
	       << "  \"save_60_hotkey\": \"" << escapeJsonString(settings.save60Hotkey) << "\",\n"
	       << "  \"save_vertical_hotkey\": \"" << escapeJsonString(settings.saveVerticalHotkey) << "\",\n"
	       << "  \"open_vertical_designer_hotkey\": \"" << escapeJsonString(settings.openVerticalDesignerHotkey)
	       << "\",\n"
	       << "  \"export_container\": " << static_cast<int>(settings.exportContainer) << ",\n"
	       << "  \"export_codec\": " << static_cast<int>(settings.exportCodec) << ",\n"
	       << "  \"export_fps\": " << settings.exportFps << ",\n"
	       << "  \"export_quality\": " << static_cast<int>(settings.exportQuality) << ",\n"
	       << "  \"vertical_resolution\": " << static_cast<int>(settings.verticalResolution) << ",\n"
	       << "  \"vertical_width\": " << settings.verticalWidth << ",\n"
	       << "  \"vertical_height\": " << settings.verticalHeight << ",\n"
	       << "  \"vertical_template\": " << static_cast<int>(settings.verticalTemplate) << ",\n"
	       << "  \"vertical_element_gameplay\": " << (settings.verticalElementsEnabled[0] ? "true" : "false")
	       << ",\n"
	       << "  \"vertical_element_camera\": " << (settings.verticalElementsEnabled[1] ? "true" : "false") << ",\n"
	       << "  \"vertical_element_subtitles\": " << (settings.verticalElementsEnabled[2] ? "true" : "false")
	       << ",\n"
	       << "  \"vertical_element_title\": " << (settings.verticalElementsEnabled[3] ? "true" : "false") << ",\n"
	       << "  \"vertical_element_logo\": " << (settings.verticalElementsEnabled[4] ? "true" : "false") << ",\n"
	       << "  \"vertical_element_chat\": " << (settings.verticalElementsEnabled[5] ? "true" : "false") << ",\n"
	       << "  \"vertical_scene_name\": \"" << escapeJsonString(settings.verticalSceneName) << "\",\n"
	       << "  \"vertical_source_name\": \"" << escapeJsonString(settings.verticalSourceName) << "\",\n"
	       << "  \"vertical_zoom_percent\": " << settings.verticalZoomPercent << ",\n"
	       << "  \"vertical_pan_x_percent\": " << settings.verticalPanXPercent << ",\n"
	       << "  \"vertical_pan_y_percent\": " << settings.verticalPanYPercent << ",\n"
	       << "  \"trigger_manual_enabled\": " << (settings.triggerManualEnabled ? "true" : "false") << ",\n"
	       << "  \"trigger_voice_enabled\": " << (settings.triggerVoiceEnabled ? "true" : "false") << ",\n"
	       << "  \"trigger_audio_spike_enabled\": " << (settings.triggerAudioSpikeEnabled ? "true" : "false")
	       << ",\n"
	       << "  \"trigger_chat_pulse_enabled\": " << (settings.triggerChatPulseEnabled ? "true" : "false") << ",\n"
	       << "  \"trigger_scene_enabled\": " << (settings.triggerSceneEnabled ? "true" : "false") << ",\n"
	       << "  \"trigger_keyword_enabled\": " << (settings.triggerKeywordEnabled ? "true" : "false") << ",\n"
	       << "  \"trigger_ai_hook_enabled\": " << (settings.triggerAiHookEnabled ? "true" : "false") << ",\n"
	       << "  \"trigger_sensitivity\": " << settings.triggerSensitivity << ",\n"
	       << "  \"trigger_action\": " << static_cast<int>(settings.triggerAction) << ",\n"
	       << "  \"trigger_keywords\": \"" << escapeJsonString(settings.triggerKeywordsCsv) << "\",\n"
	       << "  \"trigger_scenes\": \"" << escapeJsonString(settings.triggerScenesCsv) << "\",\n"
	       << "  \"trigger_cooldown_seconds\": " << settings.triggerCooldownSeconds << ",\n"
	       << "  \"voice_trigger_phrases\": \"" << escapeJsonString(settings.voiceTriggerPhrasesCsv) << "\",\n"
	       << "  \"voice_audio_source_name\": \"" << escapeJsonString(settings.voiceAudioSourceName) << "\",\n"
	       << "  \"voice_trigger_language\": \"" << escapeJsonString(settings.voiceTriggerLanguage) << "\",\n"
	       << "  \"voice_speech_mode\": " << static_cast<int>(settings.voiceSpeechMode) << ",\n"
	       << "  \"voice_cloud_consent\": " << (settings.voiceCloudConsent ? "true" : "false") << ",\n"
	       << "  \"ai_assistant_enabled\": " << (settings.aiAssistantEnabled ? "true" : "false") << ",\n"
	       << "  \"ai_privacy_consent\": " << (settings.aiPrivacyConsent ? "true" : "false") << ",\n"
	       << "  \"ai_language\": \"" << escapeJsonString(settings.aiLanguage) << "\",\n"
	       << "  \"quick_durations_seconds\": [";
	for (std::size_t index = 0; index < settings.quickDurationsSeconds.size(); ++index) {
		if (index > 0) {
			output << ", ";
		}
		output << settings.quickDurationsSeconds[index];
	}
	output << "]\n"
	       << "}\n";
	return output.str();
}

bool deserialize(const std::string &document, Settings &settings, std::string *error)
{
	const auto schemaVersion = readInteger(document, "schema_version");
	const auto language = readString(document, "language");
	const auto notificationsEnabled = readBoolean(document, "notifications_enabled");
	const auto verticalWidth = readInteger(document, "vertical_width");
	const auto verticalHeight = readInteger(document, "vertical_height");
	const auto quickDurations = readIntegerArray(document, "quick_durations_seconds");

	if (!schemaVersion || !language || !notificationsEnabled || !verticalWidth || !verticalHeight) {
		return fail(error, "settings file is missing a required field or contains an invalid value");
	}
	if (*schemaVersion < 1 || *schemaVersion > settings_constraints::kSchemaVersion) {
		return fail(error, "settings schema version is not supported");
	}
	if (language->empty()) {
		return fail(error, "settings language cannot be empty");
	}
	if (*verticalWidth <= 0 || *verticalHeight <= 0) {
		return fail(error, "vertical dimensions must be positive");
	}
	if (quickDurations.has_value() &&
	    (quickDurations->size() != 5 || std::any_of(quickDurations->begin(), quickDurations->end(),
							[](int seconds) { return seconds < 5 || seconds > 300; }))) {
		return fail(error, "quick durations must contain five values between 5 and 300 seconds");
	}

	settings = Settings::defaults();
	settings.language = *language;
	settings.notificationsEnabled = *notificationsEnabled;
	settings.verticalWidth = *verticalWidth;
	settings.verticalHeight = *verticalHeight;
	settings.quickDurationsSeconds = quickDurations.value_or(Settings::defaults().quickDurationsSeconds);
	if (*schemaVersion == 1) {
		return true;
	}

	const auto requireBool = [&document](const char *key, bool &target) {
		const auto value = readBoolean(document, key);
		if (!value) {
			return false;
		}
		target = *value;
		return true;
	};
	const auto requireInt = [&document](const char *key, int &target) {
		const auto value = readInteger(document, key);
		if (!value) {
			return false;
		}
		target = *value;
		return true;
	};
	const auto requireString = [&document](const char *key, std::string &target) {
		const auto value = readString(document, key);
		if (!value) {
			return false;
		}
		target = *value;
		return true;
	};

	int outputMode = 0;
	int notificationDetail = 0;
	int exportContainer = 0;
	int exportCodec = 0;
	int exportQuality = 0;
	int verticalResolution = 0;
	int verticalTemplate = 0;
	std::string clipDirectory;
	std::string exportDirectory;
	std::string thumbnailDirectory;
	const bool complete =
		requireBool("start_with_obs", settings.startWithObs) &&
		requireBool("open_dock_at_startup", settings.openDockAtStartup) &&
		requireBool("auto_start_replay_buffer", settings.autoStartReplayBuffer) &&
		requireInt("default_duration_seconds", settings.defaultDurationSeconds) &&
		requireInt("pre_roll_seconds", settings.preRollSeconds) &&
		requireInt("post_roll_seconds", settings.postRollSeconds) && requireInt("output_mode", outputMode) &&
		requireInt("free_clip_limit_per_session", settings.freeClipLimitPerSession) &&
		requireBool("confirm_before_delete", settings.confirmBeforeDelete) &&
		requireString("clip_directory", clipDirectory) && requireString("export_directory", exportDirectory) &&
		requireString("thumbnail_directory", thumbnailDirectory) &&
		requireBool("include_date_in_file_name", settings.includeDateInFileName) &&
		requireBool("include_score_in_file_name", settings.includeScoreInFileName) &&
		requireBool("include_trigger_in_file_name", settings.includeTriggerInFileName) &&
		requireBool("include_orientation_in_file_name", settings.includeOrientationInFileName) &&
		requireString("file_name_template", settings.fileNameTemplate) &&
		requireBool("sound_on_save", settings.soundOnSave) &&
		requireBool("system_toast_enabled", settings.systemToastEnabled) &&
		requireInt("notification_detail", notificationDetail) &&
		requireString("mark_moment_hotkey", settings.markMomentHotkey) &&
		requireString("save_15_hotkey", settings.save15Hotkey) &&
		requireString("save_30_hotkey", settings.save30Hotkey) &&
		requireString("save_60_hotkey", settings.save60Hotkey) &&
		requireString("save_vertical_hotkey", settings.saveVerticalHotkey) &&
		requireString("open_vertical_designer_hotkey", settings.openVerticalDesignerHotkey) &&
		requireInt("export_container", exportContainer) && requireInt("export_codec", exportCodec) &&
		requireInt("export_fps", settings.exportFps) && requireInt("export_quality", exportQuality) &&
		requireInt("vertical_resolution", verticalResolution);
	if (!complete) {
		return fail(error, "settings file is missing a schema v2 field");
	}
	settings.outputMode = static_cast<CaptureOutputMode>(outputMode);
	settings.notificationDetail = static_cast<NotificationDetail>(notificationDetail);
	settings.exportContainer = static_cast<ExportContainer>(exportContainer);
	settings.exportCodec = static_cast<ExportCodec>(exportCodec);
	settings.exportQuality = static_cast<ExportQuality>(exportQuality);
	settings.verticalResolution = static_cast<VerticalResolution>(verticalResolution);
	if (!isKnownVerticalResolution(settings.verticalResolution)) {
		return fail(error, "settings file contains an unknown vertical resolution");
	}
	settings.clipDirectory = std::filesystem::u8path(clipDirectory);
	settings.exportDirectory = std::filesystem::u8path(exportDirectory);
	settings.thumbnailDirectory = std::filesystem::u8path(thumbnailDirectory);
	if (*schemaVersion == 2) {
		settings.verticalResolution = settings.verticalWidth == 1080 && settings.verticalHeight == 1920
						      ? VerticalResolution::Portrait1080
						      : VerticalResolution::Custom;
		if (!validateSettings(settings, error)) {
			return false;
		}
		return true;
	}
	const bool verticalComplete = requireInt("vertical_template", verticalTemplate) &&
				      requireBool("vertical_element_gameplay", settings.verticalElementsEnabled[0]) &&
				      requireBool("vertical_element_camera", settings.verticalElementsEnabled[1]) &&
				      requireBool("vertical_element_subtitles", settings.verticalElementsEnabled[2]) &&
				      requireBool("vertical_element_title", settings.verticalElementsEnabled[3]) &&
				      requireBool("vertical_element_logo", settings.verticalElementsEnabled[4]) &&
				      requireBool("vertical_element_chat", settings.verticalElementsEnabled[5]);
	if (!verticalComplete) {
		return fail(error, "settings file is missing a schema v3 vertical field");
	}
	settings.verticalTemplate = static_cast<VerticalTemplateId>(verticalTemplate);
	if (*schemaVersion == 3) {
		if (!validateSettings(settings, error)) {
			return false;
		}
		return true;
	}
	int triggerAction = 0;
	const bool triggerComplete = requireBool("trigger_manual_enabled", settings.triggerManualEnabled) &&
				     requireBool("trigger_voice_enabled", settings.triggerVoiceEnabled) &&
				     requireBool("trigger_audio_spike_enabled", settings.triggerAudioSpikeEnabled) &&
				     requireBool("trigger_chat_pulse_enabled", settings.triggerChatPulseEnabled) &&
				     requireBool("trigger_scene_enabled", settings.triggerSceneEnabled) &&
				     requireBool("trigger_keyword_enabled", settings.triggerKeywordEnabled) &&
				     requireBool("trigger_ai_hook_enabled", settings.triggerAiHookEnabled) &&
				     requireInt("trigger_sensitivity", settings.triggerSensitivity) &&
				     requireInt("trigger_action", triggerAction) &&
				     requireString("trigger_keywords", settings.triggerKeywordsCsv) &&
				     requireString("trigger_scenes", settings.triggerScenesCsv) &&
				     requireInt("trigger_cooldown_seconds", settings.triggerCooldownSeconds);
	if (!triggerComplete) {
		return fail(error, "settings file is missing a schema v4 trigger field");
	}
	settings.triggerAction = static_cast<TriggerAction>(triggerAction);
	if (*schemaVersion == 4) {
		if (!validateSettings(settings, error)) {
			return false;
		}
		return true;
	}
	int voiceSpeechMode = 0;
	const bool voiceComplete = requireString("voice_trigger_phrases", settings.voiceTriggerPhrasesCsv) &&
				   requireString("voice_trigger_language", settings.voiceTriggerLanguage) &&
				   requireInt("voice_speech_mode", voiceSpeechMode) &&
				   requireBool("voice_cloud_consent", settings.voiceCloudConsent);
	if (!voiceComplete) {
		return fail(error, "settings file is missing a schema v5 voice field");
	}
	settings.voiceSpeechMode = static_cast<SpeechProcessingMode>(voiceSpeechMode);
	if (*schemaVersion == 5) {
		if (!validateSettings(settings, error))
			return false;
		return true;
	}
	const bool aiComplete = requireBool("ai_assistant_enabled", settings.aiAssistantEnabled) &&
				requireBool("ai_privacy_consent", settings.aiPrivacyConsent) &&
				requireString("ai_language", settings.aiLanguage);
	if (!aiComplete)
		return fail(error, "settings file is missing a schema v6 AI field");
	if (*schemaVersion == 6 && settings.voiceTriggerPhrasesCsv.find("saca clip") == std::string::npos) {
		settings.voiceTriggerPhrasesCsv += ",saca clip,save clip,save that";
	}
	if (*schemaVersion >= 8 && !requireString("voice_audio_source_name", settings.voiceAudioSourceName))
		return fail(error, "settings file is missing the schema v8 OBS audio source field");
	if (*schemaVersion < 9) {
		// Schema v9 replaces the legacy OS-language speech backend with bundled
		// multilingual recognition. Existing installs start bilingual once and
		// can still explicitly select Spanish or English afterward.
		settings.voiceTriggerLanguage = "auto";
	}
	if (*schemaVersion >= 10 && (!requireString("vertical_scene_name", settings.verticalSceneName) ||
				     !requireString("vertical_source_name", settings.verticalSourceName) ||
				     !requireInt("vertical_zoom_percent", settings.verticalZoomPercent) ||
				     !requireInt("vertical_pan_x_percent", settings.verticalPanXPercent) ||
				     !requireInt("vertical_pan_y_percent", settings.verticalPanYPercent))) {
		return fail(error, "settings file is missing the schema v10 vertical OBS selection");
	}
	if (*schemaVersion >= 12 && !requireBool("initial_setup_completed", settings.initialSetupCompleted))
		return fail(error, "settings file is missing the schema v12 initial setup state");
	if (*schemaVersion >= 13 && !requireBool("remote_commands_enabled", settings.remoteCommandsEnabled))
		return fail(error, "settings file is missing the schema v13 Remote Clipper state");
	if (*schemaVersion >= 14 &&
	    !requireString("initial_setup_completed_version", settings.initialSetupCompletedVersion))
		return fail(error, "settings file is missing the schema v14 setup version");
	if (*schemaVersion < 11) {
		// Cloud speech was previously visible before an authenticated backend
		// transport existed. Migrate those installs to the bundled local
		// recognizer so Voice Trigger remains functional after updating.
		settings.voiceSpeechMode = SpeechProcessingMode::Local;
		settings.voiceCloudConsent = false;
	}
	if (!validateSettings(settings, error)) {
		return false;
	}
	return true;
}

bool publishTemporaryFile(const std::filesystem::path &temporaryPath, const std::filesystem::path &destinationPath,
			  std::string *error)
{
	std::error_code fileError;
	std::filesystem::rename(temporaryPath, destinationPath, fileError);
	if (!fileError) {
		return true;
	}

	const auto backupPath = destinationPath.string() + ".bak";
	std::filesystem::remove(backupPath, fileError);
	fileError.clear();

	if (std::filesystem::exists(destinationPath, fileError)) {
		fileError.clear();
		std::filesystem::rename(destinationPath, backupPath, fileError);
		if (fileError) {
			return fail(error, "could not prepare the existing settings file for replacement");
		}
	}

	fileError.clear();
	std::filesystem::rename(temporaryPath, destinationPath, fileError);
	if (!fileError) {
		std::filesystem::remove(backupPath, fileError);
		return true;
	}

	std::error_code rollbackError;
	if (std::filesystem::exists(backupPath, rollbackError)) {
		std::filesystem::rename(backupPath, destinationPath, rollbackError);
	}
	return fail(error, "could not publish the new settings file");
}

} // namespace

SettingsManager::SettingsManager(std::filesystem::path settingsPath) : settingsPath_(std::move(settingsPath)) {}

bool SettingsManager::load(std::string *error)
{
	clearError(error);
	std::error_code fileError;
	if (!std::filesystem::exists(settingsPath_, fileError)) {
		if (fileError) {
			return fail(error, "could not inspect the settings path");
		}
		settings_ = Settings::defaults();
		return save(settings_, error);
	}

	std::ifstream input(settingsPath_, std::ios::binary);
	if (!input) {
		return fail(error, "could not open the settings file");
	}

	std::ostringstream document;
	document << input.rdbuf();
	if (input.bad()) {
		return fail(error, "could not read the settings file");
	}
	input.close();

	Settings loaded;
	if (!deserialize(document.str(), loaded, error)) {
		return false;
	}
	const bool repairedEmptyVerticalScene = loaded.verticalSceneName.empty();
	if (repairedEmptyVerticalScene)
		loaded.verticalSceneName = settings_constraints::kDefaultVerticalSceneName;
	settings_ = std::move(loaded);
	if (*readInteger(document.str(), "schema_version") != settings_constraints::kSchemaVersion ||
	    repairedEmptyVerticalScene) {
		return save(settings_, error);
	}
	return true;
}

bool SettingsManager::save(const Settings &settings, std::string *error)
{
	clearError(error);
	if (!validateSettings(settings, error)) {
		return false;
	}

	std::error_code fileError;
	const auto parentPath = settingsPath_.parent_path();
	if (!parentPath.empty()) {
		std::filesystem::create_directories(parentPath, fileError);
		if (fileError) {
			return fail(error, "could not create the settings directory");
		}
	}

	const auto temporaryPath = settingsPath_.string() + ".tmp";
	{
		std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
		if (!output) {
			return fail(error, "could not create the temporary settings file");
		}
		output << serialize(settings);
		output.flush();
		if (!output) {
			return fail(error, "could not write the temporary settings file");
		}
	}

	if (!publishTemporaryFile(temporaryPath, settingsPath_, error)) {
		std::filesystem::remove(temporaryPath, fileError);
		return false;
	}

	settings_ = settings;
	return true;
}

bool SettingsManager::exportProfile(const std::filesystem::path &profilePath, std::string *error) const
{
	clearError(error);
	if (profilePath.empty())
		return fail(error, "profile path cannot be empty");
	SettingsManager profile(profilePath);
	return profile.save(settings_, error);
}

bool SettingsManager::importProfile(const std::filesystem::path &profilePath, std::string *error)
{
	clearError(error);
	if (profilePath.empty())
		return fail(error, "profile path cannot be empty");
	SettingsManager profile(profilePath);
	if (!profile.load(error))
		return false;
	return save(profile.settings(), error);
}

const Settings &SettingsManager::settings() const noexcept
{
	return settings_;
}

const std::filesystem::path &SettingsManager::settingsPath() const noexcept
{
	return settingsPath_;
}

} // namespace clipcoach
