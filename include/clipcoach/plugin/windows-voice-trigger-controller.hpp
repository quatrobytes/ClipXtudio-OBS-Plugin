#pragma once

#include <clipcoach/core/voice-trigger-controller.hpp>

#include <QObject>

#include <memory>

namespace clipcoach {
class TriggerEngine;
}

namespace clipcoach::plugin {

class WindowsVoiceTriggerController final : public QObject, public VoiceTriggerController {
public:
	explicit WindowsVoiceTriggerController(TriggerEngine &engine, QObject *parent = nullptr);
	~WindowsVoiceTriggerController() override;

	void applySettings(const Settings &settings) override;
	void setProUnlocked(bool unlocked) override;
	void shutdown() noexcept override;
	[[nodiscard]] VoiceRuntimeStatus status() const override;
	void setStatusCallback(StatusCallback callback) override;
	[[nodiscard]] std::vector<std::string> availableAudioSources() const override;

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace clipcoach::plugin
