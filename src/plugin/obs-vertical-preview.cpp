#include <clipcoach/plugin/obs-vertical-preview.hpp>
#include <clipcoach/ui/components/preview-interaction-gate.hpp>

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <softpub.h>
#include <wintrust.h>
#endif

namespace clipcoach::plugin {
namespace {

bool containsInsensitive(const std::string &value, const char *needle)
{
	std::string lowered = value;
	std::transform(lowered.begin(), lowered.end(), lowered.begin(),
		       [](unsigned char character) {
			       return static_cast<char>(std::tolower(character));
		       });
	return lowered.find(needle) != std::string::npos;
}

bool equalsInsensitive(const char *value, const char *expected)
{
	if (value == nullptr || expected == nullptr)
		return false;
	while (*value != '\0' && *expected != '\0') {
		if (std::tolower(static_cast<unsigned char>(*value)) !=
		    std::tolower(static_cast<unsigned char>(*expected)))
			return false;
		++value;
		++expected;
	}
	return *value == '\0' && *expected == '\0';
}

bool isMontillaKickOverlay(obs_source_t *source)
{
	return source != nullptr &&
	       equalsInsensitive(obs_source_get_name(source), "MontillaRX Kick");
}

VerticalElementType verticalElementTypeForSource(obs_source_t *source)
{
	const char *rawName = source == nullptr ? nullptr : obs_source_get_name(source);
	const char *rawId =
		source == nullptr ? nullptr : obs_source_get_unversioned_id(source);
	const std::string identity =
		(rawName == nullptr ? std::string{} : std::string(rawName)) + " " +
		(rawId == nullptr ? std::string{} : std::string(rawId));
	if (containsInsensitive(identity, "camera") ||
	    containsInsensitive(identity, "webcam") ||
	    containsInsensitive(identity, "facecam") ||
	    containsInsensitive(identity, "camara") ||
	    containsInsensitive(identity, "dshow_input") ||
	    containsInsensitive(identity, "av_capture_input") ||
	    containsInsensitive(identity, "v4l2_input"))
		return VerticalElementType::Camera;
	if (containsInsensitive(identity, "subtitle") ||
	    containsInsensitive(identity, "caption") ||
	    containsInsensitive(identity, "subtitulo"))
		return VerticalElementType::Subtitles;
	if (containsInsensitive(identity, "title") ||
	    containsInsensitive(identity, "titulo") ||
	    containsInsensitive(identity, "text"))
		return VerticalElementType::Title;
	if (containsInsensitive(identity, "logo"))
		return VerticalElementType::Logo;
	if (containsInsensitive(identity, "chat"))
		return VerticalElementType::Chat;
	return VerticalElementType::Gameplay;
}

void applyVerticalItemSettings(obs_sceneitem_t *item,
			       const VerticalCanvasSettings &settings)
{
	if (item == nullptr)
		return;
	obs_source_t *source = obs_sceneitem_get_source(item);
	const auto &element = settings.element(verticalElementTypeForSource(source));
	obs_sceneitem_set_visible(item, element.enabled);

	vec2 position{static_cast<float>(element.x * settings.width),
		      static_cast<float>(element.y * settings.height)};
	vec2 bounds{
		std::max(1.0F, static_cast<float>(element.width * settings.width)),
		std::max(1.0F, static_cast<float>(element.height * settings.height))};
	obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
	obs_sceneitem_set_pos(item, &position);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_OUTER);
	obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
	obs_sceneitem_set_bounds(item, &bounds);
	obs_sceneitem_set_order_position(item, element.zOrder);
}

void applyFullHeightCentered(obs_sceneitem_t *item,
			     const VerticalCanvasSettings &settings)
{
	if (item == nullptr)
		return;
	obs_source_t *source = obs_sceneitem_get_source(item);
	const auto sourceWidth = source == nullptr ? 0U : obs_source_get_width(source);
	const auto sourceHeight = source == nullptr ? 0U : obs_source_get_height(source);
	obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
	obs_sceneitem_set_rot(item, 0.0F);
	obs_sceneitem_crop crop{};
	obs_sceneitem_set_crop(item, &crop);
	if (sourceWidth == 0 || sourceHeight == 0) {
		vec2 position{};
		vec2 bounds{static_cast<float>(settings.width),
			    static_cast<float>(settings.height)};
		obs_sceneitem_set_pos(item, &position);
		obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_SCALE_INNER);
		obs_sceneitem_set_bounds_alignment(item, OBS_ALIGN_CENTER);
		obs_sceneitem_set_bounds(item, &bounds);
		return;
	}
	const auto scale = static_cast<float>(settings.height) /
			   static_cast<float>(sourceHeight);
	vec2 itemScale{scale, scale};
	vec2 position{
		(static_cast<float>(settings.width) -
		 static_cast<float>(sourceWidth) * scale) /
			2.0F,
		0.0F};
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
	obs_sceneitem_set_scale(item, &itemScale);
	obs_sceneitem_set_pos(item, &position);
}

void applyBottomAlignedOverlay(obs_sceneitem_t *item,
			       const VerticalCanvasSettings &settings)
{
	if (item == nullptr)
		return;
	obs_source_t *source = obs_sceneitem_get_source(item);
	const auto sourceWidth = source == nullptr ? 0U : obs_source_get_width(source);
	const auto sourceHeight = source == nullptr ? 0U : obs_source_get_height(source);
	if (sourceWidth == 0 || sourceHeight == 0)
		return;
	const auto scale = static_cast<float>(settings.width) /
			   static_cast<float>(sourceWidth);
	const auto renderedHeight = static_cast<float>(sourceHeight) * scale;
	vec2 itemScale{scale, scale};
	vec2 position{0.0F, std::max(0.0F,
					 static_cast<float>(settings.height) -
					 renderedHeight)};
	obs_sceneitem_crop crop{};
	obs_sceneitem_set_crop(item, &crop);
	obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
	obs_sceneitem_set_rot(item, 0.0F);
	obs_sceneitem_set_bounds_type(item, OBS_BOUNDS_NONE);
	obs_sceneitem_set_scale(item, &itemScale);
	obs_sceneitem_set_pos(item, &position);
}

void applyVerticalCompositionItem(obs_sceneitem_t *item,
				  const VerticalCanvasSettings &settings)
{
	if (item == nullptr)
		return;
	obs_source_t *source = obs_sceneitem_get_source(item);
	const auto type = verticalElementTypeForSource(source);
	applyVerticalItemSettings(item, settings);
	if (type == VerticalElementType::Camera)
		applyFullHeightCentered(item, settings);
	else if (isMontillaKickOverlay(source))
		applyBottomAlignedOverlay(item, settings);
}

struct ApplyVerticalSceneContext {
	const VerticalCanvasSettings *settings{nullptr};
	obs_sceneitem_t *priorityOverlay{nullptr};
};

bool applyVerticalSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *context)
{
	auto *state = static_cast<ApplyVerticalSceneContext *>(context);
	if (state != nullptr && state->settings != nullptr) {
		applyVerticalCompositionItem(item, *state->settings);
		if (isMontillaKickOverlay(obs_sceneitem_get_source(item)))
			state->priorityOverlay = item;
	}
	return true;
}

bool isHardwareEncoder(const std::string &id)
{
	return containsInsensitive(id, "nvenc") ||
	       containsInsensitive(id, "qsv") ||
	       containsInsensitive(id, "amf") ||
	       containsInsensitive(id, "apple") ||
	       containsInsensitive(id, "videotoolbox") ||
	       containsInsensitive(id, "vaapi");
}

std::string safeConfigString(config_t *config, const char *section,
			     const char *name)
{
	const char *value = config_get_string(config, section, name);
	return value == nullptr ? std::string{} : std::string(value);
}

struct SimpleEncoderMapping {
	const char *obsId;
	const char *profileId;
	const char *label;
};

constexpr SimpleEncoderMapping kSimpleEncoderMappings[] = {
	{"obs_nvenc_h264_tex", "nvenc", "NVIDIA NVENC H.264"},
	{"ffmpeg_nvenc", "nvenc", "NVIDIA NVENC H.264"},
	{"obs_qsv11_v2", "qsv", "Intel Quick Sync H.264"},
	{"h264_texture_amf", "amd", "AMD AMF H.264"},
	{"com.apple.videotoolbox.videoencoder.ave.avc", "apple_h264",
	 "Apple VideoToolbox H.264"},
	{"obs_x264", "x264", "Software x264"},
};

std::vector<std::string> availableVideoEncoderIds()
{
	std::vector<std::string> result;
	for (std::size_t index = 0;; ++index) {
		const char *id = nullptr;
		if (!obs_enum_encoder_types(index, &id))
			break;
		if (id == nullptr || obs_get_encoder_type(id) != OBS_ENCODER_VIDEO)
			continue;
		const auto caps = obs_get_encoder_caps(id);
		if ((caps & OBS_ENCODER_CAP_DEPRECATED) != 0 ||
		    (caps & OBS_ENCODER_CAP_INTERNAL) != 0)
			continue;
		const char *codec = obs_get_encoder_codec(id);
		if (codec == nullptr ||
		    (std::strcmp(codec, "h264") != 0 &&
		     std::strcmp(codec, "avc") != 0))
			continue;
		result.emplace_back(id);
	}
	return result;
}

std::string profileOutputMode(config_t *config)
{
	const auto mode = safeConfigString(config, "Output", "Mode");
	return mode == "Advanced" ? "Advanced" : "Simple";
}

std::vector<ui::ReplayEncoderOption> replayEncoderOptions()
{
	config_t *config = obs_frontend_get_profile_config();
	if (config == nullptr)
		return {};
	const auto mode = profileOutputMode(config);
	const auto available = availableVideoEncoderIds();
	std::vector<ui::ReplayEncoderOption> result;
	if (mode == "Simple") {
		for (const auto &mapping : kSimpleEncoderMappings) {
			if (std::find(available.begin(), available.end(), mapping.obsId) ==
			    available.end())
				continue;
			const auto duplicate = std::find_if(
				result.begin(), result.end(),
				[&mapping](const auto &option) {
					return option.id == mapping.profileId;
				});
			if (duplicate == result.end()) {
				result.push_back({mapping.profileId, mapping.label,
						  isHardwareEncoder(mapping.obsId)});
			}
		}
		if (std::find(available.begin(), available.end(), "obs_x264") !=
		    available.end()) {
			result.push_back(
				{"x264_lowcpu", "Software x264 (low CPU preset)", false});
		}
	} else {
		const auto streamEncoder =
			safeConfigString(config, "AdvOut", "Encoder");
		if (!streamEncoder.empty()) {
			const char *display =
				obs_encoder_get_display_name(streamEncoder.c_str());
			result.push_back(
				{"none",
				 std::string("Use streaming encoder: ") +
					 (display != nullptr ? display
							     : streamEncoder),
				 isHardwareEncoder(streamEncoder)});
		}
		for (const auto &id : available) {
			const char *display = obs_encoder_get_display_name(id.c_str());
			result.push_back(
				{id, display != nullptr ? display : id,
				 isHardwareEncoder(id)});
		}
	}
	std::stable_sort(result.begin(), result.end(),
			 [](const auto &left, const auto &right) {
				 return left.hardware > right.hardware;
			 });
	return result;
}

ui::ReplayProfileSettings replayProfileSettings()
{
	ui::ReplayProfileSettings result;
	config_t *config = obs_frontend_get_profile_config();
	if (config == nullptr)
		return result;
	result.outputMode = profileOutputMode(config);
	const char *section =
		result.outputMode == "Advanced" ? "AdvOut" : "SimpleOutput";
	const auto quality = safeConfigString(config, "SimpleOutput", "RecQuality");
	const bool simpleShared =
		result.outputMode == "Simple" && quality == "Stream";
	result.encoderId =
		result.outputMode == "Advanced"
			? safeConfigString(config, "AdvOut", "RecEncoder")
			: safeConfigString(config, "SimpleOutput",
					   simpleShared ? "StreamEncoder"
							: "RecEncoder");
	if (result.outputMode == "Advanced" && result.encoderId == "none")
		result.hardwareEncoder = isHardwareEncoder(
			safeConfigString(config, "AdvOut", "Encoder"));
	else
		result.hardwareEncoder = isHardwareEncoder(result.encoderId);
	for (const auto &option : replayEncoderOptions()) {
		if (option.id == result.encoderId) {
			result.encoderDisplayName = option.displayName;
			result.hardwareEncoder = option.hardware;
			break;
		}
	}
	if (result.encoderDisplayName.empty())
		result.encoderDisplayName =
			result.encoderId.empty() ? "Unknown encoder" : result.encoderId;
	result.replayBufferEnabled = config_get_bool(config, section, "RecRB");
	return result;
}

ui::ReplayProfileApplyResult applyReplayProfile(const std::string &encoderId,
						 bool enabled)
{
	if (obs_frontend_streaming_active() ||
	    obs_frontend_recording_active() ||
	    obs_frontend_replay_buffer_active()) {
		return {false, false,
			"Stop streaming, recording and Replay Buffer before changing the OBS encoder profile."};
	}
	const auto options = replayEncoderOptions();
	if (std::none_of(options.begin(), options.end(),
			 [&encoderId](const auto &option) {
				 return option.id == encoderId;
			 })) {
		return {false, false,
			"The selected H.264 encoder is no longer available in OBS."};
	}
	config_t *config = obs_frontend_get_profile_config();
	if (config == nullptr)
		return {false, false, "OBS did not provide the active profile configuration."};
	const auto mode = profileOutputMode(config);
	if (mode == "Advanced") {
		config_set_string(config, "AdvOut", "RecEncoder",
				  encoderId.c_str());
		config_set_bool(config, "AdvOut", "RecRB", enabled);
	} else {
		const bool sharedWithStream =
			safeConfigString(config, "SimpleOutput", "RecQuality") ==
			"Stream";
		config_set_string(config, "SimpleOutput",
				  sharedWithStream ? "StreamEncoder" : "RecEncoder",
				  encoderId.c_str());
		config_set_bool(config, "SimpleOutput", "RecRB", enabled);
	}
	if (config_save_safe(config, "tmp", "bak") != CONFIG_SUCCESS)
		return {false, false,
			"OBS could not save the active profile. Check profile folder permissions."};
	blog(LOG_INFO,
	     "[ClipXtudio] OBS replay profile saved: mode=%s encoder=%s enabled=%s; restart required",
	     mode.c_str(), encoderId.c_str(), enabled ? "true" : "false");
	return {true, true, "OBS replay profile saved."};
}

bool restartObs(std::string *error)
{
	if (obs_frontend_streaming_active() ||
	    obs_frontend_recording_active() ||
	    obs_frontend_replay_buffer_active()) {
		if (error != nullptr) {
			*error =
				"Stop streaming, recording and Replay Buffer before restarting OBS.";
		}
		return false;
	}
	auto *application = QCoreApplication::instance();
	auto *mainWindow =
		static_cast<QWidget *>(obs_frontend_get_main_window());
	auto executable = QCoreApplication::applicationFilePath();
#ifdef _WIN32
	// cmd.exe truncates Win32 extended paths such as \\?\C:\... to "\\"
	// when they are passed through `start`. Use a detached, hidden PowerShell
	// helper and a normal drive path instead.
	if (executable.startsWith(QStringLiteral("\\\\?\\")))
		executable.remove(0, 4);
	const auto restartHelper =
		QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
#endif
	if (application == nullptr || mainWindow == nullptr ||
	    executable.isEmpty() || !QFileInfo::exists(executable)
#ifdef _WIN32
	    || restartHelper.isEmpty()
#endif
	) {
		if (error != nullptr)
			*error =
				"OBS could not locate a valid executable or restart helper. Restart OBS manually.";
		return false;
	}

	auto connection = std::make_shared<QMetaObject::Connection>();
	*connection = QObject::connect(
		application, &QCoreApplication::aboutToQuit, application,
		[executable,
#ifdef _WIN32
		 restartHelper,
#endif
		 connection] {
#ifdef _WIN32
			QProcess helper;
			auto environment =
				QProcessEnvironment::systemEnvironment();
			environment.insert(
				QStringLiteral("CLIPXTUDIO_OBS_EXECUTABLE"),
				executable);
			environment.insert(
				QStringLiteral("CLIPXTUDIO_OBS_WORKDIR"),
				QFileInfo(executable).absolutePath());
			helper.setProcessEnvironment(environment);
			helper.setProgram(restartHelper);
			helper.setArguments(
				{QStringLiteral("-NoLogo"),
				 QStringLiteral("-NoProfile"),
				 QStringLiteral("-NonInteractive"),
				 QStringLiteral("-WindowStyle"),
				 QStringLiteral("Hidden"),
				 QStringLiteral("-Command"),
				 QStringLiteral(
					 "Start-Sleep -Milliseconds 1500; "
					 "Start-Process -FilePath "
					 "$env:CLIPXTUDIO_OBS_EXECUTABLE "
					 "-WorkingDirectory "
					 "$env:CLIPXTUDIO_OBS_WORKDIR")});
			const bool started = helper.startDetached();
#else
			const bool started = QProcess::startDetached(
				QStringLiteral("/bin/sh"),
				{QStringLiteral("-c"),
				 QStringLiteral("sleep 2; exec \"$1\""),
				 QStringLiteral("clipxtudio-restart"), executable});
#endif
			blog(started ? LOG_INFO : LOG_ERROR,
			     "[ClipXtudio] OBS restart helper %s",
			     started ? "started" : "could not start");
			QObject::disconnect(*connection);
		},
		Qt::SingleShotConnection);
	if (!*connection) {
		if (error != nullptr)
			*error = "OBS could not register the restart request.";
		return false;
	}

	blog(LOG_INFO,
	     "[ClipXtudio] Closing OBS for encoder profile restart; executable validated");
	QTimer::singleShot(0, mainWindow,
			   [mainWindow] { mainWindow->close(); });
	QTimer::singleShot(1500, mainWindow, [mainWindow, connection] {
		if (mainWindow->isVisible()) {
			QObject::disconnect(*connection);
			blog(LOG_INFO,
			     "[ClipXtudio] OBS restart canceled because the main window remained open");
		}
	});
	return true;
}

bool installUpdate(const std::string &installerPath, std::string *error)
{
#ifndef _WIN32
	(void)installerPath;
	if (error != nullptr)
		*error = "Automatic update installation is currently available on Windows.";
	return false;
#else
	if (obs_frontend_streaming_active() ||
	    obs_frontend_recording_active() ||
	    obs_frontend_replay_buffer_active()) {
		if (error != nullptr) {
			*error =
				"Stop streaming, recording and Replay Buffer before installing the update.";
		}
		return false;
	}

	const auto installer =
		QFileInfo(QString::fromStdString(installerPath)).absoluteFilePath();
#if !defined(CLIPX_ALLOW_INSECURE_LOCAL_API)
	WINTRUST_FILE_INFO fileInfo{};
	fileInfo.cbStruct = sizeof(fileInfo);
	fileInfo.pcwszFilePath =
		reinterpret_cast<LPCWSTR>(installer.utf16());
	WINTRUST_DATA trustData{};
	trustData.cbStruct = sizeof(trustData);
	trustData.dwUIChoice = WTD_UI_NONE;
	trustData.fdwRevocationChecks = WTD_REVOKE_WHOLECHAIN;
	trustData.dwUnionChoice = WTD_CHOICE_FILE;
	trustData.pFile = &fileInfo;
	trustData.dwStateAction = WTD_STATEACTION_VERIFY;
	trustData.dwProvFlags =
		WTD_REVOCATION_CHECK_CHAIN_EXCLUDE_ROOT | WTD_SAFER_FLAG;
	GUID policy = WINTRUST_ACTION_GENERIC_VERIFY_V2;
	const auto signatureStatus =
		WinVerifyTrust(nullptr, &policy, &trustData);
	trustData.dwStateAction = WTD_STATEACTION_CLOSE;
	(void)WinVerifyTrust(nullptr, &policy, &trustData);
	if (signatureStatus != ERROR_SUCCESS) {
		if (error != nullptr)
			*error =
				"The update installer is unsigned, untrusted or revoked.";
		return false;
	}
#endif
	auto executable = QCoreApplication::applicationFilePath();
	if (executable.startsWith(QStringLiteral("\\\\?\\")))
		executable.remove(0, 4);
	auto *application = QCoreApplication::instance();
	auto *mainWindow =
		static_cast<QWidget *>(obs_frontend_get_main_window());
	const auto helper =
		QStandardPaths::findExecutable(QStringLiteral("powershell.exe"));
	if (application == nullptr || mainWindow == nullptr ||
	    helper.isEmpty() || !QFileInfo::exists(installer) ||
	    !installer.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) ||
	    executable.isEmpty() || !QFileInfo::exists(executable)) {
		if (error != nullptr)
			*error = "The verified installer or OBS executable could not be located.";
		return false;
	}

	auto connection = std::make_shared<QMetaObject::Connection>();
	*connection = QObject::connect(
		application, &QCoreApplication::aboutToQuit, application,
		[installer, executable, helper, connection] {
			QProcess updater;
			auto environment = QProcessEnvironment::systemEnvironment();
			environment.insert(QStringLiteral("CLIPXTUDIO_UPDATE_INSTALLER"),
					   installer);
			environment.insert(QStringLiteral("CLIPXTUDIO_OBS_EXECUTABLE"),
					   executable);
			environment.insert(QStringLiteral("CLIPXTUDIO_OBS_WORKDIR"),
					   QFileInfo(executable).absolutePath());
			updater.setProcessEnvironment(environment);
			updater.setProgram(helper);
			updater.setArguments(
				{QStringLiteral("-NoLogo"),
				 QStringLiteral("-NoProfile"),
				 QStringLiteral("-NonInteractive"),
				 QStringLiteral("-WindowStyle"),
				 QStringLiteral("Hidden"),
				 QStringLiteral("-Command"),
				 QStringLiteral(
					 "Start-Sleep -Milliseconds 1800; "
					 "try { "
					 "$p = Start-Process -FilePath "
					 "$env:CLIPXTUDIO_UPDATE_INSTALLER "
					 "-ArgumentList "
					 "'/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/CLOSEAPPLICATIONS' "
					 "-Verb RunAs -Wait -PassThru; "
					 "} finally { "
					 "Start-Process -FilePath "
					 "$env:CLIPXTUDIO_OBS_EXECUTABLE "
					 "-WorkingDirectory "
					 "$env:CLIPXTUDIO_OBS_WORKDIR "
					 "}")});
			const bool started = updater.startDetached();
			blog(started ? LOG_INFO : LOG_ERROR,
			     "[ClipXtudio] Verified update installer helper %s",
			     started ? "started" : "could not start");
			QObject::disconnect(*connection);
		},
		Qt::SingleShotConnection);
	if (!*connection) {
		if (error != nullptr)
			*error = "OBS could not register the update installation request.";
		return false;
	}

	blog(LOG_INFO,
	     "[ClipXtudio] Closing OBS to install a verified ClipXtudio update");
	QTimer::singleShot(0, mainWindow, [mainWindow] { mainWindow->close(); });
	QTimer::singleShot(1500, mainWindow, [mainWindow, connection] {
		if (mainWindow->isVisible()) {
			QObject::disconnect(*connection);
			blog(LOG_INFO,
			     "[ClipXtudio] Update canceled because OBS remained open");
		}
	});
	return true;
#endif
}

class ObsVerticalPreview final : public QWidget {
public:
	explicit ObsVerticalPreview(QWidget *parent) : QWidget(parent)
	{
		setAttribute(Qt::WA_PaintOnScreen);
		setAttribute(Qt::WA_StaticContents);
		setAttribute(Qt::WA_NoSystemBackground);
		setAttribute(Qt::WA_OpaquePaintEvent);
		setAttribute(Qt::WA_DontCreateNativeAncestors);
		setAttribute(Qt::WA_NativeWindow);
		setMinimumSize(180, 320);
		setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
		setFocusPolicy(Qt::ClickFocus);
		updateInteractionState();
	}

	~ObsVerticalPreview() override
	{
		destroying_ = true;
		if (display_ != nullptr) {
			obs_display_remove_draw_callback(display_, &ObsVerticalPreview::draw, this);
			obs_display_destroy(display_);
			display_ = nullptr;
		}
		std::lock_guard<std::mutex> lock(sourceMutex_);
		if (source_ != nullptr) {
			obs_source_release(source_);
			source_ = nullptr;
		}
	}

	void setSource(const std::string &name)
	{
		obs_source_t *next = name.empty() ? nullptr : obs_get_source_by_name(name.c_str());
		std::lock_guard<std::mutex> lock(sourceMutex_);
		if (source_ != nullptr)
			obs_source_release(source_);
		source_ = next;
	}

	void setCanvasSettings(const VerticalCanvasSettings &settings)
	{
		{
			std::lock_guard<std::mutex> lock(sourceMutex_);
			canvasWidth_ = std::max(1, settings.width);
			canvasHeight_ = std::max(1, settings.height);
			zoomPercent_ = settings.zoomPercent;
			panXPercent_ = settings.panXPercent;
			panYPercent_ = settings.panYPercent;
		}
	}

	void setFramingChanged(ui::VerticalObsBridge::FramingChanged callback)
	{
		framingChanged_ = std::move(callback);
	}

	[[nodiscard]] QSize sizeHint() const override { return {225, 400}; }
	[[nodiscard]] bool hasHeightForWidth() const override { return true; }
	[[nodiscard]] int heightForWidth(int width) const override { return width * 16 / 9; }

protected:
	QPaintEngine *paintEngine() const override { return nullptr; }

	void paintEvent(QPaintEvent *event) override
	{
		createDisplay();
		QWidget::paintEvent(event);
	}

	void showEvent(QShowEvent *event) override
	{
		QWidget::showEvent(event);
		createDisplay();
		resizeDisplay();
	}

	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		createDisplay();
		resizeDisplay();
	}

	void mousePressEvent(QMouseEvent *event) override
	{
		if (event->button() != Qt::LeftButton) {
			QWidget::mousePressEvent(event);
			return;
		}
		interactionGate_.activate();
		setFocus(Qt::MouseFocusReason);
		updateInteractionState();
		dragOrigin_ = event->position();
		{
			std::lock_guard<std::mutex> lock(sourceMutex_);
			dragPanX_ = panXPercent_;
			dragPanY_ = panYPercent_;
		}
		dragging_ = true;
		setCursor(Qt::ClosedHandCursor);
		event->accept();
	}

	void mouseMoveEvent(QMouseEvent *event) override
	{
		if (!dragging_ || !(event->buttons() & Qt::LeftButton)) {
			QWidget::mouseMoveEvent(event);
			return;
		}
		const auto delta = event->position() - dragOrigin_;
		const auto nextX = std::clamp(
			dragPanX_ - static_cast<int>(std::lround(delta.x() * 200.0 / std::max(1, width()))),
			-100, 100);
		const auto nextY = std::clamp(
			dragPanY_ - static_cast<int>(std::lround(delta.y() * 200.0 / std::max(1, height()))),
			-100, 100);
		int zoom = 100;
		{
			std::lock_guard<std::mutex> lock(sourceMutex_);
			panXPercent_ = nextX;
			panYPercent_ = nextY;
			zoom = zoomPercent_;
		}
		if (framingChanged_)
			framingChanged_(zoom, nextX, nextY);
		event->accept();
	}

	void mouseReleaseEvent(QMouseEvent *event) override
	{
		if (event->button() == Qt::LeftButton) {
			dragging_ = false;
			if (!underMouse())
				interactionGate_.deactivate();
			updateInteractionState();
			event->accept();
			return;
		}
		QWidget::mouseReleaseEvent(event);
	}

	void leaveEvent(QEvent *event) override
	{
		if (!dragging_) {
			interactionGate_.deactivate();
			updateInteractionState();
		}
		QWidget::leaveEvent(event);
	}

	void wheelEvent(QWheelEvent *event) override
	{
		if (!interactionGate_.acceptsWheel()) {
			event->ignore();
			return;
		}
		const int steps = event->angleDelta().y() / 120;
		if (steps == 0) {
			event->ignore();
			return;
		}
		int zoom = 100;
		int panX = 0;
		int panY = 0;
		{
			std::lock_guard<std::mutex> lock(sourceMutex_);
			zoomPercent_ = std::clamp(zoomPercent_ + steps * 5, 100, 300);
			zoom = zoomPercent_;
			panX = panXPercent_;
			panY = panYPercent_;
		}
		if (framingChanged_)
			framingChanged_(zoom, panX, panY);
		event->accept();
	}

private:
	void updateInteractionState()
	{
		const bool active = interactionGate_.isActive();
		setProperty("previewInteractionActive", active);
		setCursor(active ? Qt::OpenHandCursor : Qt::ArrowCursor);
		setToolTip(active
				   ? QStringLiteral(
					     "Edición activa: arrastra para mover y usa la rueda para acercar o alejar. Al salir de la vista, el zoom se desactiva.")
				   : QStringLiteral(
					     "Haz clic para activar la edición. Mientras está inactiva, la rueda desplaza la pantalla sin cambiar el zoom."));
	}

	void createDisplay()
	{
		if (display_ != nullptr || destroying_ || !isVisible() || windowHandle() == nullptr ||
		    !windowHandle()->isExposed())
			return;
		const auto ratio = devicePixelRatioF();
		gs_init_data info{};
		info.window.hwnd = reinterpret_cast<void *>(winId());
		info.cx = std::max(1, static_cast<int>(std::lround(width() * ratio)));
		info.cy = std::max(1, static_cast<int>(std::lround(height() * ratio)));
		info.format = GS_BGRA;
		info.zsformat = GS_ZS_NONE;
		display_ = obs_display_create(&info, 0xFF0B0F17);
		if (display_ != nullptr)
			obs_display_add_draw_callback(display_, &ObsVerticalPreview::draw, this);
	}

	void resizeDisplay()
	{
		if (display_ == nullptr)
			return;
		const auto ratio = devicePixelRatioF();
		obs_display_resize(display_, std::max(1, static_cast<int>(std::lround(width() * ratio))),
				   std::max(1, static_cast<int>(std::lround(height() * ratio))));
	}

	static void draw(void *data, uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
			return;
		auto *preview = static_cast<ObsVerticalPreview *>(data);
		obs_source_t *source = nullptr;
		int canvasWidth = 1080;
		int canvasHeight = 1920;
		int zoomPercent = 100;
		int panXPercent = 0;
		int panYPercent = 0;
		{
			std::lock_guard<std::mutex> lock(preview->sourceMutex_);
			source = preview->source_;
			canvasWidth = preview->canvasWidth_;
			canvasHeight = preview->canvasHeight_;
			zoomPercent = preview->zoomPercent_;
			panXPercent = preview->panXPercent_;
			panYPercent = preview->panYPercent_;
			if (source != nullptr)
				source = obs_source_get_ref(source);
		}
		if (source == nullptr)
			return;

		uint32_t sourceWidth = obs_source_get_width(source);
		uint32_t sourceHeight = obs_source_get_height(source);
		if (sourceWidth == 0 || sourceHeight == 0) {
			obs_video_info video{};
			if (obs_get_video_info(&video)) {
				sourceWidth = video.base_width;
				sourceHeight = video.base_height;
			}
		}
		if (sourceWidth == 0 || sourceHeight == 0) {
			obs_source_release(source);
			return;
		}

		const float targetAspect = static_cast<float>(canvasWidth) /
					   static_cast<float>(canvasHeight);
		const float sourceAspect = static_cast<float>(sourceWidth) /
					   static_cast<float>(sourceHeight);
		float left = 0.0F;
		float top = 0.0F;
		float viewWidth = static_cast<float>(sourceWidth);
		float viewHeight = static_cast<float>(sourceHeight);
		if (sourceAspect > targetAspect) {
			viewWidth = viewHeight * targetAspect;
		} else {
			viewHeight = viewWidth / targetAspect;
		}
		const float zoom = std::clamp(static_cast<float>(zoomPercent) / 100.0F,
					      1.0F, 3.0F);
		viewWidth /= zoom;
		viewHeight /= zoom;
		left = (static_cast<float>(sourceWidth) - viewWidth) *
		       (static_cast<float>(std::clamp(panXPercent, -100, 100)) + 100.0F) /
		       200.0F;
		top = (static_cast<float>(sourceHeight) - viewHeight) *
		      (static_cast<float>(std::clamp(panYPercent, -100, 100)) + 100.0F) /
		      200.0F;

		const float displayAspect = static_cast<float>(width) / static_cast<float>(height);
		uint32_t viewportWidth = width;
		uint32_t viewportHeight = height;
		int viewportX = 0;
		int viewportY = 0;
		if (displayAspect > targetAspect) {
			viewportWidth = static_cast<uint32_t>(
				std::lround(static_cast<float>(height) * targetAspect));
			viewportX = static_cast<int>((width - viewportWidth) / 2);
		} else {
			viewportHeight = static_cast<uint32_t>(
				std::lround(static_cast<float>(width) / targetAspect));
			viewportY = static_cast<int>((height - viewportHeight) / 2);
		}

		gs_viewport_push();
		gs_projection_push();
		gs_ortho(left, left + viewWidth, top, top + viewHeight, -100.0F, 100.0F);
		gs_set_viewport(viewportX, viewportY, static_cast<int>(viewportWidth),
				static_cast<int>(viewportHeight));
		obs_source_video_render(source);
		gs_load_vertexbuffer(nullptr);
		gs_projection_pop();
		gs_viewport_pop();
		obs_source_release(source);
	}

	std::mutex sourceMutex_;
	obs_source_t *source_{nullptr};
	obs_display_t *display_{nullptr};
	int canvasWidth_{1080};
	int canvasHeight_{1920};
	int zoomPercent_{100};
	int panXPercent_{0};
	int panYPercent_{0};
	ui::VerticalObsBridge::FramingChanged framingChanged_;
	ui::PreviewInteractionGate interactionGate_;
	QPointF dragOrigin_;
	int dragPanX_{0};
	int dragPanY_{0};
	bool dragging_{false};
	bool destroying_{false};
};

std::vector<std::string> sceneNames()
{
	std::vector<std::string> result;
	obs_frontend_source_list scenes{};
	obs_frontend_get_scenes(&scenes);
	for (std::size_t index = 0; index < scenes.sources.num; ++index) {
		const char *name = obs_source_get_name(scenes.sources.array[index]);
		if (name != nullptr && *name != '\0')
			result.emplace_back(name);
	}
	obs_frontend_source_list_free(&scenes);
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::string activeSceneName()
{
	obs_source_t *scene = obs_frontend_get_current_scene();
	if (scene == nullptr)
		return {};
	const char *name = obs_source_get_name(scene);
	const std::string result = name == nullptr ? std::string{} : std::string(name);
	obs_source_release(scene);
	return result;
}

bool collectSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *context)
{
	auto &result = *static_cast<std::vector<std::string> *>(context);
	obs_source_t *source = obs_sceneitem_get_source(item);
	const char *name = source == nullptr ? nullptr : obs_source_get_name(source);
	if (name != nullptr && *name != '\0')
		result.emplace_back(name);
	return true;
}

std::vector<std::string> sourceNames(const std::string &sceneName)
{
	std::vector<std::string> result;
	obs_source_t *source = obs_get_source_by_name(sceneName.c_str());
	if (source == nullptr)
		return result;
	obs_scene_t *scene = obs_scene_from_source(source);
	if (scene != nullptr)
		obs_scene_enum_items(scene, collectSceneItem, &result);
	obs_source_release(source);
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

struct CloneVerticalSceneContext {
	obs_scene_t *target{nullptr};
	const VerticalCanvasSettings *settings{nullptr};
	int added{0};
	obs_sceneitem_t *priorityOverlay{nullptr};
};

bool cloneVerticalSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *context)
{
	auto *state = static_cast<CloneVerticalSceneContext *>(context);
	if (state == nullptr || state->target == nullptr || state->settings == nullptr)
		return true;
	obs_source_t *source = obs_sceneitem_get_source(item);
	if (source == nullptr)
		return true;
	obs_sceneitem_t *copy = obs_scene_add(state->target, source);
	if (copy != nullptr) {
		applyVerticalCompositionItem(copy, *state->settings);
		if (isMontillaKickOverlay(source))
			state->priorityOverlay = copy;
		++state->added;
	}
	return true;
}

bool createVerticalScene(const std::string &sceneName, const std::string &sourceName,
			 const VerticalCanvasSettings &settings, std::string *error)
{
	constexpr const char *name = "ClipXtudio Vertical";
	obs_source_t *existing = obs_get_source_by_name(name);
	if (existing != nullptr) {
		obs_scene_t *scene = obs_scene_from_source(existing);
		if (scene == nullptr) {
			if (error != nullptr)
				*error = "ClipXtudio Vertical exists but is not an OBS scene";
			obs_source_release(existing);
			return false;
		}
		ApplyVerticalSceneContext context{&settings};
		obs_scene_enum_items(scene, applyVerticalSceneItem, &context);
		if (context.priorityOverlay != nullptr)
			obs_sceneitem_set_order(context.priorityOverlay, OBS_ORDER_MOVE_TOP);
		obs_frontend_set_current_scene(existing);
		obs_source_release(existing);
		return true;
	}
	const std::string selectedName = sourceName.empty() ? sceneName : sourceName;
	if (selectedName.empty()) {
		if (error != nullptr)
			*error = "select an OBS scene or source first";
		return false;
	}
	obs_source_t *selected = obs_get_source_by_name(selectedName.c_str());
	if (selected == nullptr) {
		if (error != nullptr)
			*error = "the selected OBS source is no longer available";
		return false;
	}
	obs_scene_t *created = obs_scene_create(name);
	if (created == nullptr) {
		obs_source_release(selected);
		if (error != nullptr)
			*error = "OBS could not create the ClipXtudio Vertical scene";
		return false;
	}
	CloneVerticalSceneContext context{created, &settings, 0, nullptr};
	obs_scene_t *selectedScene = sourceName.empty() ? obs_scene_from_source(selected) : nullptr;
	if (selectedScene != nullptr)
		obs_scene_enum_items(selectedScene, cloneVerticalSceneItem, &context);
	if (context.added == 0) {
		obs_sceneitem_t *item = obs_scene_add(created, selected);
		if (item != nullptr) {
			applyVerticalCompositionItem(item, settings);
			if (isMontillaKickOverlay(selected))
				context.priorityOverlay = item;
			++context.added;
		}
	}
	if (context.priorityOverlay != nullptr)
		obs_sceneitem_set_order(context.priorityOverlay, OBS_ORDER_MOVE_TOP);
	obs_source_release(selected);
	obs_source_t *createdSource = obs_scene_get_source(created);
	if (createdSource != nullptr)
		obs_frontend_set_current_scene(createdSource);
	obs_scene_release(created);
	return context.added > 0;
}

} // namespace

ui::VerticalObsBridge makeObsVerticalBridge()
{
	ui::VerticalObsBridge bridge;
	bridge.scenes = sceneNames;
	bridge.activeScene = activeSceneName;
	bridge.sourcesForScene = sourceNames;
	bridge.createPreview = [](QWidget *parent) { return new ObsVerticalPreview(parent); };
	bridge.updatePreview = [](QWidget *widget, const std::string &source,
				  const VerticalCanvasSettings &settings) {
		auto *preview = dynamic_cast<ObsVerticalPreview *>(widget);
		if (preview == nullptr)
			return;
		preview->setSource(source);
		preview->setCanvasSettings(settings);
	};
	bridge.bindPreviewInteraction =
		[](QWidget *widget, ui::VerticalObsBridge::FramingChanged callback) {
			auto *preview = dynamic_cast<ObsVerticalPreview *>(widget);
			if (preview != nullptr)
				preview->setFramingChanged(std::move(callback));
		};
	bridge.createVerticalScene = createVerticalScene;
	bridge.replayEncoders = replayEncoderOptions;
	bridge.replayProfile = replayProfileSettings;
	bridge.applyReplayProfile = applyReplayProfile;
	bridge.restartObs = restartObs;
	bridge.installUpdate = installUpdate;
	return bridge;
}

} // namespace clipcoach::plugin
