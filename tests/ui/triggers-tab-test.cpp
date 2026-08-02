#include "../unit/test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/trigger-engine.hpp>
#include <clipcoach/core/voice-trigger-controller.hpp>
#include <clipcoach/integrations/chat-pulse-service.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/triggers-tab.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>

#include <chrono>
#include <filesystem>
#include <string_view>

namespace {

class WaitingVoiceController final : public clipcoach::VoiceTriggerController {
public:
	WaitingVoiceController()
	{
		status_.state = clipcoach::VoiceRuntimeState::WaitingForAudio;
		status_.modelLoaded = true;
		status_.sourceName = "Shure Mic";
	}

	void applySettings(const clipcoach::Settings &) override {}
	void setProUnlocked(bool) override {}
	clipcoach::VoiceRuntimeStatus status() const override { return status_; }
	void setStatusCallback(StatusCallback callback) override
	{
		callback_ = std::move(callback);
	}
	std::vector<std::string> availableAudioSources() const override
	{
		return {"Shure Mic"};
	}

private:
	clipcoach::VoiceRuntimeStatus status_;
	StatusCallback callback_;
};

} // namespace

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);
	const auto directory =
		std::filesystem::temp_directory_path() /
		("clipcoach-trigger-ui-" +
		 std::to_string(std::chrono::steady_clock::now()
					.time_since_epoch()
					.count()));
	std::filesystem::create_directories(directory);
	clipcoach::SettingsManager manager(directory / "settings.json");
	std::string error;
	clipcoach::test::expect(manager.load(&error),
			       "trigger UI settings must initialize");
	clipcoach::TriggerEngine engine(false);
	clipcoach::ui::TriggersTab tab(
		[](const char *key) { return QString::fromUtf8(key); }, &engine,
		&manager, nullptr, nullptr,
		[] { return std::vector<std::string>{"Gameplay", "Final"}; });

	const auto toggle = [&tab](const char *name) {
		return tab.findChild<QCheckBox *>(QString::fromUtf8(name));
	};
	clipcoach::test::expect(toggle("triggerManualToggle") != nullptr &&
				       toggle("triggerManualToggle")->isEnabled(),
			       "manual trigger must be configurable in Free");
	const auto signalMargins =
		toggle("triggerManualToggle")->parentWidget()->layout()->contentsMargins();
	clipcoach::test::expect(
		signalMargins.left() ==
				clipcoach::ui::tokens::kCardPaddingHorizontal &&
			signalMargins.right() ==
				clipcoach::ui::tokens::kCardPaddingHorizontal,
		"trigger signal text and toggle must preserve horizontal card padding");
	for (const auto name :
	     {"triggerVoiceToggle", "triggerAudioSpikeToggle",
	      "triggerChatPulseToggle", "triggerSceneToggle",
	      "triggerKeywordToggle", "triggerAiHookToggle"}) {
		clipcoach::test::expect(toggle(name) != nullptr &&
					       !toggle(name)->isEnabled(),
				       "Pro trigger must be visible and gated");
	}
	clipcoach::test::expect(
		tab.findChild<QSpinBox *>(
			QStringLiteral("triggerSensitivity")) != nullptr &&
			tab.findChild<QComboBox *>(
				QStringLiteral("triggerAction")) != nullptr &&
			tab.findChild<QLineEdit *>(
				QStringLiteral("triggerKeywords")) != nullptr,
		"trigger configuration controls must exist");
	auto *actionSelector =
		tab.findChild<QComboBox *>(QStringLiteral("triggerAction"));
	clipcoach::test::expect(
		actionSelector->findData(static_cast<int>(
			clipcoach::TriggerAction::SaveVerticalClip)) >= 0 &&
			actionSelector->findData(static_cast<int>(
				clipcoach::TriggerAction::SaveBoth)) >= 0 &&
			actionSelector->findData(static_cast<int>(
				clipcoach::TriggerAction::AddToRecommended)) >= 0,
		"trigger actions must expose vertical, both and recommended workflows");
	auto *sceneSelector =
		tab.findChild<QListWidget *>(QStringLiteral("triggerScenes"));
	clipcoach::test::expect(
		sceneSelector != nullptr &&
			sceneSelector->count() == 2 &&
			tab.findChild<QLabel *>(
				QStringLiteral("triggerTimingSummary")) != nullptr,
		"scene configuration must list live OBS options and explain timing");
	clipcoach::test::expect(
		tab.findChild<QLabel *>(QStringLiteral("voiceRuntimeStatus")) != nullptr &&
			tab.findChild<QToolButton *>(
				QStringLiteral("triggerVoiceSectionInfoButton")) != nullptr &&
			tab.findChild<QComboBox *>(QStringLiteral("voiceAudioSource")) != nullptr,
		"Voice Trigger must explain its phrases, expose OBS audio source selection and recognizer status");
	auto *speechMode =
		tab.findChild<QComboBox *>(QStringLiteral("voiceSpeechMode"));
	clipcoach::test::expect(
		speechMode != nullptr && speechMode->count() == 1 &&
			speechMode->currentData().toInt() ==
				static_cast<int>(clipcoach::SpeechProcessingMode::Local) &&
			tab.findChild<QCheckBox *>(
				QStringLiteral("voiceCloudConsent")) == nullptr,
		"Voice Trigger must expose only the functional local OBS-audio pipeline");
	for (const auto *name :
	     {"triggerManualToggleInfoButton", "triggerPreRollInfoButton",
	      "triggerVoiceSectionInfoButton"}) {
		auto *help =
			tab.findChild<QToolButton *>(QString::fromLatin1(name));
		clipcoach::test::expect(
			help != nullptr && !help->toolTip().isEmpty(),
			"Trigger help must be available from compact information buttons");
	}
	clipcoach::test::expect(
		tab.findChild<QWidget *>(QStringLiteral("triggerSignalsSection")) != nullptr &&
			tab.findChild<QWidget *>(QStringLiteral("triggerConfigurationSection")) != nullptr &&
			tab.findChild<QWidget *>(QStringLiteral("triggerScenesSection")) != nullptr &&
			tab.findChild<QWidget *>(QStringLiteral("triggerVoiceSection")) != nullptr,
		"Triggers must expose the signal, clip timing, scene and voice groups");
	tab.resize(1200, 900);
	tab.show();
	application.processEvents();
	auto *leftColumn =
		tab.findChild<QWidget *>(QStringLiteral("triggersLeftColumn"));
	auto *rightColumn =
		tab.findChild<QWidget *>(QStringLiteral("triggersRightColumn"));
	clipcoach::test::expect(
		leftColumn != nullptr && rightColumn != nullptr &&
			leftColumn->geometry().x() < rightColumn->geometry().x(),
		"wide Triggers layout must use two scannable columns");
	auto *recentEventsSection = tab.findChild<QWidget *>(
		QStringLiteral("triggerRecentEventsSection"));
	clipcoach::test::expect(
		recentEventsSection != nullptr &&
			recentEventsSection->width() > leftColumn->width() * 3 / 2 &&
			recentEventsSection->geometry().x() == leftColumn->geometry().x() &&
			recentEventsSection->minimumHeight() >= 112,
		"recent events must use the complete content width with aligned left padding");
	tab.resize(720, 900);
	application.processEvents();
	clipcoach::test::expect(
		leftColumn->geometry().y() < rightColumn->geometry().y(),
		"narrow Triggers layout must stack sections instead of clipping controls");

	clipcoach::TriggerSignal signal;
	signal.type = clipcoach::SmartTriggerType::Manual;
	signal.manualMarker = true;
	clipcoach::test::expect(engine.process(signal).event.has_value(),
			       "manual trigger must generate an event");
	tab.refreshRecentEvents();
	clipcoach::test::expect(
		tab.findChildren<QLabel *>(
			   QStringLiteral("triggerEventItem"))
				.size() == 1,
		"recent trigger event must be rendered");
	auto *eventItem = tab.findChild<QLabel *>(
		QStringLiteral("triggerEventItem"));
	clipcoach::test::expect(
		eventItem != nullptr && eventItem->wordWrap() &&
			eventItem->minimumHeight() >= 38 &&
			eventItem->margin() == clipcoach::ui::tokens::kSpaceMd,
		"recent event text must be complete, padded and visually separated");

	auto *freePhraseList = tab.findChild<QListWidget *>(
		QStringLiteral("voicePhraseList"));
	clipcoach::test::expect(
		freePhraseList != nullptr && freePhraseList->count() == 10 &&
			!freePhraseList->isEnabled(),
		"Free must see default Voice phrases in a locked Pro control");
	engine.setProUnlocked(true);
	tab.refreshLicenseState();
	clipcoach::test::expect(
		toggle("triggerVoiceToggle")->isEnabled() && freePhraseList->isEnabled() &&
			tab.findChild<QLineEdit *>(QStringLiteral("voicePhraseInput"))->isEnabled(),
		"an in-session Free-to-Pro activation must immediately unlock trigger and Voice controls");

	clipcoach::TriggerEngine proEngine(true);
	WaitingVoiceController waitingVoice;
	clipcoach::ui::TriggersTab proTab(
		[](const char *key) {
			return std::string_view(key) == "Trigger.Event.Format"
				       ? QStringLiteral("%1 · score %2 · %3")
				       : QString::fromUtf8(key);
		},
		&proEngine, &manager, nullptr, &waitingVoice,
		[] { return std::vector<std::string>{"Gameplay", "Final"}; });
	int requiredReplaySeconds = -1;
	proTab.setTimingChangedCallback(
		[&requiredReplaySeconds](int seconds) {
			requiredReplaySeconds = seconds;
		});
	auto *triggerPreRoll = proTab.findChild<QSpinBox *>(
		QStringLiteral("triggerPreRoll"));
	auto *triggerPostRoll = proTab.findChild<QSpinBox *>(
		QStringLiteral("triggerPostRoll"));
	triggerPostRoll->setValue(28);
	triggerPreRoll->setValue(60);
	triggerPostRoll->setValue(29);
	QMetaObject::invokeMethod(triggerPostRoll, "editingFinished",
				  Qt::DirectConnection);
	clipcoach::test::expect(
		requiredReplaySeconds == 89 &&
			manager.settings().preRollSeconds == 60 &&
			manager.settings().postRollSeconds == 29,
		"Trigger timing changes must persist and request matching OBS Replay Buffer capacity");
	auto *phraseInput = proTab.findChild<QLineEdit *>(
		QStringLiteral("voicePhraseInput"));
	auto *addPhrase = proTab.findChild<QPushButton *>(
		QStringLiteral("addVoicePhraseButton"));
	auto *removePhrase = proTab.findChild<QPushButton *>(
		QStringLiteral("removeVoicePhraseButton"));
	auto *phraseList = proTab.findChild<QListWidget *>(
		QStringLiteral("voicePhraseList"));
	clipcoach::test::expect(
		phraseInput != nullptr && addPhrase != nullptr &&
			removePhrase != nullptr &&
			phraseList != nullptr && phraseList->isEnabled(),
		"Pro must expose editable Voice phrase controls");
	proTab.resize(1200, 1000);
	proTab.show();
	application.processEvents();
	auto *voiceSection = proTab.findChild<QWidget *>(
		QStringLiteral("triggerVoiceSection"));
	auto *voicePhraseEditor = proTab.findChild<QWidget *>(
		QStringLiteral("voicePhraseEditor"));
	auto *voiceRecognitionControls = proTab.findChild<QWidget *>(
		QStringLiteral("voiceRecognitionControls"));
	auto *voiceStatus = proTab.findChild<QLabel *>(
		QStringLiteral("voiceRuntimeStatus"));
	auto *proLeftColumn = proTab.findChild<QWidget *>(
		QStringLiteral("triggersLeftColumn"));
	clipcoach::test::expect(
		voiceSection != nullptr && proLeftColumn != nullptr &&
			voiceSection->width() > proLeftColumn->width() * 3 / 2,
		"Voice phrases must use the full Triggers content width");
	clipcoach::test::expect(
		voicePhraseEditor != nullptr &&
			voiceRecognitionControls != nullptr &&
			voicePhraseEditor->geometry().x() <
				voiceRecognitionControls->geometry().x(),
		"wide Voice phrases layout must separate phrase editing and recognition settings");
	clipcoach::test::expect(
		phraseInput->minimumHeight() ==
				clipcoach::ui::tokens::kLargeControlHeight &&
			addPhrase->minimumHeight() ==
				clipcoach::ui::tokens::kLargeControlHeight &&
			removePhrase->minimumHeight() ==
				clipcoach::ui::tokens::kLargeControlHeight,
		"Voice phrase input and actions must use a consistent control height");
	clipcoach::test::expect(
		qAbs(addPhrase->width() - removePhrase->width()) <= 1,
		"Voice phrase actions must use equal widths");
	clipcoach::test::expect(
		voiceStatus != nullptr &&
			voiceStatus->margin() == clipcoach::ui::tokens::kSpaceMd,
		"Voice listening status must preserve readable inner padding");
	clipcoach::test::expect(
		voiceStatus->text().contains(QStringLiteral("Voice.Status.WaitingForAudio")) &&
			voiceStatus->text().contains(QStringLiteral("Shure Mic")) &&
			voiceStatus->property("notificationTone").toString() ==
				QStringLiteral("info"),
		"a loaded model without microphone samples must report waiting for audio, not listening");
	proTab.resize(760, 1000);
	application.processEvents();
	clipcoach::test::expect(
		voicePhraseEditor->geometry().y() <
			voiceRecognitionControls->geometry().y(),
		"narrow Voice phrases layout must stack without clipping controls");
	const auto previousCount = phraseList->count();
	phraseInput->setText(QStringLiteral("clip this now"));
	addPhrase->click();
	clipcoach::test::expect(
		phraseList->count() == previousCount + 1 &&
			manager.settings().voiceTriggerPhrasesCsv.find(
				"clip this now") != std::string::npos,
		"adding a Voice phrase must update UI and persistent settings");
	phraseList->setCurrentRow(phraseList->count() - 1);
	removePhrase->click();
	clipcoach::test::expect(
		phraseList->count() == previousCount &&
			manager.settings().voiceTriggerPhrasesCsv.find(
				"clip this now") == std::string::npos,
		"removing a Voice phrase must update UI and persistent settings");
	auto *proSceneSelector =
		proTab.findChild<QListWidget *>(QStringLiteral("triggerScenes"));
	proSceneSelector->item(0)->setCheckState(Qt::Checked);
	proSceneSelector->item(1)->setCheckState(Qt::Checked);
	clipcoach::test::expect(
		manager.settings().triggerScenesCsv == "Gameplay,Final",
		"Scene Trigger must persist every checked OBS scene");

	clipcoach::integrations::ChatPulseService chatPulse(proEngine, true);
	clipcoach::integrations::ChatPulseConfiguration chatConfig;
	chatConfig.enabled = true;
	clipcoach::test::expect(
		chatPulse.configure(chatConfig),
		"Pro Chat Pulse must configure for recent-event UI");
	clipcoach::integrations::ChatMessage chatMessage;
	chatMessage.id = "chat-1";
	chatMessage.authorId = "viewer";
	chatMessage.text = "!clip";
	clipcoach::test::expect(
		chatPulse
			.process(clipcoach::integrations::ChatPlatform::Twitch,
				 chatMessage)
			.triggerEvent.has_value(),
		"simulated chat pulse must create a trigger event");
	proTab.refreshRecentEvents();
	const auto chatEvents = proTab.findChildren<QLabel *>(
		QStringLiteral("triggerEventItem"));
	clipcoach::test::expect(
		!chatEvents.empty() &&
			chatEvents.front()->text().contains(
				QStringLiteral("chat_pulse")),
		"Chat Pulse must appear in Triggers recent events");

	std::error_code removeError;
	std::filesystem::remove_all(directory, removeError);
	return clipcoach::test::pass("triggers-tab-test");
}
