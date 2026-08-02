#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/core/hotkey-manager.hpp>
#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/trigger-engine.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>
#include <clipcoach/core/voice-trigger-controller.hpp>
#include <clipcoach/licensing/license-manager.hpp>
#include <clipcoach/licensing/machine-fingerprint.hpp>
#include <clipcoach/network/qt-license-api.hpp>
#include <clipcoach/network/qt-ai-api.hpp>
#include <clipcoach/network/remote-clipper-client.hpp>
#include <clipcoach/network/remote-command-poller.hpp>
#include <clipcoach/remote/remote-command-executor.hpp>
#include <clipcoach/remote/remote-result-outbox.hpp>
#include <clipcoach/plugin/remote-capture-coordinator.hpp>
#include <clipcoach/plugin/obs-hotkey-adapter.hpp>
#include <clipcoach/plugin/obs-replay-manager.hpp>
#include <clipcoach/plugin/obs-vertical-preview.hpp>
#include <clipcoach/plugin/clip-caption-transcriber.hpp>
#ifdef _WIN32
#include <clipcoach/plugin/windows-voice-trigger-controller.hpp>
#endif
#include <clipcoach/security/rs256-token-verifier.hpp>
#include <clipcoach/security/secure-storage.hpp>
#include <clipcoach/export/ffmpeg-export-backend.hpp>
#include <clipcoach/storage/clip-library-service.hpp>
#include <clipcoach/core/session-metadata.hpp>
#include <clipcoach/ui/main-dock.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/bmem.h>
#include <util/config-file.h>

#include <exception>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <QDockWidget>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QSettings>
#include <QSslSocket>
#include <QTabBar>
#include <QThread>
#include <QTimeZone>
#include <QTimer>
#include <QUrl>
#include <random>
#include <QString>
#include <QStringList>
#include <sstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("clipxtudio", "en-US")

namespace {

constexpr const char *kLogPrefix = "[ClipXtudio]";
constexpr const char *kDockId = "com.clipcoach.studio.main";

bool dockRegistered = false;
bool frontendShuttingDown = false;
bool lifecycleCallbackRegistered = false;
std::unique_ptr<clipcoach::SettingsManager> settingsManager;
std::unique_ptr<clipcoach::plugin::ObsReplayManager> replayManager;
std::unique_ptr<clipcoach::ClipManager> clipManager;
std::unique_ptr<clipcoach::storage::ClipLibraryService> libraryService;
std::unique_ptr<clipcoach::plugin::ObsHotkeyAdapter> hotkeyAdapter;
std::unique_ptr<clipcoach::HotkeyManager> hotkeyManager;
std::unique_ptr<clipcoach::VerticalCanvasManager> verticalCanvasManager;
std::unique_ptr<clipcoach::ExportManager> exportManager;
std::unique_ptr<clipcoach::TriggerEngine> triggerEngine;
std::unique_ptr<clipcoach::VoiceTriggerController> voiceTriggerController;
std::unique_ptr<clipcoach::security::SecureStorage> licenseSecureStorage;
std::unique_ptr<clipcoach::licensing::FileMachineFingerprintProvider> licenseFingerprintProvider;
std::unique_ptr<clipcoach::network::QtLicenseApi> licenseApi;
std::unique_ptr<clipcoach::security::Rs256TokenVerifier> licenseTokenVerifier;
std::unique_ptr<clipcoach::licensing::LicenseManager> licenseManager;
std::unique_ptr<clipcoach::FeatureGateService> featureGateService;
std::unique_ptr<clipcoach::network::QtAiApi> aiApi;
std::unique_ptr<clipcoach::AiAssistantService> aiAssistant;
std::unique_ptr<clipcoach::plugin::ClipCaptionTranscriber> captionTranscriber;
std::unique_ptr<QNetworkAccessManager> clipTelemetryNetwork;
std::unique_ptr<QSettings> uiLocaleSettings;
std::unique_ptr<QTimer> licenseRefreshTimer;
std::unique_ptr<clipcoach::network::RemoteClipperClient> remoteClipperClient;
std::unique_ptr<clipcoach::network::RemoteCommandPoller> remoteCommandPoller;
std::unique_ptr<clipcoach::remote::RemoteCommandExecutor> remoteCommandExecutor;
std::unique_ptr<clipcoach::plugin::RemoteCaptureCoordinator> remoteCaptureCoordinator;
std::unique_ptr<QSettings> remoteCommandState;
std::unique_ptr<clipcoach::remote::RemoteResultOutbox> remoteResultOutbox;
std::set<std::string> remoteResultsInFlight;
std::vector<QPointer<QProcess>> thumbnailProcesses;
clipcoach::licensing::LicenseManager::ObserverId licenseGateObserverId = 0;
QPointer<clipcoach::ui::MainDock> mainDockContent;
std::string currentSessionId;
std::set<std::string> aiAnalysisInFlight;
bool replayCapacityRestartPending = false;
bool remoteReplayCapacityPrepared = false;

std::string currentSceneName();
std::vector<std::string> availableSceneNames();
std::string readFile(const std::filesystem::path &path);
void ensureObsReplayCapacity(int requiredSeconds);
void shutdownRemoteClipper() noexcept;

void restartReplayBufferWhenReady(int attempts = 0)
{
	if (frontendShuttingDown)
		return;
	if (obs_frontend_replay_buffer_active()) {
		if (attempts >= 40) {
			replayCapacityRestartPending = false;
			blog(LOG_ERROR, "%s Replay Buffer did not stop after its duration was updated",
			     kLogPrefix);
			return;
		}
		QTimer::singleShot(250, [attempts] {
			restartReplayBufferWhenReady(attempts + 1);
		});
		return;
	}
	blog(LOG_INFO, "%s Restarting Replay Buffer with the updated clip window",
	     kLogPrefix);
	replayCapacityRestartPending = false;
	obs_frontend_replay_buffer_start();
}

void ensureObsReplayCapacity(int requiredSeconds)
{
	const auto target = std::clamp(
		requiredSeconds,
		clipcoach::settings_constraints::kMinClipDurationSeconds,
		clipcoach::settings_constraints::kMaxClipDurationSeconds);
	config_t *config = obs_frontend_get_profile_config();
	if (config == nullptr) {
		blog(LOG_WARNING, "%s OBS profile is unavailable; Replay Buffer duration was not updated",
		     kLogPrefix);
		return;
	}
	const char *rawMode = config_get_string(config, "Output", "Mode");
	const bool advanced = rawMode != nullptr &&
			      std::string_view(rawMode) == "Advanced";
	const char *section = advanced ? "AdvOut" : "SimpleOutput";
	const auto current = static_cast<int>(
		config_get_uint(config, section, "RecRBTime"));
	const auto currentSizeLimit = config_get_uint(
		config, section, "RecRBSize");
	if (current >= target && currentSizeLimit == 0)
		return;

	config_set_uint(config, section, "RecRBTime",
			static_cast<std::uint64_t>(std::max(current, target)));
	// OBS treats RecRBSize as a second, competing stop condition for
	// quality-based encoders. A low MB cap can turn an 89-second request into
	// a much shorter file, so the configured time is the single source of truth.
	config_set_uint(config, section, "RecRBSize", 0);
	if (config_save_safe(config, "tmp", "bak") != CONFIG_SUCCESS) {
		blog(LOG_ERROR, "%s OBS could not save Replay Buffer duration %d s",
		     kLogPrefix, target);
		return;
	}
	blog(LOG_INFO,
	     "%s Replay Buffer capacity set to %d s with no competing size cap",
	     kLogPrefix, std::max(current, target));
	if (!obs_frontend_replay_buffer_active())
		return;
	if (clipManager != nullptr && clipManager->capturePending()) {
		blog(LOG_WARNING,
		     "%s Replay Buffer duration saved but restart deferred because a capture is pending",
		     kLogPrefix);
		return;
	}
	if (replayCapacityRestartPending)
		return;
	replayCapacityRestartPending = true;
	obs_frontend_replay_buffer_stop();
	QTimer::singleShot(250, [] { restartReplayBufferWhenReady(); });
}

bool configureQtNetworkPlugins()
{
#ifdef _WIN32
	char *rawPluginPath = obs_module_file("qt-plugins");
	if (rawPluginPath != nullptr) {
		const auto pluginPath = QString::fromUtf8(rawPluginPath);
		bfree(rawPluginPath);
		if (!pluginPath.isEmpty() && !QCoreApplication::libraryPaths().contains(pluginPath))
			QCoreApplication::addLibraryPath(pluginPath);
		blog(LOG_INFO, "%s Qt plugin path: %s", kLogPrefix, pluginPath.toUtf8().constData());
	}
#endif
	const bool available = QSslSocket::supportsSsl();
	if (available) {
		blog(LOG_INFO, "%s Qt TLS backend ready: %s", kLogPrefix,
		     QSslSocket::activeBackend().toUtf8().constData());
	} else {
		blog(LOG_ERROR, "%s Qt TLS backend unavailable; licensing, updates and cloud services cannot use HTTPS",
		     kLogPrefix);
	}
	return available;
}

void migrateLegacyModuleConfig()
{
	char *rawCurrentPath = obs_module_config_path("settings.json");
	if (rawCurrentPath == nullptr)
		return;

	const std::filesystem::path currentDirectory = std::filesystem::path(rawCurrentPath).parent_path();
	bfree(rawCurrentPath);
	const auto legacyDirectory = currentDirectory.parent_path() / "clipcoach-studio";
	const auto migrationMarker = currentDirectory / ".clipxtudio-config-migrated";

	std::error_code error;
	if (std::filesystem::exists(migrationMarker, error) || !std::filesystem::is_directory(legacyDirectory, error))
		return;

	std::filesystem::create_directories(currentDirectory, error);
	if (error) {
		blog(LOG_WARNING, "%s Could not prepare the ClipXtudio configuration directory: %s", kLogPrefix,
		     error.message().c_str());
		return;
	}

	const auto backupDirectory = currentDirectory / "pre-0.5.11-migration";
	std::filesystem::create_directories(backupDirectory, error);
	if (error) {
		blog(LOG_WARNING, "%s Could not create the configuration migration backup: %s", kLogPrefix,
		     error.message().c_str());
		return;
	}

	const std::vector<std::string> files = {
		"install-id",
		"settings.json",
		"clipcoach.db",
	};
	for (const auto &name : files) {
		const auto legacyFile = legacyDirectory / name;
		if (!std::filesystem::is_regular_file(legacyFile, error))
			continue;

		const auto currentFile = currentDirectory / name;
		if (std::filesystem::is_regular_file(currentFile, error)) {
			std::filesystem::copy_file(currentFile, backupDirectory / name,
						   std::filesystem::copy_options::overwrite_existing, error);
			if (error) {
				blog(LOG_WARNING, "%s Could not back up %s before migration: %s", kLogPrefix,
				     name.c_str(), error.message().c_str());
				return;
			}
		}

		std::filesystem::copy_file(legacyFile, currentFile, std::filesystem::copy_options::overwrite_existing,
					   error);
		if (error) {
			blog(LOG_WARNING, "%s Could not migrate %s: %s", kLogPrefix, name.c_str(),
			     error.message().c_str());
			return;
		}
	}

	std::ofstream marker(migrationMarker, std::ios::binary | std::ios::trunc);
	if (!marker) {
		blog(LOG_WARNING, "%s Configuration migrated but the migration marker could not be saved", kLogPrefix);
		return;
	}
	marker << "clipcoach-studio -> clipxtudio\n";
	marker.close();
	blog(LOG_INFO, "%s Migrated legacy settings, device identity and clip library", kLogPrefix);
}

void configureUiLocale()
{
	uiLocaleSettings.reset();
	if (settingsManager == nullptr || settingsManager->settings().language == "system")
		return;

	const auto &language = settingsManager->settings().language;
	if (language != "en-US" && language != "es-ES")
		return;
	const auto relativePath = std::string("locale/") + language + ".ini";
	char *rawPath = obs_module_file(relativePath.c_str());
	if (rawPath == nullptr) {
		blog(LOG_WARNING, "%s Requested UI locale is unavailable: %s", kLogPrefix, language.c_str());
		return;
	}
	uiLocaleSettings = std::make_unique<QSettings>(QString::fromUtf8(rawPath), QSettings::IniFormat);
	bfree(rawPath);
	blog(LOG_INFO, "%s UI language override loaded: %s", kLogPrefix, language.c_str());
}

QString uiText(const char *key)
{
	const auto fallback = QString::fromUtf8(obs_module_text(key));
	return uiLocaleSettings != nullptr ? uiLocaleSettings->value(QString::fromUtf8(key), fallback).toString()
					   : fallback;
}

clipcoach::TriggerType persistedTriggerType(clipcoach::SmartTriggerType type) noexcept
{
	switch (type) {
	case clipcoach::SmartTriggerType::Voice:
	case clipcoach::SmartTriggerType::Keyword:
		return clipcoach::TriggerType::Voice;
	case clipcoach::SmartTriggerType::AudioSpike:
		return clipcoach::TriggerType::AudioSpike;
	case clipcoach::SmartTriggerType::ChatPulse:
		return clipcoach::TriggerType::Chat;
	case clipcoach::SmartTriggerType::Scene:
		return clipcoach::TriggerType::Scene;
	case clipcoach::SmartTriggerType::FutureAiHook:
		return clipcoach::TriggerType::Ai;
	case clipcoach::SmartTriggerType::Manual:
		return clipcoach::TriggerType::Manual;
	}
	return clipcoach::TriggerType::Manual;
}

std::string triggerLabel(const clipcoach::TriggerEvent &event)
{
	if (!event.keyword.empty())
		return event.keyword;
	if (!event.scene.empty())
		return event.scene;
	return clipcoach::triggerTypeName(event.primaryType);
}

std::filesystem::path bundledFfmpegPath()
{
#ifdef _WIN32
	constexpr const char *relativePath = "tools/ffmpeg/ffmpeg.exe";
#else
	constexpr const char *relativePath = "tools/ffmpeg/ffmpeg";
#endif
	char *path = obs_module_file(relativePath);
	if (path == nullptr)
		return {};
	std::filesystem::path result = std::filesystem::u8path(std::string(path));
	bfree(path);
	return result;
}

std::filesystem::path bundledWhisperModelPath()
{
	char *path = obs_module_file("models/ggml-tiny-q5_1.bin");
	if (path == nullptr)
		return {};
	std::filesystem::path result = std::filesystem::u8path(std::string(path));
	bfree(path);
	return result;
}

std::string safeThumbnailStem(std::string value)
{
	for (auto &character : value) {
		const auto valid = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
				   (character >= '0' && character <= '9') || character == '-' || character == '_';
		if (!valid)
			character = '_';
	}
	return value.empty() ? "clip" : value;
}

void removeThumbnailProcess(QProcess *process)
{
	thumbnailProcesses.erase(std::remove_if(thumbnailProcesses.begin(), thumbnailProcesses.end(),
						[process](const auto &candidate) {
							return candidate.isNull() || candidate.data() == process;
						}),
				 thumbnailProcesses.end());
}

void queueThumbnailGeneration(const clipcoach::ClipMetadata &clip)
{
	if (libraryService == nullptr || settingsManager == nullptr || clip.filePath.empty() ||
	    !std::filesystem::is_regular_file(clip.filePath))
		return;
	if (!clip.thumbnailPath.empty() && std::filesystem::is_regular_file(clip.thumbnailPath))
		return;
	const auto ffmpeg = bundledFfmpegPath();
	if (!std::filesystem::is_regular_file(ffmpeg))
		return;
	auto directory = settingsManager->settings().thumbnailDirectory;
	if (directory.empty())
		directory = clip.filePath.parent_path() / "ClipXtudio Thumbnails";
	std::error_code filesystemError;
	std::filesystem::create_directories(directory, filesystemError);
	if (filesystemError) {
		blog(LOG_WARNING, "%s Thumbnail directory is unavailable: %s", kLogPrefix,
		     filesystemError.message().c_str());
		return;
	}
	const auto output = directory / (safeThumbnailStem(clip.id) + std::string(".jpg"));
	auto *process = new QProcess(QCoreApplication::instance());
	process->setObjectName(QStringLiteral("clipThumbnailGenerator"));
	thumbnailProcesses.push_back(QPointer<QProcess>(process));
	const auto clipId = clip.id;
	QObject::connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), process,
			 [process, clipId, output](int exitCode, QProcess::ExitStatus status) {
				 removeThumbnailProcess(process);
				 const bool generated = status == QProcess::NormalExit && exitCode == 0 &&
							std::filesystem::is_regular_file(output);
				 if (!generated) {
					 blog(LOG_WARNING, "%s Thumbnail generation failed for clip %s: %s", kLogPrefix,
					      clipId.c_str(), process->readAllStandardError().constData());
					 process->deleteLater();
					 return;
				 }
				 if (libraryService != nullptr) {
					 libraryService->updateThumbnail(
						 clipId, output, [](clipcoach::storage::StorageStatus result) {
							 if (!result.success) {
								 blog(LOG_WARNING,
								      "%s Thumbnail metadata could not be saved: %s",
								      kLogPrefix, result.error.c_str());
								 return;
							 }
							 QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
							 if (!dock.isNull()) {
								 QMetaObject::invokeMethod(
									 dock,
									 [dock] {
										 if (!dock.isNull())
											 dock->refreshClipLibrary();
									 },
									 Qt::QueuedConnection);
							 }
						 });
				 }
				 blog(LOG_INFO, "%s Thumbnail generated for clip %s", kLogPrefix, clipId.c_str());
				 process->deleteLater();
			 });
	QObject::connect(process, &QProcess::errorOccurred, process, [process, clipId](QProcess::ProcessError error) {
		if (error != QProcess::FailedToStart)
			return;
		removeThumbnailProcess(process);
		blog(LOG_WARNING, "%s Thumbnail generator could not start for clip %s", kLogPrefix, clipId.c_str());
		process->deleteLater();
	});
	process->start(QString::fromStdString(ffmpeg.u8string()),
		       {QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
			QStringLiteral("-ss"), QStringLiteral("0.2"), QStringLiteral("-i"),
			QString::fromStdString(clip.filePath.u8string()), QStringLiteral("-frames:v"),
			QStringLiteral("1"), QStringLiteral("-vf"), QStringLiteral("scale=320:-2"),
			QStringLiteral("-q:v"), QStringLiteral("3"), QStringLiteral("-y"),
			QString::fromStdString(output.u8string())});
}

void backfillRecentThumbnails()
{
	if (libraryService == nullptr)
		return;
	libraryService->listRecentAsync(
		100, [](clipcoach::storage::RepositoryResult<std::vector<clipcoach::ClipMetadata>> result) mutable {
			if (!result.success || QCoreApplication::instance() == nullptr)
				return;
			QMetaObject::invokeMethod(
				QCoreApplication::instance(),
				[clips = std::move(result.value)]() mutable {
					int pendingIndex = 0;
					for (const auto &clip : clips) {
						if ((!clip.thumbnailPath.empty() &&
						     std::filesystem::is_regular_file(clip.thumbnailPath)) ||
						    !std::filesystem::is_regular_file(clip.filePath))
							continue;
						const auto delayMs = pendingIndex++ * 750;
						QTimer::singleShot(delayMs, QCoreApplication::instance(),
								   [clip] { queueThumbnailGeneration(clip); });
					}
					if (pendingIndex > 0)
						blog(LOG_INFO, "%s Queued thumbnail backfill for %d clips", kLogPrefix,
						     pendingIndex);
				},
				Qt::QueuedConnection);
		});
}

void shutdownThumbnailGeneration() noexcept
{
	for (auto &guard : thumbnailProcesses) {
		auto *process = guard.data();
		if (process == nullptr)
			continue;
		QObject::disconnect(process, nullptr, nullptr, nullptr);
		if (process->state() != QProcess::NotRunning) {
			process->kill();
			process->waitForFinished(2000);
		}
		delete process;
	}
	thumbnailProcesses.clear();
}

clipcoach::AiLanguage aiLanguage(const std::string &value)
{
	return value == "es"   ? clipcoach::AiLanguage::Spanish
	       : value == "en" ? clipcoach::AiLanguage::English
			       : clipcoach::AiLanguage::Auto;
}

clipcoach::AiAssistantConfiguration aiConfiguration(const clipcoach::ClipMetadata &clip)
{
	clipcoach::AiAssistantConfiguration configuration;
	if (settingsManager == nullptr)
		return configuration;
	const auto &settings = settingsManager->settings();
	configuration.enabled = settings.aiAssistantEnabled;
	configuration.privacyConsent = settings.aiPrivacyConsent;
	configuration.language = aiLanguage(settings.aiLanguage);
	configuration.subtitleDirectory = settings.exportDirectory.empty()
						  ? clip.filePath.parent_path() / "ClipXtudio Subtitles"
						  : settings.exportDirectory / "subtitles";
	return configuration;
}

QStringList responseHashtags(const clipcoach::AiAssistantResponse &response)
{
	QStringList hashtags;
	for (const auto &value : response.hashtags) {
		auto hashtag = QString::fromStdString(value).trimmed();
		if (hashtag.isEmpty())
			continue;
		if (!hashtag.startsWith(QLatin1Char('#')))
			hashtag.prepend(QLatin1Char('#'));
		hashtags.push_back(std::move(hashtag));
	}
	return hashtags;
}

QString aiAuthorizationRequiredMessage()
{
	return uiText("Clips.Caption.AuthorizationRequired");
}

QString aiSynchronizationRequiredMessage()
{
	return uiText("Clips.Caption.SynchronizationRequired");
}

QString aiLicenseServiceUnavailableMessage()
{
	return uiText("Clips.Caption.LicenseServiceUnavailable");
}

QString aiInvalidLicenseMessage()
{
	return uiText("Clips.Caption.LicenseInvalid");
}

QString aiBackendUnavailableMessage()
{
	return uiText("Clips.Caption.BackendUnavailable");
}

QString aiProviderUnavailableMessage()
{
	return uiText("Clips.Caption.ProviderUnavailable");
}

QString aiInvalidResponseMessage()
{
	return uiText("Clips.Caption.InvalidResponse");
}

bool isAiAuthorizationFailure(const clipcoach::AiAssistantResult &result)
{
	return result.error == clipcoach::AiError::ProRequired &&
	       (result.code == "AI_AUTHORIZATION_REQUIRED" || result.code == "PRO_REQUIRED" ||
		result.code == "TOKEN_INVALID" || result.code == "TOKEN_EXPIRED" ||
		result.code == "LICENSE_TOKEN_INVALID" || result.code == "LICENSE_TOKEN_EXPIRED" ||
		result.code == "LICENSE_TOKEN_REVOKED" || result.code == "LICENSE_TOKEN_MISSING");
}

bool isLicenseSigningFailure(const clipcoach::AiAssistantResult &result)
{
	return result.code == "LICENSE_SIGNING_UNAVAILABLE" || result.code == "SERVER_MISCONFIGURED";
}

bool isInvalidLicenseCode(const QString &code)
{
	static const QStringList invalidCodes{
		QStringLiteral("LICENSE_KEY_INVALID"),
		QStringLiteral("LICENSE_KEY_REVOKED"),
		QStringLiteral("LICENSE_KEY_EXPIRED"),
		QStringLiteral("LICENSE_REVOKED"),
		QStringLiteral("LICENSE_NOT_FOUND"),
		QStringLiteral("LICENSE_TOKEN_INVALID"),
		QStringLiteral("LICENSE_TOKEN_EXPIRED"),
		QStringLiteral("LICENSE_TOKEN_REVOKED"),
		QStringLiteral("REFRESH_TOKEN_INVALID"),
		QStringLiteral("REFRESH_TOKEN_REVOKED"),
		QStringLiteral("SUBSCRIPTION_INACTIVE"),
		QStringLiteral("TOKEN_DEVICE_MISMATCH"),
		QStringLiteral("DEVICE_MISMATCH"),
		QStringLiteral("DEVICE_BLOCKED"),
	};
	return invalidCodes.contains(code);
}

clipcoach::AiAssistantResult synchronizedLicenseRequiredResult()
{
	clipcoach::AiAssistantResult result;
	result.error = clipcoach::AiError::ProRequired;
	result.code = "AI_LICENSE_SYNCHRONIZATION_REQUIRED";
	result.message = aiSynchronizationRequiredMessage().toStdString();
	return result;
}

void analyzeClipWithAuthorization(clipcoach::ClipMetadata clip, std::string transcript,
				  clipcoach::AiAssistantConfiguration configuration,
				  clipcoach::AiAssistantService::Completion completion, bool allowRefresh = true)
{
	if (aiAssistant == nullptr || licenseManager == nullptr) {
		clipcoach::AiAssistantResult result;
		result.error = clipcoach::AiError::ProRequired;
		result.code = "AI_AUTHORIZATION_REQUIRED";
		result.message = aiAuthorizationRequiredMessage().toStdString();
		if (completion)
			completion(std::move(result));
		return;
	}

	// Offline grace keeps local Pro features available, but a cached/expired
	// bearer token must never be presented as current online authorization.
	// Refresh first so courtesy and perpetual licenses use the backend exactly
	// like paid licenses before calling an online AI endpoint.
	if (licenseManager->snapshot().state == clipcoach::licensing::LicenseState::ProGrace) {
		if (allowRefresh) {
			licenseManager->refresh([clip = std::move(clip), transcript = std::move(transcript),
						 configuration = std::move(configuration),
						 completion = std::move(completion)](
							const clipcoach::licensing::LicenseSnapshot &) mutable {
				analyzeClipWithAuthorization(std::move(clip), std::move(transcript),
							     std::move(configuration), std::move(completion), false);
			});
			return;
		}

		if (completion)
			completion(synchronizedLicenseRequiredResult());
		return;
	}

	if (licenseManager->authorizationToken().empty()) {
		if (allowRefresh) {
			licenseManager->refresh([clip = std::move(clip), transcript = std::move(transcript),
						 configuration = std::move(configuration),
						 completion = std::move(completion)](
							const clipcoach::licensing::LicenseSnapshot &) mutable {
				analyzeClipWithAuthorization(std::move(clip), std::move(transcript),
							     std::move(configuration), std::move(completion), false);
			});
			return;
		}

		clipcoach::AiAssistantResult result;
		result.error = clipcoach::AiError::ProRequired;
		result.code = "AI_AUTHORIZATION_REQUIRED";
		result.message = aiAuthorizationRequiredMessage().toStdString();
		if (completion)
			completion(std::move(result));
		return;
	}

	auto retryClip = clip;
	auto retryTranscript = transcript;
	auto retryConfiguration = configuration;
	aiAssistant->analyzeClip(
		clip, std::move(transcript), configuration,
		[retryClip = std::move(retryClip), retryTranscript = std::move(retryTranscript),
		 retryConfiguration = std::move(retryConfiguration), completion = std::move(completion),
		 allowRefresh](clipcoach::AiAssistantResult result) mutable {
			if (allowRefresh && (isAiAuthorizationFailure(result) || isLicenseSigningFailure(result)) &&
			    licenseManager != nullptr) {
				licenseManager->refresh([clip = std::move(retryClip),
							 transcript = std::move(retryTranscript),
							 configuration = std::move(retryConfiguration),
							 completion = std::move(completion)](
								const clipcoach::licensing::LicenseSnapshot &) mutable {
					analyzeClipWithAuthorization(std::move(clip), std::move(transcript),
								     std::move(configuration), std::move(completion),
								     false);
				});
				return;
			}
			if (isLicenseSigningFailure(result))
				result.message = aiLicenseServiceUnavailableMessage().toStdString();
			else if (result.code == "AI_LICENSE_SYNCHRONIZATION_REQUIRED")
				result.message = aiSynchronizationRequiredMessage().toStdString();
			else if (result.code == "AI_PROVIDER_UNAVAILABLE" ||
				 result.code == "AI_PROVIDER_MISCONFIGURED" ||
				 result.code == "AI_PROVIDER_REQUEST_FAILED" ||
				 result.code == "AI_PROVIDER_RESPONSE_INVALID")
				result.message = aiProviderUnavailableMessage().toStdString();
			else if (result.code == "AI_INVALID_RESPONSE")
				result.message = aiInvalidResponseMessage().toStdString();
			else if (result.code == "AI_BACKEND_UNAVAILABLE" ||
				 result.code == "AI_NETWORK_ERROR")
				result.message = aiBackendUnavailableMessage().toStdString();
			if (completion)
				completion(std::move(result));
		});
}

void generateCaptionFromValidatedLicense(
	const clipcoach::ClipMetadata &clip,
	clipcoach::AiAssistantConfiguration configuration,
	clipcoach::ui::CaptionGenerationProgressCallback progress,
	std::function<void(clipcoach::ui::CaptionGenerationResult)> complete)
{
	auto fail = [complete](QString message) mutable {
		complete({false, {}, std::move(message)});
	};
	auto analyze = [clip, configuration, progress, complete](std::string transcript) mutable {
		if (aiAssistant == nullptr) {
			complete({false, {}, QStringLiteral("El Asistente AI dejó de estar disponible.")});
			return;
		}
		if (progress)
			progress({82, uiText("Clips.Caption.GeneratingAi"), 30});
		analyzeClipWithAuthorization(clip, std::move(transcript), configuration,
					     [progress, complete](clipcoach::AiAssistantResult result) mutable {
						     clipcoach::ui::CaptionGenerationResult output;
						     if (result.success && result.response.has_value()) {
							     const auto hashtags = responseHashtags(*result.response);
							     output.caption = clipcoach::ui::formatSocialCaption(
								     QString::fromStdString(result.response->caption), hashtags,
								     QString::fromStdString(result.response->summary));
							     const auto title = result.response->suggestedTitles.empty()
								     ? QString::fromStdString(result.response->caption)
								     : QString::fromStdString(result.response->suggestedTitles.front());
							     output.youtubeShortsCaption =
								     clipcoach::ui::formatYouTubeShortsCaption(title, output.caption);
							     output.success = !output.caption.isEmpty();
						     }
						     if (!output.success) {
							     output.error = QString::fromStdString(result.message);
							     if (output.error.trimmed().isEmpty())
								     output.error = QStringLiteral(
									     "No se pudo generar el caption sugerido.");
						     }
						     if (progress)
							     progress({100, uiText("Clips.Caption.Finalizing"), 0});
						     complete(std::move(output));
					     });
	};

	if (!clip.transcriptPath.empty() && std::filesystem::is_regular_file(clip.transcriptPath)) {
		auto transcript = readFile(clip.transcriptPath);
		if (!transcript.empty() && transcript.size() <= 100'000) {
			if (progress)
				progress({75, uiText("Clips.Caption.AnalyzingMedia"), 30});
			analyze(std::move(transcript));
			return;
		}
	}
	if (captionTranscriber == nullptr) {
		fail(QStringLiteral("El motor local de transcripción no está disponible."));
		return;
	}
	captionTranscriber->transcribe(
		clip.filePath, clipcoach::AiAssistantService::languageCode(configuration.language),
		[progress, duration = std::max(1, clip.durationSeconds)](int localPercentage) {
			if (!progress)
				return;
			const int percentage = 15 + ((std::clamp(localPercentage, 0, 100) * 60) / 100);
			const int transcriptionEstimate = std::clamp(duration * 2, 15, 180);
			const int remaining = 30 +
				((100 - std::clamp(localPercentage, 0, 100)) * transcriptionEstimate / 100);
			progress({percentage, uiText("Clips.Caption.AnalyzingMedia"), remaining});
		},
		[analyze = std::move(analyze), fail](clipcoach::plugin::ClipTranscriptionResult result) mutable {
			if (!result.success) {
				fail(QString::fromStdString(result.error));
				return;
			}
			analyze(std::move(result.transcript));
		});
}

void generateCaption(const clipcoach::ClipMetadata &clip,
		     clipcoach::ui::CaptionGenerationProgressCallback progress,
		     clipcoach::ui::CaptionGenerationCompletion completion)
{
	if (progress)
		progress({3, uiText("Clips.Caption.ValidatingLicense"), 10});
	auto sharedCompletion = std::make_shared<clipcoach::ui::CaptionGenerationCompletion>(std::move(completion));
	auto complete = [sharedCompletion](clipcoach::ui::CaptionGenerationResult result) mutable {
		if (*sharedCompletion) {
			auto callback = std::move(*sharedCompletion);
			*sharedCompletion = {};
			callback(std::move(result));
		}
	};
	auto fail = [complete](QString message) mutable {
		complete({false, {}, std::move(message)});
	};
	if (aiAssistant == nullptr || settingsManager == nullptr) {
		fail(QStringLiteral("El Asistente AI no está disponible en esta sesión."));
		return;
	}
	const auto configuration = aiConfiguration(clip);
	if (!configuration.enabled) {
		fail(QStringLiteral("Activa el Asistente AI en Ajustes para generar captions."));
		return;
	}
	if (!configuration.privacyConsent) {
		fail(QStringLiteral(
			"Acepta el envío de la transcripción en Ajustes. El video y el audio no salen de este equipo."));
		return;
	}

	if (licenseManager == nullptr) {
		fail(aiAuthorizationRequiredMessage());
		return;
	}

	// Validate the license online before spending time extracting and transcribing
	// audio. This also makes a revoked/replaced key fail promptly instead of leaving
	// the card apparently stuck in local audio analysis.
	licenseManager->refresh(
		[clip, configuration, progress, complete, fail](const clipcoach::licensing::LicenseSnapshot &snapshot) mutable {
			if (licenseManager == nullptr)
				return fail(aiAuthorizationRequiredMessage());
			if (!snapshot.lastErrorCode.empty() ||
			    snapshot.state != clipcoach::licensing::LicenseState::ProActive ||
			    licenseManager->authorizationToken().empty()) {
				const auto code = QString::fromStdString(snapshot.lastErrorCode);
				return fail(isInvalidLicenseCode(code)
						 ? aiInvalidLicenseMessage()
						 : code == QStringLiteral("LICENSE_SIGNING_UNAVAILABLE") ||
							 code == QStringLiteral("SERVER_MISCONFIGURED")
						 ? aiLicenseServiceUnavailableMessage()
						 : aiSynchronizationRequiredMessage());
			}
			if (progress)
				progress({12, uiText("Clips.Caption.AnalyzingMedia"),
					  std::clamp(std::max(1, clip.durationSeconds) * 2 + 30, 45, 210)});
			generateCaptionFromValidatedLicense(clip, std::move(configuration),
						    std::move(progress), std::move(complete));
		});
}

void queueAiAnalysis(const clipcoach::ClipMetadata &clip)
{
	if (aiAssistant == nullptr || settingsManager == nullptr ||
	    clip.filePath.empty() ||
	    !std::filesystem::is_regular_file(clip.filePath))
		return;
	const auto configuration = aiConfiguration(clip);
	if (!configuration.enabled || !configuration.privacyConsent)
		return;
	if (!aiAnalysisInFlight.insert(clip.id).second)
		return;

	auto finish = [clipId = clip.id](clipcoach::AiAssistantResult result) {
		aiAnalysisInFlight.erase(clipId);
		if (!result.success)
			blog(LOG_WARNING, "%s AI scoring failed for %s: %s", kLogPrefix,
			     clipId.c_str(), result.code.c_str());
	};
	auto analyze = [clip, configuration, finish](std::string transcript) mutable {
		if (transcript.empty() || transcript.size() > 100'000) {
			aiAnalysisInFlight.erase(clip.id);
			blog(LOG_WARNING, "%s AI scoring skipped: transcript is empty or too large", kLogPrefix);
			return;
		}
		analyzeClipWithAuthorization(clip, std::move(transcript), configuration,
					     std::move(finish));
	};

	if (!clip.transcriptPath.empty() &&
	    std::filesystem::is_regular_file(clip.transcriptPath)) {
		auto transcript = readFile(clip.transcriptPath);
		analyze(std::move(transcript));
		return;
	}
	if (captionTranscriber == nullptr) {
		aiAnalysisInFlight.erase(clip.id);
		return;
	}

	captionTranscriber->transcribe(
		clip.filePath,
		clipcoach::AiAssistantService::languageCode(configuration.language),
		[](int) {},
		[analyze = std::move(analyze), clipId = clip.id](
			clipcoach::plugin::ClipTranscriptionResult result) mutable {
			if (!result.success) {
				aiAnalysisInFlight.erase(clipId);
				blog(LOG_WARNING, "%s Local transcription failed for AI scoring: %s",
				     kLogPrefix, result.error.c_str());
				return;
			}
			analyze(std::move(result.transcript));
		});
}

QString clipOrientation(const clipcoach::ClipOrientation orientation)
{
	switch (orientation) {
	case clipcoach::ClipOrientation::Vertical:
		return QStringLiteral("vertical");
	case clipcoach::ClipOrientation::Both:
		return QStringLiteral("both");
	case clipcoach::ClipOrientation::Horizontal:
	default:
		return QStringLiteral("horizontal");
	}
}

QString clipTrigger(const clipcoach::TriggerType trigger)
{
	switch (trigger) {
	case clipcoach::TriggerType::Voice:
		return QStringLiteral("voice");
	case clipcoach::TriggerType::AudioSpike:
		return QStringLiteral("audio_spike");
	case clipcoach::TriggerType::Chat:
		return QStringLiteral("chat");
	case clipcoach::TriggerType::Scene:
		return QStringLiteral("scene");
	case clipcoach::TriggerType::Ai:
		return QStringLiteral("ai");
	case clipcoach::TriggerType::Manual:
	default:
		return QStringLiteral("manual");
	}
}

void queueClipTelemetry(const clipcoach::ClipMetadata &clip)
{
	if (licenseManager == nullptr)
		return;
	const auto token = licenseManager->authorizationToken();
	if (token.empty())
		return;
	if (!clipTelemetryNetwork)
		clipTelemetryNetwork = std::make_unique<QNetworkAccessManager>();

	QUrl url(QStringLiteral(CLIPX_SERVICE_BASE_URL));
	url.setPath(QStringLiteral("/api/clips/events"));
	QNetworkRequest request(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("Authorization", QByteArray("Bearer ") + QByteArray::fromStdString(token));
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	request.setTransferTimeout(10'000);

	const auto capturedAt = QDateTime::fromMSecsSinceEpoch(
		std::chrono::duration_cast<std::chrono::milliseconds>(clip.createdAt.time_since_epoch()).count(),
		QTimeZone::UTC);
	const QJsonObject payload{
		{QStringLiteral("clip_id"), QString::fromStdString(clip.id)},
		{QStringLiteral("session_id"), QString::fromStdString(clip.sessionId)},
		{QStringLiteral("orientation"), clipOrientation(clip.orientation)},
		{QStringLiteral("trigger_type"), clipTrigger(clip.triggerType)},
		{QStringLiteral("score"), clip.score},
		{QStringLiteral("duration_seconds"), clip.durationSeconds},
		{QStringLiteral("captured_at"), capturedAt.toString(Qt::ISODateWithMs)},
		{QStringLiteral("app_version"), QString::fromStdString(clip.appVersion)},
	};
	auto *reply = clipTelemetryNetwork->post(request, QJsonDocument(payload).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply] {
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 300)
			blog(LOG_WARNING, "%s Clip activity sync failed: HTTP %d (%s)", kLogPrefix, status,
			     reply->errorString().toUtf8().constData());
		reply->deleteLater();
	});
}

void initializeAiAssistant()
{
	if (featureGateService == nullptr)
		return;
	aiApi = std::make_unique<clipcoach::network::QtAiApi>(QList<QUrl>{
		QUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL)), QUrl(QStringLiteral(CLIPX_LOCAL_SERVICE_BASE_URL))});
	aiAssistant = std::make_unique<clipcoach::AiAssistantService>(
		*aiApi, *featureGateService,
		[] { return licenseManager != nullptr ? licenseManager->authorizationToken() : std::string{}; },
		[](const clipcoach::AiPersistedClipResult &result, std::string *) {
			if (libraryService == nullptr)
				return false;
			libraryService->storeAiResult(
				result, [](clipcoach::storage::StorageStatus status) {
					if (!status.success)
						return;
					QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
					if (!dock.isNull())
						QMetaObject::invokeMethod(
							dock, [dock] {
								if (!dock.isNull())
									dock->refreshClipLibrary();
							}, Qt::QueuedConnection);
				});
			return true;
		},
		[](const std::string &sessionId, const std::string &summary, clipcoach::AiLanguage language,
		   std::string *) {
			if (libraryService == nullptr)
				return false;
			libraryService->storeSessionAiSummary(sessionId, summary, language);
			return true;
		});
}

void initializeCaptionTranscriber()
{
	const auto ffmpeg = bundledFfmpegPath();
	const auto model = bundledWhisperModelPath();
	if (!std::filesystem::is_regular_file(ffmpeg) || !std::filesystem::is_regular_file(model)) {
		blog(LOG_WARNING,
		     "%s Local caption transcription is unavailable because a bundled runtime file is missing",
		     kLogPrefix);
		captionTranscriber.reset();
		return;
	}
	captionTranscriber = std::make_unique<clipcoach::plugin::ClipCaptionTranscriber>(ffmpeg, model);
	blog(LOG_INFO, "%s Local caption transcription ready", kLogPrefix);
}

void reportRemoteResult(clipcoach::remote::RemoteCommandResult result, int attempt = 0)
{
	if (remoteResultOutbox != nullptr && attempt == 0 && !remoteResultOutbox->enqueue(result)) {
		blog(LOG_ERROR, "%s Remote result could not be persisted before synchronization", kLogPrefix);
		return;
	}
	if (remoteClipperClient == nullptr || licenseManager == nullptr || frontendShuttingDown)
		return;
	if (attempt == 0 && !remoteResultsInFlight.insert(result.commandUuid).second) return;
	const auto token = licenseManager->authorizationToken();
	if (token.empty()) {
		remoteResultsInFlight.erase(result.commandUuid);
		return;
	}
	const auto retryCopy = result;
	remoteClipperClient->reportResult(
		result, token, [result = retryCopy, attempt](auto response) mutable {
			if (response.succeeded()) {
				if (remoteResultOutbox != nullptr) remoteResultOutbox->remove(result.commandUuid);
				remoteResultsInFlight.erase(result.commandUuid);
				return;
			}
			if (response.error.retryable && attempt < 5 && !frontendShuttingDown) {
				const auto delayMs = std::min(30'000, 1000 * (1 << attempt));
				QTimer::singleShot(delayMs, [result = std::move(result), attempt]() mutable {
					reportRemoteResult(std::move(result), attempt + 1);
				});
				return;
			}
			remoteResultsInFlight.erase(result.commandUuid);
			blog(LOG_WARNING, "%s Remote command result could not be synchronized (%s)",
			     kLogPrefix, response.error.code.c_str());
		});
}

void persistRemoteCommandUuid(const std::string &uuid)
{
	if (remoteCommandState == nullptr || !clipcoach::remote::isValidCommandUuid(uuid))
		return;
	auto values = remoteCommandState->value(QStringLiteral("processed_commands")).toStringList();
	const auto value = QString::fromStdString(uuid);
	values.removeAll(value);
	values.push_back(value);
	while (values.size() > 200)
		values.removeFirst();
	remoteCommandState->setValue(QStringLiteral("processed_commands"), values);
	remoteCommandState->sync();
}

clipcoach::remote::RemoteHeartbeatRequest remoteHeartbeat()
{
	clipcoach::remote::RemoteHeartbeatRequest request;
	if (licenseManager != nullptr)
		request.deviceActivationId = licenseManager->snapshot().activationId;
	request.pluginVersion = CLIPCOACH_VERSION;
	request.obsVersion = obs_get_version_string();
	if (clipManager != nullptr) {
		switch (clipManager->replayState()) {
		case clipcoach::ReplayState::Active: request.replayBufferStatus = "active"; break;
		case clipcoach::ReplayState::Inactive: request.replayBufferStatus = "inactive"; break;
		default: request.replayBufferStatus = "unknown"; break;
		}
	}
	request.verticalCanvasStatus = verticalCanvasManager != nullptr ? "active" : "inactive";
	request.currentScene = currentSceneName();
	return request;
}

bool initializeRemoteClipper()
{
	if (licenseManager == nullptr || clipManager == nullptr || exportManager == nullptr ||
	    settingsManager == nullptr)
		return false;
	char *rawStatePath = obs_module_config_path("remote-clipper-state.ini");
	if (rawStatePath != nullptr) {
		remoteCommandState = std::make_unique<QSettings>(QString::fromUtf8(rawStatePath), QSettings::IniFormat);
		remoteResultOutbox = std::make_unique<clipcoach::remote::RemoteResultOutbox>(QString::fromUtf8(rawStatePath));
		bfree(rawStatePath);
	}
	remoteClipperClient = std::make_unique<clipcoach::network::RemoteClipperClient>(
		QUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL)));
	remoteCaptureCoordinator = std::make_unique<clipcoach::plugin::RemoteCaptureCoordinator>(
		*clipManager, *exportManager, *settingsManager, [](int) {});
	remoteCommandExecutor = std::make_unique<clipcoach::remote::RemoteCommandExecutor>(
		[](const clipcoach::remote::RemoteCapturePlan &plan, auto completion) {
			if (remoteCaptureCoordinator == nullptr) {
				completion({plan.commandUuid, false, {}, {}, 0, {}, {}, "CAPTURE_UNAVAILABLE",
					    "Remote capture service is unavailable"});
				return;
			}
			remoteCaptureCoordinator->capture(plan, std::move(completion));
		},
		[](const clipcoach::remote::RemoteCommand &command, auto completion) {
			if (clipManager == nullptr) {
				completion({command.uuid, false, {}, {}, 0, {}, {}, "MARK_MOMENT_UNAVAILABLE",
					    "Mark moment service is unavailable"});
				return;
			}
			const auto marked = clipManager->markMoment("remote_clipper", command.delayCompensationSeconds);
			completion({command.uuid, marked.accepted, {}, {}, 0, {},
				    marked.accepted ? "Moment marked locally" : std::string{},
				    marked.accepted ? std::string{} : "MARK_MOMENT_FAILED",
				    marked.accepted ? std::string{} : marked.message});
		},
		persistRemoteCommandUuid);
	if (remoteCommandState != nullptr) {
		std::set<std::string> processed;
		for (const auto &value : remoteCommandState->value(QStringLiteral("processed_commands")).toStringList()) {
			const auto uuid = value.toStdString();
			if (clipcoach::remote::isValidCommandUuid(uuid)) processed.insert(uuid);
		}
		remoteCommandExecutor->preloadProcessed(std::move(processed));
	}

	clipcoach::network::RemoteCommandPoller::Providers providers;
	providers.proActive = [] { return licenseManager != nullptr && licenseManager->snapshot().proEnabled(); };
	providers.localCommandsEnabled = [] {
		return settingsManager != nullptr && settingsManager->settings().remoteCommandsEnabled;
	};
	providers.shuttingDown = [] { return frontendShuttingDown; };
	providers.bearerToken = [] {
		return licenseManager != nullptr ? licenseManager->authorizationToken() : std::string{};
	};
	providers.heartbeat = remoteHeartbeat;
	remoteCommandPoller = std::make_unique<clipcoach::network::RemoteCommandPoller>(
		*remoteClipperClient, std::move(providers));
	remoteCommandPoller->setStatusCallback([](const clipcoach::remote::RemoteClipperStatus &status) {
		if (status.remoteEnabled && !remoteReplayCapacityPrepared) {
			remoteReplayCapacityPrepared = true;
			ensureObsReplayCapacity(240);
		}
		QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
		if (!dock.isNull()) {
			QMetaObject::invokeMethod(dock, [dock, status] {
				if (!dock.isNull()) dock->setRemoteClipperStatus(status);
			}, Qt::QueuedConnection);
		}
	});
	remoteCommandPoller->setCommandsCallback([](std::vector<clipcoach::remote::RemoteCommand> commands) {
		if (remoteCommandExecutor == nullptr || remoteClipperClient == nullptr || licenseManager == nullptr) return;
		for (auto &command : commands) {
			auto pending = std::make_shared<clipcoach::remote::RemoteCommand>(std::move(command));
			remoteClipperClient->markProcessing(pending->uuid, licenseManager->authorizationToken(), [pending](auto ack) {
				if (!ack.succeeded() || remoteCommandExecutor == nullptr) {
					blog(LOG_WARNING, "%s Remote command was not executed because processing acknowledgement failed", kLogPrefix);
					return;
				}
				remoteCommandExecutor->submit(std::move(*pending), [](auto result) {
					reportRemoteResult(std::move(result));
				});
			});
		}
	});
	remoteCommandPoller->start();
	if (remoteResultOutbox != nullptr)
		for (auto &result : remoteResultOutbox->pending()) reportRemoteResult(std::move(result));
	blog(LOG_INFO, "%s Remote Clipper polling initialized", kLogPrefix);
	return true;
}

void shutdownRemoteClipper() noexcept
{
	if (remoteCommandPoller != nullptr) remoteCommandPoller->stop();
	if (remoteCaptureCoordinator != nullptr) remoteCaptureCoordinator->cancel();
	remoteCommandPoller.reset();
	remoteCommandExecutor.reset();
	remoteCaptureCoordinator.reset();
	remoteClipperClient.reset();
	remoteResultOutbox.reset();
	remoteResultsInFlight.clear();
	remoteCommandState.reset();
	remoteReplayCapacityPrepared = false;
}

void shutdownAiAssistant() noexcept
{
	captionTranscriber.reset();
	aiAssistant.reset();
	aiApi.reset();
}

std::string readFile(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
		return {};
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool initializeLicensing()
{
	char *rawInstallIdPath = obs_module_config_path("install-id");
	if (rawInstallIdPath == nullptr) {
		blog(LOG_ERROR, "%s OBS did not provide an install identity path", kLogPrefix);
		return false;
	}
	const std::filesystem::path installIdPath(rawInstallIdPath);
	bfree(rawInstallIdPath);

	std::string publicKey;
	char *rawPublicKeyPath = obs_module_file("license-public.pem");
	if (rawPublicKeyPath != nullptr) {
		publicKey = readFile(std::filesystem::path(rawPublicKeyPath));
		bfree(rawPublicKeyPath);
	}
	if (publicKey.empty()) {
		blog(LOG_WARNING, "%s license-public.pem is not packaged; Pro activation will remain unavailable",
		     kLogPrefix);
	}

	// Compatibility namespace: changing this visible-to-Windows credential name
	// would orphan license tokens created by releases before the ClipXtudio rename.
	licenseSecureStorage = clipcoach::security::createPlatformSecureStorage("ClipX Studio");
	licenseFingerprintProvider =
		std::make_unique<clipcoach::licensing::FileMachineFingerprintProvider>(installIdPath);
	licenseApi = std::make_unique<clipcoach::network::QtLicenseApi>(QList<QUrl>{
		QUrl(QStringLiteral(CLIPX_SERVICE_BASE_URL)), QUrl(QStringLiteral(CLIPX_LOCAL_SERVICE_BASE_URL))});
	licenseTokenVerifier = std::make_unique<clipcoach::security::Rs256TokenVerifier>(
		std::move(publicKey), "clipcoach-studio", "clipcoach-native-plugin", "clipcoach-rs256-v1");
	licenseManager = std::make_unique<clipcoach::licensing::LicenseManager>(
		*licenseSecureStorage, *licenseApi, *licenseTokenVerifier, *licenseFingerprintProvider);
	std::string error;
	if (!licenseManager->initialize(&error) && !error.empty()) {
		blog(LOG_WARNING, "%s License cache unavailable: %s", kLogPrefix, error.c_str());
	}
	licenseRefreshTimer = std::make_unique<QTimer>();
	licenseRefreshTimer->setInterval(
		static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::hours(6)).count()));
	QObject::connect(licenseRefreshTimer.get(), &QTimer::timeout, [] {
		if (licenseManager != nullptr)
			licenseManager->periodicRefresh();
	});
	licenseRefreshTimer->start();
	QTimer::singleShot(0, [] {
		if (licenseManager != nullptr && licenseManager->shouldRefresh())
			licenseManager->periodicRefresh();
	});
	return true;
}

void bindLicenseGates()
{
	if (licenseManager == nullptr)
		return;
	licenseGateObserverId = licenseManager->addObserver([](const clipcoach::licensing::LicenseSnapshot &snapshot) {
		const bool pro = snapshot.proEnabled();
		if (featureGateService != nullptr) {
			featureGateService->setEntitlementState(
				snapshot.state == clipcoach::licensing::LicenseState::ProActive
					? clipcoach::EntitlementState::ProActive
				: snapshot.state == clipcoach::licensing::LicenseState::ProGrace
					? clipcoach::EntitlementState::ProOfflineGrace
				: snapshot.lastErrorCode == "SUBSCRIPTION_INACTIVE"
					? clipcoach::EntitlementState::Revoked
					: clipcoach::EntitlementState::Free);
		}
		if (triggerEngine != nullptr)
			triggerEngine->setProUnlocked(pro);
		if (voiceTriggerController != nullptr)
			voiceTriggerController->setProUnlocked(pro);
		if (verticalCanvasManager != nullptr)
			verticalCanvasManager->setProUnlocked(pro);
		blog(LOG_INFO, "%s Plan changed to %s", kLogPrefix, pro ? "Pro" : "Free");
		if (remoteCommandPoller != nullptr)
			remoteCommandPoller->notifyCredentialsChanged();
	});
}

void shutdownLicensing() noexcept
{
	shutdownAiAssistant();
	if (licenseManager != nullptr && licenseGateObserverId != 0) {
		licenseManager->removeObserver(licenseGateObserverId);
		licenseGateObserverId = 0;
	}
	if (licenseRefreshTimer != nullptr)
		licenseRefreshTimer->stop();
	licenseRefreshTimer.reset();
	licenseApi.reset();
	licenseManager.reset();
	licenseTokenVerifier.reset();
	licenseFingerprintProvider.reset();
	licenseSecureStorage.reset();
}

void removeDockSafely() noexcept
{
	if (!dockRegistered) {
		return;
	}

	if (!frontendShuttingDown) {
		obs_frontend_remove_dock(kDockId);
	}
	mainDockContent.clear();
	dockRegistered = false;
}

void frontendEventCallback(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		frontendShuttingDown = true;
		if (remoteCommandPoller != nullptr)
			remoteCommandPoller->stop();
		lifecycleCallbackRegistered = false;
		if (voiceTriggerController != nullptr) {
			// Detach from OBS audio while its sources and signal handlers are
			// still alive. Waiting for obs_module_unload is too late because
			// libobs may already be destroying the captured source.
			voiceTriggerController->shutdown();
		}
		if (hotkeyAdapter != nullptr) {
			hotkeyAdapter->stopPersistence();
			hotkeyAdapter->notifyFrontendShutdown();
		}
	} else if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
#ifdef _WIN32
		// OBS loads scene collections and their audio sources after module load.
		// Retry the native audio attachment now that Mic/Aux and input sources exist.
		if (voiceTriggerController != nullptr && settingsManager != nullptr)
			voiceTriggerController->applySettings(settingsManager->settings());
#endif
		if (settingsManager != nullptr) {
			const auto &settings = settingsManager->settings();
			ensureObsReplayCapacity(settings.preRollSeconds +
						settings.postRollSeconds);
		}
		QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
		if (!dock.isNull())
			QMetaObject::invokeMethod(dock, [dock] {
				if (!dock.isNull()) {
					dock->refreshVerticalObsScenes();
					dock->showInitialSetupIfNeeded();
				}
			}, Qt::QueuedConnection);
	} else if (event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED ||
		   event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED) {
		QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
		if (!dock.isNull())
			QMetaObject::invokeMethod(dock, [dock] {
				if (!dock.isNull())
					dock->refreshVerticalObsScenes();
			}, Qt::QueuedConnection);
	} else if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED && triggerEngine != nullptr) {
		clipcoach::TriggerSignal signal;
		signal.type = clipcoach::SmartTriggerType::Scene;
		signal.scene = currentSceneName();
		signal.sceneRelevance = 1.0;
		(void)triggerEngine->process(signal);
	}
}

bool initializeSettings()
{
	char *rawConfigPath = obs_module_config_path("settings.json");
	if (rawConfigPath == nullptr) {
		blog(LOG_WARNING, "%s OBS did not provide a module configuration path", kLogPrefix);
		return false;
	}

	const std::filesystem::path configPath(rawConfigPath);
	bfree(rawConfigPath);

	auto manager = std::make_unique<clipcoach::SettingsManager>(configPath);
	std::string error;
	if (!manager->load(&error)) {
		blog(LOG_WARNING, "%s Settings could not be loaded: %s", kLogPrefix, error.c_str());
		return false;
	}

	settingsManager = std::move(manager);
	return true;
}

std::string takeObsString(char *value)
{
	if (value == nullptr) {
		return {};
	}
	std::string result(value);
	bfree(value);
	return result;
}

std::string createSessionId()
{
	const auto now = std::chrono::system_clock::now();
	const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
	std::random_device random;
	std::ostringstream id;
	id << "session-" << timestamp << '-' << std::hex << std::setw(8) << std::setfill('0') << random();
	return id.str();
}

std::string currentSceneName()
{
	obs_source_t *scene = obs_frontend_get_current_scene();
	if (scene == nullptr) {
		return {};
	}
	const char *name = obs_source_get_name(scene);
	const std::string result = name != nullptr ? name : "";
	obs_source_release(scene);
	return result;
}

std::vector<std::string> availableSceneNames()
{
	std::vector<std::string> result;
	obs_frontend_source_list scenes{};
	obs_frontend_get_scenes(&scenes);
	result.reserve(scenes.sources.num);
	for (std::size_t index = 0; index < scenes.sources.num; ++index) {
		auto *scene = scenes.sources.array[index];
		const char *name = scene != nullptr ? obs_source_get_name(scene) : nullptr;
		if (name != nullptr && *name != '\0')
			result.emplace_back(name);
	}
	obs_frontend_source_list_free(&scenes);
	std::sort(result.begin(), result.end());
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

bool initializeLibrary()
{
	char *rawDatabasePath = obs_module_config_path("clipcoach.db");
	if (rawDatabasePath == nullptr) {
		blog(LOG_ERROR, "%s OBS did not provide a clip library path", kLogPrefix);
		return false;
	}
	const std::filesystem::path databasePath(rawDatabasePath);
	bfree(rawDatabasePath);

	libraryService = std::make_unique<clipcoach::storage::ClipLibraryService>(
		databasePath, [](bool error, const std::string &message) {
			blog(error ? LOG_ERROR : LOG_INFO, "%s %s", kLogPrefix, message.c_str());
		});

	currentSessionId = createSessionId();
	clipcoach::SessionMetadata session;
	session.id = currentSessionId;
	session.startedAt = std::chrono::system_clock::now();
	session.profileName = takeObsString(obs_frontend_get_current_profile());
	session.sceneCollection = takeObsString(obs_frontend_get_current_scene_collection());
	session.appVersion = CLIPCOACH_VERSION;
	libraryService->storeSession(std::move(session));
	backfillRecentThumbnails();
	return true;
}

void shutdownLibrarySafely() noexcept
{
	try {
		shutdownThumbnailGeneration();
		if (libraryService != nullptr && !currentSessionId.empty()) {
			libraryService->endSession(currentSessionId, std::chrono::system_clock::now());
		}
		libraryService.reset();
		currentSessionId.clear();
	} catch (const std::exception &error) {
		blog(LOG_ERROR, "%s Clip library shutdown failed: %s", kLogPrefix, error.what());
		libraryService.reset();
		currentSessionId.clear();
	} catch (...) {
		blog(LOG_ERROR, "%s Clip library shutdown failed with an unknown error", kLogPrefix);
		libraryService.reset();
		currentSessionId.clear();
	}
}

bool registerMainDock()
{
	configureUiLocale();
	auto dock = std::make_unique<clipcoach::ui::MainDock>(
		[](const char *key) { return uiText(key); }, clipManager.get(), settingsManager.get(),
		libraryService.get(), nullptr, currentSessionId, verticalCanvasManager.get(), exportManager.get(),
		triggerEngine.get(), nullptr, licenseManager.get(), featureGateService.get(),
		voiceTriggerController.get(), availableSceneNames, clipcoach::plugin::makeObsVerticalBridge(),
		[](const clipcoach::ClipMetadata &clip,
		   clipcoach::ui::CaptionGenerationProgressCallback progress,
		   clipcoach::ui::CaptionGenerationCompletion completion) {
			generateCaption(clip, std::move(progress), std::move(completion));
		});
	auto *content = dock.get();
	content->setLanguageChangedCallback([] {
		int selectedTab = 0;
		if (!mainDockContent.isNull()) {
			if (auto *tabs = mainDockContent->findChild<QTabBar *>(QStringLiteral("mainTabBar")))
				selectedTab = tabs->currentIndex();
		}
		QTimer::singleShot(0, [selectedTab] {
			if (frontendShuttingDown)
				return;
			removeDockSafely();
			configureUiLocale();
			if (!registerMainDock()) {
				blog(LOG_ERROR, "%s Could not rebuild the dock after a language change", kLogPrefix);
				return;
			}
			if (!mainDockContent.isNull()) {
				if (auto *tabs = mainDockContent->findChild<QTabBar *>(QStringLiteral("mainTabBar")))
					tabs->setCurrentIndex(selectedTab);
			}
			blog(LOG_INFO, "%s UI language applied without restarting OBS", kLogPrefix);
		});
	});
	content->setHotkeySettingsChangedCallback([](const clipcoach::Settings &settings) {
		if (hotkeyAdapter != nullptr && !hotkeyAdapter->applySettings(settings)) {
			blog(LOG_WARNING, "%s One or more Settings shortcuts could not be applied", kLogPrefix);
		}
	});
	content->setReplayDurationChangedCallback([](int requiredSeconds) {
		ensureObsReplayCapacity(requiredSeconds);
	});
	const auto dockTitle = uiText("Dock.Title").toUtf8();
	if (!obs_frontend_add_dock_by_id(kDockId, dockTitle.constData(), content)) {
		blog(LOG_ERROR, "%s Could not register the main dock; the dock id may already be in use", kLogPrefix);
		return false;
	}

	// OBS wraps the widget in its own QDockWidget and owns it after registration.
	mainDockContent = content;
	content->setRemoteCommandsChangedCallback([](bool) {
		if (remoteCommandPoller != nullptr)
			remoteCommandPoller->notifyCredentialsChanged();
	});
	if (remoteCommandPoller != nullptr)
		content->setRemoteClipperStatus(remoteCommandPoller->status());
	(void)dock.release();
	dockRegistered = true;
	QPointer<clipcoach::ui::MainDock> setupDock = mainDockContent;
	QTimer::singleShot(1500, content, [setupDock] {
		if (!setupDock.isNull())
			setupDock->showInitialSetupIfNeeded();
	});
	return true;
}

void executeTriggerAction(const clipcoach::TriggerEvent &event)
{
	const bool saveHorizontal = event.action == clipcoach::TriggerAction::SaveClip;
	const bool saveVertical = event.action == clipcoach::TriggerAction::SaveVerticalClip;
	const bool saveBoth = event.action == clipcoach::TriggerAction::SaveBoth;
	if ((!saveHorizontal && !saveVertical && !saveBoth) || event.primaryType == clipcoach::SmartTriggerType::Manual)
		return;
	QPointer<clipcoach::ui::MainDock> context = mainDockContent;
	if (context.isNull())
		return;
	const auto configuredPostRoll = std::max(
		0,
		static_cast<int>(
			std::chrono::duration_cast<std::chrono::seconds>(event.captureEnd - event.occurredAt).count()));
	const auto postRoll = configuredPostRoll;
	const auto requestedDuration = std::clamp(
		static_cast<int>(
			std::chrono::duration_cast<std::chrono::seconds>(event.captureEnd - event.captureStart).count()),
		clipcoach::settings_constraints::kMinClipDurationSeconds,
		clipcoach::settings_constraints::kMaxClipDurationSeconds);
	const auto type = persistedTriggerType(event.primaryType);
	const auto label = triggerLabel(event);
	const auto score = event.score;
	auto output = clipcoach::ExportOrientation::Horizontal;
	if (saveVertical) {
		output = clipcoach::ExportOrientation::Vertical;
	} else if (saveBoth) {
		output = clipcoach::ExportOrientation::Both;
	} else if (verticalCanvasManager != nullptr) {
		switch (verticalCanvasManager->settings().outputMode) {
		case clipcoach::CaptureOutputMode::Vertical:
			output = clipcoach::ExportOrientation::Vertical;
			break;
		case clipcoach::CaptureOutputMode::Both:
			output = clipcoach::ExportOrientation::Both;
			break;
		case clipcoach::CaptureOutputMode::Horizontal:
			break;
		}
	}
	QMetaObject::invokeMethod(
		context,
		[context, postRoll, requestedDuration, type, label, score, output] {
			if (context.isNull())
				return;
			// Feedback starts at detection time. Saving happens only after the
			// configured post-roll has elapsed so the requested window is real.
			context->showCapturePending();
			QTimer::singleShot(postRoll * 1000, context,
					   [context, requestedDuration, type, label, score, output] {
						   if (context.isNull())
							   return;
						   const auto result = context->requestTriggeredCapture(
							   requestedDuration, type, label, score,
							   output);
						   if (!result.accepted) {
							   blog(LOG_WARNING, "%s Smart trigger save failed: %s",
								kLogPrefix, result.message.c_str());
						   }
					   });
		},
		Qt::QueuedConnection);
}

clipcoach::HotkeyActionResult captureFromHotkey(int seconds)
{
	if (clipManager == nullptr) {
		return clipcoach::HotkeyActionResult::fail("capture service is unavailable");
	}
	QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
	clipcoach::CaptureResult result;
	if (!dock.isNull()) {
		auto output = clipcoach::ExportOrientation::Horizontal;
		if (verticalCanvasManager != nullptr) {
			const auto mode = verticalCanvasManager->settings().outputMode;
			output = mode == clipcoach::CaptureOutputMode::Vertical
					 ? clipcoach::ExportOrientation::Vertical
				 : mode == clipcoach::CaptureOutputMode::Both
					 ? clipcoach::ExportOrientation::Both
					 : clipcoach::ExportOrientation::Horizontal;
		}
		const auto invoke = [&] {
			if (!dock.isNull())
				result = dock->requestTriggeredCapture(
					seconds, clipcoach::TriggerType::Manual,
					"manual", 0, output);
		};
		if (QThread::currentThread() == dock->thread())
			invoke();
		else
			QMetaObject::invokeMethod(dock, invoke,
						  Qt::BlockingQueuedConnection);
	} else {
		result = clipManager->captureManual(seconds);
	}
	if (result.accepted && triggerEngine != nullptr) {
		clipcoach::TriggerSignal signal;
		signal.type = clipcoach::SmartTriggerType::Manual;
		signal.manualMarker = true;
		signal.durationSeconds = seconds;
		(void)triggerEngine->process(signal);
	}
	return result.accepted ? clipcoach::HotkeyActionResult::ok()
			       : clipcoach::HotkeyActionResult::fail(result.message);
}

clipcoach::HotkeyActionResult captureVerticalFromHotkey()
{
	QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
	if (dock.isNull())
		return clipcoach::HotkeyActionResult::fail("dock is unavailable");
	const auto seconds = settingsManager != nullptr ? settingsManager->settings().defaultDurationSeconds
							: clipcoach::settings_constraints::kDefaultClipDurationSeconds;
	clipcoach::CaptureResult capture;
	const auto invoke = [&] {
		if (!dock.isNull())
			capture = dock->requestTriggeredCapture(
				seconds, clipcoach::TriggerType::Manual, "manual", 0,
				clipcoach::ExportOrientation::Vertical);
	};
	if (QThread::currentThread() == dock->thread())
		invoke();
	else
		QMetaObject::invokeMethod(dock, invoke, Qt::BlockingQueuedConnection);
	return capture.accepted ? clipcoach::HotkeyActionResult::ok()
				: clipcoach::HotkeyActionResult::fail(capture.message);
}

clipcoach::HotkeyActionResult cycleOutputMode()
{
	if (verticalCanvasManager == nullptr) {
		return clipcoach::HotkeyActionResult::fail("vertical canvas service is unavailable");
	}
	auto mode = verticalCanvasManager->settings().outputMode;
	switch (mode) {
	case clipcoach::CaptureOutputMode::Horizontal:
		mode = clipcoach::CaptureOutputMode::Vertical;
		break;
	case clipcoach::CaptureOutputMode::Vertical:
		mode = clipcoach::CaptureOutputMode::Both;
		break;
	case clipcoach::CaptureOutputMode::Both:
		mode = clipcoach::CaptureOutputMode::Horizontal;
		break;
	}
	std::string error;
	if (!verticalCanvasManager->setOutputMode(mode, &error)) {
		return clipcoach::HotkeyActionResult::fail(error);
	}
	QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
	if (!dock.isNull())
		QMetaObject::invokeMethod(dock, [dock] {
			if (!dock.isNull())
				dock->refreshCaptureOutputMode();
		}, Qt::QueuedConnection);
	blog(LOG_INFO, "%s Output mode changed from native hotkey", kLogPrefix);
	return clipcoach::HotkeyActionResult::ok();
}

clipcoach::HotkeyActionResult toggleDock()
{
	QPointer<QWidget> guard = mainDockContent;
	if (guard.isNull()) {
		return clipcoach::HotkeyActionResult::fail("dock is unavailable");
	}
	QMetaObject::invokeMethod(
		guard,
		[guard] {
			if (guard.isNull()) {
				return;
			}
			QWidget *candidate = guard->parentWidget();
			QDockWidget *dock = nullptr;
			while (candidate != nullptr && dock == nullptr) {
				dock = qobject_cast<QDockWidget *>(candidate);
				candidate = candidate->parentWidget();
			}
			if (dock == nullptr) {
				blog(LOG_WARNING, "%s Dock hotkey could not resolve the OBS dock wrapper", kLogPrefix);
				return;
			}
			dock->setVisible(!dock->isVisible());
			if (dock->isVisible()) {
				dock->raise();
			}
		},
		Qt::QueuedConnection);
	return clipcoach::HotkeyActionResult::ok();
}

const char *hotkeyDescriptionKey(clipcoach::HotkeyAction action)
{
	switch (action) {
	case clipcoach::HotkeyAction::MarkMoment:
		return clipcoach::ui::strings::kHotkeyMarkMoment;
	case clipcoach::HotkeyAction::Save15Seconds:
		return clipcoach::ui::strings::kHotkeySave15;
	case clipcoach::HotkeyAction::Save30Seconds:
		return clipcoach::ui::strings::kHotkeySave30;
	case clipcoach::HotkeyAction::Save60Seconds:
		return clipcoach::ui::strings::kHotkeySave60;
	case clipcoach::HotkeyAction::Save2Minutes:
		return clipcoach::ui::strings::kHotkeySave2Minutes;
	case clipcoach::HotkeyAction::Save5Minutes:
		return clipcoach::ui::strings::kHotkeySave5Minutes;
	case clipcoach::HotkeyAction::SaveVertical:
		return clipcoach::ui::strings::kHotkeySaveVertical;
	case clipcoach::HotkeyAction::CycleOutputMode:
		return clipcoach::ui::strings::kHotkeyCycleOutput;
	case clipcoach::HotkeyAction::ToggleDock:
		return clipcoach::ui::strings::kHotkeyToggleDock;
	}
	return "Hotkey.Unknown";
}

bool initializeHotkeys()
{
	hotkeyAdapter = std::make_unique<clipcoach::plugin::ObsHotkeyAdapter>();
	clipcoach::HotkeyServices services;
	services.defaultDurationSeconds = [] {
		return settingsManager != nullptr ? settingsManager->settings().defaultDurationSeconds
						  : clipcoach::settings_constraints::kDefaultClipDurationSeconds;
	};
	services.capture = captureFromHotkey;
	services.captureVertical = captureVerticalFromHotkey;
	services.cycleOutputMode = cycleOutputMode;
	services.toggleDock = toggleDock;
	hotkeyManager = std::make_unique<clipcoach::HotkeyManager>(
		*hotkeyAdapter, std::move(services), [](clipcoach::HotkeyAction action) {
			return std::string(obs_module_text(hotkeyDescriptionKey(action)));
		});
	hotkeyManager->setErrorCallback([](clipcoach::HotkeyAction action, const std::string &error) {
		blog(LOG_WARNING, "%s Hotkey action %d failed: %s", kLogPrefix, static_cast<int>(action),
		     error.c_str());
	});
	if (!hotkeyManager->registerAll()) {
		blog(LOG_ERROR, "%s Native hotkey registration failed", kLogPrefix);
		hotkeyManager.reset();
		hotkeyAdapter.reset();
		return false;
	}
	hotkeyAdapter->startPersistence();
	if (settingsManager != nullptr && !hotkeyAdapter->applySettings(settingsManager->settings())) {
		blog(LOG_WARNING, "%s One or more saved shortcuts could not be applied", kLogPrefix);
	}
	blog(LOG_INFO, "%s Registered 9 native frontend hotkeys", kLogPrefix);
	return true;
}

void shutdownHotkeys() noexcept
{
	hotkeyManager.reset();
	hotkeyAdapter.reset();
}

} // namespace

MODULE_EXPORT const char *obs_module_name(void)
{
	return "ClipXtudio";
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Native clip capture and vertical workflow tools for OBS Studio.";
}

MODULE_EXPORT bool obs_module_load(void)
{
	try {
		blog(LOG_INFO, "%s Loading native plugin", kLogPrefix);
		configureQtNetworkPlugins();
		migrateLegacyModuleConfig();

		if (!initializeSettings()) {
			blog(LOG_WARNING, "%s Continuing with in-memory defaults", kLogPrefix);
		}
		if (!initializeLicensing()) {
			blog(LOG_WARNING, "%s Licensing could not initialize; continuing with Free", kLogPrefix);
		}
		const bool proUnlocked = licenseManager != nullptr && licenseManager->snapshot().proEnabled();
		featureGateService = std::make_unique<clipcoach::FeatureGateService>(
			proUnlocked ? clipcoach::EntitlementState::ProActive : clipcoach::EntitlementState::Free);
		replayManager = std::make_unique<clipcoach::plugin::ObsReplayManager>();
		clipManager = std::make_unique<clipcoach::ClipManager>(*replayManager, std::chrono::system_clock::now,
								       featureGateService.get());
		triggerEngine = std::make_unique<clipcoach::TriggerEngine>(proUnlocked);
		triggerEngine->setEventCallback(executeTriggerAction);
#ifdef _WIN32
		voiceTriggerController =
			std::make_unique<clipcoach::plugin::WindowsVoiceTriggerController>(*triggerEngine);
		voiceTriggerController->setProUnlocked(proUnlocked);
		if (settingsManager != nullptr)
			voiceTriggerController->applySettings(settingsManager->settings());
#endif
		if (settingsManager != nullptr) {
			verticalCanvasManager =
				std::make_unique<clipcoach::VerticalCanvasManager>(*settingsManager, proUnlocked);
		}
		bindLicenseGates();
		if (!initializeLibrary()) {
			blog(LOG_WARNING, "%s Clip library is unavailable for this session", kLogPrefix);
		}
		initializeAiAssistant();
		initializeCaptionTranscriber();
		const auto ffmpegPath = bundledFfmpegPath();
		if (!std::filesystem::is_regular_file(ffmpegPath))
			blog(LOG_ERROR, "%s Bundled media engine is missing; processed copies will be unavailable",
			     kLogPrefix);
		else
			blog(LOG_INFO, "%s Bundled media engine ready", kLogPrefix);
		exportManager = std::make_unique<clipcoach::ExportManager>(
			std::make_unique<clipcoach::exporting::FfmpegExportBackend>(ffmpegPath),
			[](const clipcoach::ExportJob &job) {
				if (libraryService != nullptr) {
					libraryService->storeExportJob(
						job, [terminal = job.state == clipcoach::ExportJobState::Done](
							     clipcoach::storage::StorageStatus status) {
							if (!terminal || !status.success)
								return;
							QPointer<clipcoach::ui::MainDock> dock = mainDockContent;
							if (!dock.isNull())
								QMetaObject::invokeMethod(
									dock, [dock] {
										if (!dock.isNull())
											dock->refreshClipLibrary();
									}, Qt::QueuedConnection);
						});
				}
			},
			[](bool error, const std::string &message) {
				blog(error ? LOG_ERROR : LOG_INFO, "%s %s", kLogPrefix, message.c_str());
			},
			featureGateService.get());
		clipManager->setCaptureContext(currentSessionId, CLIPCOACH_VERSION, currentSceneName);
		clipManager->setClipPersistenceCallback([](const clipcoach::ClipMetadata &clip) {
			if (libraryService != nullptr) {
				libraryService->storeClip(clip);
			}
			queueClipTelemetry(clip);
			queueThumbnailGeneration(clip);
			queueAiAnalysis(clip);
		});
		if (!initializeRemoteClipper())
			blog(LOG_WARNING, "%s Remote Clipper integration is unavailable", kLogPrefix);
		if (settingsManager != nullptr) {
			const auto &settings = settingsManager->settings();
			ensureObsReplayCapacity(settings.preRollSeconds +
						settings.postRollSeconds);
		}
		obs_frontend_add_event_callback(frontendEventCallback, nullptr);
		lifecycleCallbackRegistered = true;
		if (!registerMainDock()) {
			shutdownRemoteClipper();
			obs_frontend_remove_event_callback(frontendEventCallback, nullptr);
			lifecycleCallbackRegistered = false;
			clipManager.reset();
			replayManager.reset();
			verticalCanvasManager.reset();
			exportManager.reset();
			voiceTriggerController.reset();
			triggerEngine.reset();
			shutdownLicensing();
			shutdownLibrarySafely();
			settingsManager.reset();
			featureGateService.reset();
			return false;
		}
		if (!initializeHotkeys()) {
			shutdownRemoteClipper();
			removeDockSafely();
			obs_frontend_remove_event_callback(frontendEventCallback, nullptr);
			lifecycleCallbackRegistered = false;
			clipManager.reset();
			replayManager.reset();
			verticalCanvasManager.reset();
			exportManager.reset();
			voiceTriggerController.reset();
			triggerEngine.reset();
			shutdownLicensing();
			shutdownLibrarySafely();
			settingsManager.reset();
			featureGateService.reset();
			return false;
		}

		blog(LOG_INFO, "%s Plugin loaded successfully", kLogPrefix);
		return true;
	} catch (const std::exception &error) {
		shutdownHotkeys();
		shutdownRemoteClipper();
		removeDockSafely();
		if (lifecycleCallbackRegistered) {
			obs_frontend_remove_event_callback(frontendEventCallback, nullptr);
			lifecycleCallbackRegistered = false;
		}
		clipManager.reset();
		replayManager.reset();
		verticalCanvasManager.reset();
		exportManager.reset();
		voiceTriggerController.reset();
		triggerEngine.reset();
		shutdownLicensing();
		shutdownLibrarySafely();
		settingsManager.reset();
		featureGateService.reset();
		blog(LOG_ERROR, "%s Plugin load failed: %s", kLogPrefix, error.what());
		return false;
	} catch (...) {
		shutdownHotkeys();
		shutdownRemoteClipper();
		removeDockSafely();
		if (lifecycleCallbackRegistered) {
			obs_frontend_remove_event_callback(frontendEventCallback, nullptr);
			lifecycleCallbackRegistered = false;
		}
		clipManager.reset();
		replayManager.reset();
		verticalCanvasManager.reset();
		exportManager.reset();
		voiceTriggerController.reset();
		triggerEngine.reset();
		shutdownLicensing();
		shutdownLibrarySafely();
		settingsManager.reset();
		featureGateService.reset();
		blog(LOG_ERROR, "%s Plugin load failed with an unknown error", kLogPrefix);
		return false;
	}
}

MODULE_EXPORT void obs_module_unload(void)
{
	shutdownHotkeys();
	shutdownRemoteClipper();
	removeDockSafely();
	if (lifecycleCallbackRegistered) {
		obs_frontend_remove_event_callback(frontendEventCallback, nullptr);
		lifecycleCallbackRegistered = false;
	}
	shutdownLicensing();
	clipTelemetryNetwork.reset();
	clipManager.reset();
	replayManager.reset();
	verticalCanvasManager.reset();
	exportManager.reset();
	voiceTriggerController.reset();
	triggerEngine.reset();
	shutdownLibrarySafely();
	settingsManager.reset();
	featureGateService.reset();
	blog(LOG_INFO, "%s Plugin unloaded safely", kLogPrefix);
}
