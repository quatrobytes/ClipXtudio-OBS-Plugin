#include <clipcoach/core/trigger-engine.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>

namespace clipcoach {
namespace {

std::string lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value;
}

double strength(const TriggerSignal &signal) noexcept
{
	switch (signal.type) {
	case SmartTriggerType::Manual:
		return signal.manualMarker ? 1.0 : 0.0;
	case SmartTriggerType::Voice:
		return signal.voiceConfidence;
	case SmartTriggerType::AudioSpike:
		return signal.audioIntensity;
	case SmartTriggerType::ChatPulse:
		return signal.chatActivity;
	case SmartTriggerType::Scene:
		return signal.sceneRelevance;
	case SmartTriggerType::Keyword:
		return signal.keywordStrength;
	case SmartTriggerType::FutureAiHook:
		return signal.aiConfidence;
	}
	return 0.0;
}

bool containsCaseInsensitive(const std::vector<std::string> &values, const std::string &candidate)
{
	const auto expected = lower(candidate);
	return std::any_of(values.begin(), values.end(),
			   [&expected](const std::string &value) { return lower(value) == expected; });
}

} // namespace

const char *triggerTypeName(SmartTriggerType type) noexcept
{
	switch (type) {
	case SmartTriggerType::Manual:
		return "manual";
	case SmartTriggerType::Voice:
		return "voice";
	case SmartTriggerType::AudioSpike:
		return "audio_spike";
	case SmartTriggerType::ChatPulse:
		return "chat_pulse";
	case SmartTriggerType::Scene:
		return "scene";
	case SmartTriggerType::Keyword:
		return "keyword";
	case SmartTriggerType::FutureAiHook:
		return "future_ai_hook";
	}
	return "unknown";
}

const char *triggerActionName(TriggerAction action) noexcept
{
	switch (action) {
	case TriggerAction::SaveClip:
		return "save_clip";
	case TriggerAction::MarkMoment:
		return "mark_moment";
	case TriggerAction::AddToRecommended:
		return "add_to_recommended";
	case TriggerAction::SaveVerticalClip:
		return "save_vertical_clip";
	case TriggerAction::SaveBoth:
		return "save_both";
	}
	return "unknown";
}

TriggerEngine::TriggerEngine(bool proUnlocked) : proUnlocked_(proUnlocked)
{
	for (const auto type : {SmartTriggerType::Manual, SmartTriggerType::Voice, SmartTriggerType::AudioSpike,
				SmartTriggerType::ChatPulse, SmartTriggerType::Scene, SmartTriggerType::Keyword,
				SmartTriggerType::FutureAiHook}) {
		TriggerConfiguration config;
		config.enabled = type == SmartTriggerType::Manual;
		config.action = type == SmartTriggerType::Manual ? TriggerAction::MarkMoment
								 : TriggerAction::AddToRecommended;
		configurations_.emplace(type, std::move(config));
	}
}

bool TriggerEngine::setConfiguration(SmartTriggerType type, const TriggerConfiguration &configuration,
				     std::string *error)
{
	if (configuration.preRollSeconds < 0 || configuration.postRollSeconds < 0 ||
	    configuration.preRollSeconds > 120 || configuration.postRollSeconds > 120 ||
	    configuration.sensitivity < 0 || configuration.sensitivity > 100 || configuration.cooldown.count() < 0 ||
	    configuration.duplicateWindow.count() < 0) {
		if (error)
			*error = "Invalid trigger timing or sensitivity";
		return false;
	}

	std::scoped_lock lock(mutex_);
	if (configuration.enabled && requiresPro(type) && !proUnlocked_) {
		if (error)
			*error = "This trigger requires ClipXtudio Pro";
		return false;
	}
	configurations_[type] = configuration;
	return true;
}

TriggerConfiguration TriggerEngine::configuration(SmartTriggerType type) const
{
	std::scoped_lock lock(mutex_);
	const auto iterator = configurations_.find(type);
	return iterator == configurations_.end() ? TriggerConfiguration{} : iterator->second;
}

void TriggerEngine::setProUnlocked(bool unlocked) noexcept
{
	std::scoped_lock lock(mutex_);
	proUnlocked_ = unlocked;
	if (!unlocked) {
		for (auto &[type, config] : configurations_) {
			if (requiresPro(type))
				config.enabled = false;
		}
	}
}

bool TriggerEngine::proUnlocked() const noexcept
{
	std::scoped_lock lock(mutex_);
	return proUnlocked_;
}

bool TriggerEngine::requiresPro(SmartTriggerType type) noexcept
{
	return type != SmartTriggerType::Manual;
}

TriggerResult TriggerEngine::process(const TriggerSignal &signal)
{
	return evaluateMoment({signal});
}

TriggerResult TriggerEngine::evaluateMoment(const std::vector<TriggerSignal> &inputSignals)
{
	TriggerResult result;
	EventCallback callback;
	{
		std::scoped_lock lock(mutex_);
		result = evaluateLocked(inputSignals);
		callback = eventCallback_;
	}
	if (result.event && callback) {
		try {
			callback(*result.event);
		} catch (...) {
			// External action handlers cannot compromise signal processing.
		}
	}
	return result;
}

bool TriggerEngine::isEligible(const TriggerSignal &signal, const TriggerConfiguration &config,
			       TriggerRejection &rejection) const
{
	if (!config.enabled) {
		rejection = TriggerRejection::Disabled;
		return false;
	}
	if (requiresPro(signal.type) && !proUnlocked_) {
		rejection = TriggerRejection::ProRequired;
		return false;
	}
	if (signal.durationSeconds < 0) {
		rejection = TriggerRejection::InvalidSignal;
		return false;
	}
	if (signal.type == SmartTriggerType::Keyword) {
		const auto configured = !signal.keyword.empty() &&
					containsCaseInsensitive(config.keywords, signal.keyword);
		const auto foundInText = std::any_of(
			config.keywords.begin(), config.keywords.end(), [&signal](const std::string &keyword) {
				return !keyword.empty() && lower(signal.text).find(lower(keyword)) != std::string::npos;
			});
		if (!configured && !foundInText) {
			rejection = TriggerRejection::NotConfigured;
			return false;
		}
	}
	if (signal.type == SmartTriggerType::Scene &&
	    (signal.scene.empty() || !containsCaseInsensitive(config.scenes, signal.scene))) {
		rejection = TriggerRejection::NotConfigured;
		return false;
	}
	if (std::clamp(strength(signal), 0.0, 1.0) < static_cast<double>(config.sensitivity) / 100.0) {
		rejection = TriggerRejection::BelowThreshold;
		return false;
	}
	return true;
}

TriggerResult TriggerEngine::evaluateLocked(const std::vector<TriggerSignal> &inputSignals)
{
	if (inputSignals.empty())
		return {{}, TriggerRejection::InvalidSignal};

	std::vector<TriggerSignal> eligible;
	TriggerRejection lastRejection = TriggerRejection::Disabled;
	for (const auto &signal : inputSignals) {
		const auto iterator = configurations_.find(signal.type);
		if (iterator == configurations_.end()) {
			lastRejection = TriggerRejection::NotConfigured;
			continue;
		}
		TriggerRejection rejection = TriggerRejection::None;
		if (isEligible(signal, iterator->second, rejection))
			eligible.push_back(signal);
		else
			lastRejection = rejection;
	}
	if (eligible.empty())
		return {{}, lastRejection};

	const auto primary = std::max_element(eligible.begin(), eligible.end(),
					      [](const TriggerSignal &left, const TriggerSignal &right) {
						      return strength(left) < strength(right);
					      });
	const auto &config = configurations_.at(primary->type);
	const auto occurredAt = primary->occurredAt;

	if (lastEventAt_) {
		const auto delta = occurredAt >= *lastEventAt_ ? occurredAt - *lastEventAt_
							       : *lastEventAt_ - occurredAt;
		if (delta <= config.duplicateWindow)
			return {{}, TriggerRejection::Duplicate};
	}
	const bool savesMedia = config.action == TriggerAction::SaveClip ||
				config.action == TriggerAction::SaveVerticalClip ||
				config.action == TriggerAction::SaveBoth;
	if (savesMedia && lastClipAt_ && occurredAt >= *lastClipAt_ && occurredAt - *lastClipAt_ < config.cooldown)
		return {{}, TriggerRejection::Cooldown};

	TriggerEvent event;
	event.id = "trigger-" + std::to_string(nextEventId_++);
	event.primaryType = primary->type;
	event.occurredAt = occurredAt;
	event.captureStart = occurredAt - std::chrono::seconds(config.preRollSeconds);
	event.captureEnd = occurredAt + std::chrono::seconds(config.postRollSeconds);
	event.action = config.action;
	event.score = scoreEngine_.calculate(eligible);
	event.keyword = primary->keyword;
	event.scene = primary->scene;
	std::set<SmartTriggerType> types;
	for (const auto &signal : eligible)
		types.insert(signal.type);
	event.contributingTypes.assign(types.begin(), types.end());

	lastEventAt_ = occurredAt;
	if (savesMedia)
		lastClipAt_ = occurredAt;
	recentEvents_.insert(recentEvents_.begin(), event);
	if (recentEvents_.size() > 100)
		recentEvents_.resize(100);
	return {event, TriggerRejection::None};
}

std::vector<TriggerEvent> TriggerEngine::recentEvents(std::size_t limit) const
{
	std::scoped_lock lock(mutex_);
	const auto count = std::min(limit, recentEvents_.size());
	return {recentEvents_.begin(), recentEvents_.begin() + static_cast<std::ptrdiff_t>(count)};
}

void TriggerEngine::clearRecentEvents()
{
	std::scoped_lock lock(mutex_);
	recentEvents_.clear();
}

void TriggerEngine::setEventCallback(EventCallback callback)
{
	std::scoped_lock lock(mutex_);
	eventCallback_ = std::move(callback);
}

} // namespace clipcoach
