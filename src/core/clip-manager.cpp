#include <clipcoach/core/clip-manager.hpp>
#include <clipcoach/core/score-engine.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace clipcoach {
namespace {

std::tm utcTime(std::time_t value)
{
	std::tm result{};
#ifdef _WIN32
	gmtime_s(&result, &value);
#else
	gmtime_r(&value, &result);
#endif
	return result;
}

std::string sanitizedTrigger(const std::string &trigger)
{
	std::string result;
	result.reserve(trigger.size());
	for (const auto character : trigger) {
		const auto byte = static_cast<unsigned char>(character);
		if (std::isalnum(byte) != 0 || character == '-' || character == '_') {
			result.push_back(static_cast<char>(std::tolower(byte)));
		}
	}
	return result.empty() ? "clip" : result;
}

std::filesystem::path availableDestination(const std::filesystem::path &source, const std::string &fileName)
{
	auto candidate = source.parent_path() / fileName;
	if (candidate == source) {
		return candidate;
	}
	std::error_code error;
	if (!std::filesystem::exists(candidate, error)) {
		return candidate;
	}

	const auto stem = candidate.stem().string();
	const auto extension = candidate.extension().string();
	for (int suffix = 2; suffix <= 9999; ++suffix) {
		candidate = source.parent_path() / (stem + "_" + std::to_string(suffix) + extension);
		error.clear();
		if (!std::filesystem::exists(candidate, error)) {
			return candidate;
		}
	}
	return {};
}

CaptureResult rejected(CaptureError error, std::string message)
{
	return {false, error, std::move(message)};
}

} // namespace

ClipManager::ClipManager(ReplayManager &replayManager, Clock clock, const FeatureGateService *featureGates)
	: replayManager_(replayManager),
	  featureGates_(featureGates),
	  clock_(std::move(clock))
{
	replayManager_.setReplaySavedCallback([this](const std::filesystem::path &path) { handleReplaySaved(path); });
	replayManager_.setStateChangedCallback([this](ReplayState state) {
		if (replayStateCallback_) {
			replayStateCallback_(state);
		}
	});
}

ClipManager::~ClipManager()
{
	replayManager_.setReplaySavedCallback({});
	replayManager_.setStateChangedCallback({});
}

bool ClipManager::isValidDuration(int seconds) noexcept
{
	return seconds >= 5 && seconds <= 300;
}

std::string ClipManager::createReadableFileName(std::chrono::system_clock::time_point capturedAt,
						const std::string &trigger, const std::string &extension)
{
	const auto time = std::chrono::system_clock::to_time_t(capturedAt);
	const auto utc = utcTime(time);
	std::ostringstream name;
	name << "ClipX_" << std::put_time(&utc, "%Y-%m-%d_%H-%M-%S") << '_' << sanitizedTrigger(trigger);
	if (!extension.empty()) {
		name << (extension.front() == '.' ? "" : ".") << extension;
	}
	return name.str();
}

ReplayState ClipManager::replayState() const noexcept
{
	return replayManager_.state();
}

CaptureResult ClipManager::startReplayBuffer()
{
	if (replayManager_.state() == ReplayState::Active || replayManager_.state() == ReplayState::Starting) {
		return {true, CaptureError::None, {}};
	}
	const auto result = replayManager_.start();
	if (!result.accepted) {
		reportError(CaptureError::SaveRejected, result.errorMessage);
		return rejected(CaptureError::SaveRejected, result.errorMessage);
	}
	return {true, CaptureError::None, {}};
}

CaptureResult ClipManager::stopReplayBuffer()
{
	if (replayManager_.state() == ReplayState::Inactive) {
		return {true, CaptureError::None, {}};
	}
	const auto result = replayManager_.stop();
	if (!result.accepted) {
		reportError(CaptureError::SaveRejected, result.errorMessage);
		return rejected(CaptureError::SaveRejected, result.errorMessage);
	}
	return {true, CaptureError::None, {}};
}

CaptureResult ClipManager::captureManual(int durationSeconds)
{
	TriggerSignal signal;
	signal.type = SmartTriggerType::Manual;
	signal.manualMarker = true;
	signal.durationSeconds = durationSeconds;
	return beginCapture(durationSeconds, TriggerType::Manual, "manual", ScoreEngine{}.calculate(signal));
}

CaptureResult ClipManager::captureTriggered(int durationSeconds, TriggerType triggerType, std::string triggerLabel,
					    int score, std::string requestedBy)
{
	if (triggerLabel.empty()) {
		switch (triggerType) {
		case TriggerType::Voice:
			triggerLabel = "voice";
			break;
		case TriggerType::AudioSpike:
			triggerLabel = "audio-spike";
			break;
		case TriggerType::Chat:
			triggerLabel = "chat";
			break;
		case TriggerType::Scene:
			triggerLabel = "scene";
			break;
		case TriggerType::Ai:
			triggerLabel = "ai";
			break;
		case TriggerType::Manual:
			triggerLabel = "manual";
			break;
		}
	}
	return beginCapture(durationSeconds, triggerType, std::move(triggerLabel), std::clamp(score, 0, 100),
			    std::move(requestedBy));
}

CaptureResult ClipManager::markMoment(std::string label, int secondsBeforeNow)
{
	if (label.empty())
		label = "manual";
	secondsBeforeNow = std::clamp(secondsBeforeNow, 0, 120);
	markedMoments_.push_back({clock_() - std::chrono::seconds(secondsBeforeNow), std::move(label)});
	return {true, CaptureError::None, "moment marked"};
}

CaptureResult ClipManager::beginCapture(int durationSeconds, TriggerType triggerType, std::string triggerLabel,
					int score, std::string requestedBy)
{
	if (!isValidDuration(durationSeconds)) {
		const std::string message = "clip duration must be between 5 and 300 seconds";
		reportError(CaptureError::InvalidDuration, message);
		return rejected(CaptureError::InvalidDuration, message);
	}
	if (featureGates_ != nullptr && !PlanPolicy::isFreeQuickDuration(durationSeconds)) {
		const auto decision = featureGates_->check(Feature::UnlimitedDurations);
		if (!decision.allowed) {
			reportError(CaptureError::ProRequired, decision.message);
			return rejected(CaptureError::ProRequired, decision.message);
		}
	}
	if (replayManager_.state() != ReplayState::Active) {
		const std::string message = "Replay Buffer is not active";
		reportError(CaptureError::ReplayBufferInactive, message);
		return rejected(CaptureError::ReplayBufferInactive, message);
	}
	const auto bufferedSeconds = replayManager_.bufferedDurationSeconds();
	if (bufferedSeconds < durationSeconds - 2) {
		const std::string message = "Replay Buffer contains only " + std::to_string(bufferedSeconds) + " of " +
					    std::to_string(durationSeconds) + " requested seconds";
		reportError(CaptureError::ReplayBufferWarmingUp, message);
		return rejected(CaptureError::ReplayBufferWarmingUp, message);
	}
	if (pendingCapture_.has_value()) {
		const std::string message = "a replay save is already pending";
		reportError(CaptureError::CaptureAlreadyPending, message);
		return rejected(CaptureError::CaptureAlreadyPending, message);
	}

	pendingCapture_ = PendingCapture{durationSeconds,        triggerType, std::move(triggerLabel),
					 std::move(requestedBy), score,       clock_()};
	const auto result = replayManager_.save();
	if (!result.accepted) {
		pendingCapture_.reset();
		reportError(CaptureError::SaveRejected, result.errorMessage);
		return rejected(CaptureError::SaveRejected, result.errorMessage);
	}

	return {true, CaptureError::None, {}};
}

bool ClipManager::capturePending() const noexcept
{
	return pendingCapture_.has_value();
}

const std::vector<ClipMetadata> &ClipManager::sessionClips() const noexcept
{
	return sessionClips_;
}

const std::vector<MomentMarker> &ClipManager::markedMoments() const noexcept
{
	return markedMoments_;
}

void ClipManager::setClipSavedCallback(ClipSavedCallback callback)
{
	clipSavedCallback_ = std::move(callback);
}

void ClipManager::setErrorCallback(ErrorCallback callback)
{
	errorCallback_ = std::move(callback);
}

void ClipManager::setReplayStateCallback(ReplayStateCallback callback)
{
	replayStateCallback_ = std::move(callback);
}

void ClipManager::setClipPersistenceCallback(ClipPersistenceCallback callback)
{
	clipPersistenceCallback_ = std::move(callback);
}

void ClipManager::setCaptureContext(std::string sessionId, std::string appVersion,
				    SourceSceneProvider sourceSceneProvider)
{
	if (!sessionId.empty()) {
		sessionId_ = std::move(sessionId);
	}
	appVersion_ = std::move(appVersion);
	sourceSceneProvider_ = std::move(sourceSceneProvider);
}

ClipManager::ObserverId ClipManager::addClipSavedObserver(ClipSavedCallback callback)
{
	const auto id = nextObserverId_++;
	clipSavedObservers_.emplace(id, std::move(callback));
	return id;
}

ClipManager::ObserverId ClipManager::addErrorObserver(ErrorCallback callback)
{
	const auto id = nextObserverId_++;
	errorObservers_.emplace(id, std::move(callback));
	return id;
}

void ClipManager::removeClipSavedObserver(ObserverId id)
{
	clipSavedObservers_.erase(id);
}
void ClipManager::removeErrorObserver(ObserverId id)
{
	errorObservers_.erase(id);
}

void ClipManager::handleReplaySaved(const std::filesystem::path &sourcePath)
{
	if (!pendingCapture_) {
		return;
	}

	const auto pending = *pendingCapture_;
	pendingCapture_.reset();

	std::error_code error;
	if (sourcePath.empty() || !std::filesystem::is_regular_file(sourcePath, error)) {
		reportError(CaptureError::SavedFileUnavailable,
			    "OBS reported a replay but the saved file is unavailable");
		return;
	}

	const auto readableName =
		createReadableFileName(pending.capturedAt, pending.triggerLabel, sourcePath.extension().string());
	const auto destination = availableDestination(sourcePath, readableName);
	if (destination.empty()) {
		reportError(CaptureError::FileRenameFailed, "could not allocate a unique clip file name");
		return;
	}

	auto finalPath = sourcePath;
	bool renameFailed = false;
	if (destination != sourcePath) {
		std::filesystem::rename(sourcePath, destination, error);
		if (error) {
			renameFailed = true;
		} else {
			finalPath = destination;
		}
	}

	ClipMetadata metadata;
	metadata.id = sessionId_ + "-clip-" + std::to_string(sessionClips_.size() + 1);
	metadata.sessionId = sessionId_;
	metadata.filePath = finalPath;
	metadata.fileName = finalPath.filename().string();
	metadata.createdAt = pending.capturedAt;
	metadata.durationSeconds = pending.durationSeconds;
	metadata.orientation = ClipOrientation::Horizontal;
	metadata.triggerType = pending.triggerType;
	metadata.triggerLabel = pending.triggerLabel;
	metadata.requestedBy = pending.requestedBy;
	metadata.score = pending.score;
	metadata.title = finalPath.stem().string();
	// The source replay is already a complete, playable file at this point.
	// Pending is reserved for processed/exported copies that are still running.
	metadata.exportStatus = ExportStatus::Exported;
	metadata.sourceScene = sourceSceneProvider_ ? sourceSceneProvider_() : "";
	metadata.appVersion = appVersion_;
	sessionClips_.push_back(std::move(metadata));

	if (clipPersistenceCallback_) {
		clipPersistenceCallback_(sessionClips_.back());
	}
	if (clipSavedCallback_) {
		clipSavedCallback_(sessionClips_.back());
	}
	const auto savedObservers = clipSavedObservers_;
	for (const auto &[id, observer] : savedObservers) {
		(void)id;
		if (observer)
			observer(sessionClips_.back());
	}
	if (renameFailed) {
		reportError(CaptureError::FileRenameFailed, "the replay was saved but could not be renamed");
	}
}

void ClipManager::reportError(CaptureError error, std::string message)
{
	if (errorCallback_) {
		errorCallback_(error, message);
	}
	const auto errorObservers = errorObservers_;
	for (const auto &[id, observer] : errorObservers) {
		(void)id;
		if (observer)
			observer(error, message);
	}
}

} // namespace clipcoach
