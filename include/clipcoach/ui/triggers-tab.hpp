#pragma once

#include <QWidget>

#include <functional>
#include <string>
#include <vector>

class QCheckBox;
class QBoxLayout;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QResizeEvent;

namespace clipcoach {
class SettingsManager;
class TriggerEngine;
class VoiceTriggerController;
struct VoiceRuntimeStatus;
} // namespace clipcoach

namespace clipcoach::ui {

class TriggersTab final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using SceneProvider = std::function<std::vector<std::string>()>;
	using TimingChangedCallback = std::function<void(int)>;

	TriggersTab(TranslationFunction translator, TriggerEngine *engine, SettingsManager *settingsManager,
		    QWidget *parent = nullptr, VoiceTriggerController *voiceController = nullptr,
		    SceneProvider sceneProvider = {});

	void refreshRecentEvents();
	void refreshLicenseState();
	void refreshConfiguration();
	void setTimingChangedCallback(TimingChangedCallback callback);

private:
	void resizeEvent(QResizeEvent *event) override;
	[[nodiscard]] QString text(const char *key) const;
	QCheckBox *addTriggerToggle(QVBoxLayout *layout, const QString &title, const QString &description,
				    const QString &objectName, bool checked, bool requiresPro);
	void load();
	void applyAndSave();
	void applyEngineConfiguration();
	void updateVoiceStatus(const VoiceRuntimeStatus &status);
	void refreshSceneOptions();
	void updateTimingSummary();

	TranslationFunction translator_;
	TriggerEngine *engine_{nullptr};
	SettingsManager *settingsManager_{nullptr};
	VoiceTriggerController *voiceController_{nullptr};
	SceneProvider sceneProvider_;
	TimingChangedCallback timingChangedCallback_;
	QCheckBox *manual_{nullptr};
	QCheckBox *voice_{nullptr};
	QCheckBox *audioSpike_{nullptr};
	QCheckBox *chatPulse_{nullptr};
	QCheckBox *scene_{nullptr};
	QCheckBox *keyword_{nullptr};
	QCheckBox *aiHook_{nullptr};
	QSpinBox *preRoll_{nullptr};
	QSpinBox *postRoll_{nullptr};
	QSpinBox *sensitivity_{nullptr};
	QSpinBox *cooldown_{nullptr};
	QComboBox *action_{nullptr};
	QLineEdit *keywords_{nullptr};
	QListWidget *scenes_{nullptr};
	QListWidget *voicePhrases_{nullptr};
	QLineEdit *voicePhraseInput_{nullptr};
	QPushButton *addVoicePhrase_{nullptr};
	QPushButton *removeVoicePhrase_{nullptr};
	QComboBox *voiceLanguage_{nullptr};
	QComboBox *voiceAudioSource_{nullptr};
	QComboBox *voiceSpeechMode_{nullptr};
	QVBoxLayout *eventsLayout_{nullptr};
	QLabel *saveError_{nullptr};
	QLabel *voiceStatus_{nullptr};
	QLabel *timingSummary_{nullptr};
	QWidget *upgradeBanner_{nullptr};
	QBoxLayout *mainColumns_{nullptr};
	QBoxLayout *voiceBody_{nullptr};
};

} // namespace clipcoach::ui
