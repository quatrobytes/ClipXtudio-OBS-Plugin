#pragma once

#include <clipcoach/ui/clip-library-controller.hpp>
#include <clipcoach/ui/caption-generator.hpp>

#include <QWidget>

#include <functional>
#include <memory>
#include <array>
#include <string>
#include <vector>

class QButtonGroup;
class QFrame;
class QGridLayout;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QTimer;
class QVBoxLayout;
class QComboBox;
class QProgressBar;

namespace clipcoach {
class ClipActionService;
class ExportManager;
class FeatureGateService;
class SettingsManager;
class VerticalCanvasManager;
namespace storage {
class ClipLibraryService;
}
} // namespace clipcoach

namespace clipcoach::ui {

class StatusCard;

class ClipsTab final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using CaptionBusyCallback = std::function<void(bool, const CaptionGenerationProgress &)>;
	using CaptionReadyCallback = std::function<void(const QString &, const QString &)>;
	using CaptionProgressReadyCallback = std::function<void(const CaptionGenerationProgress &)>;
	using CaptionFailedCallback = std::function<void(const QString &)>;

	ClipsTab(TranslationFunction translator, storage::ClipLibraryService *libraryService, QString sessionId,
		 QWidget *parent = nullptr, ClipActionService *actionService = nullptr,
		 ExportManager *exportManager = nullptr, SettingsManager *settingsManager = nullptr,
		 VerticalCanvasManager *verticalManager = nullptr, FeatureGateService *featureGates = nullptr,
		 CaptionGenerator captionGenerator = {});
	~ClipsTab() override;

	void reload();
	void refreshFeatureGates();
	void setCaptionBusyCallback(CaptionBusyCallback callback);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	[[nodiscard]] QString text(const char *key) const;
	void buildUi();
	void bindUi();
	void applyState(ClipLibraryUiState state);
	void startBatchedRender(std::vector<ClipMetadata> clips);
	void renderNextBatch(std::uint64_t generation);
	void addClipCard(const ClipMetadata &clip);
	void clearCards();
	void showActionResult(const ClipActionResult &result, const char *errorKey);
	void requestCaption(const ClipMetadata &clip, class ClipCard *card, CaptionReadyCallback ready = {},
			    CaptionProgressReadyCallback progressReady = {}, CaptionFailedCallback failed = {});
	void showCaptionDialog(const QString &caption, const QString &clipTitle,
			       const QString &youtubeShortsCaption = {});
	void confirmAndDelete(std::vector<std::string> clipIds, const QString &clipTitle = {});
	void updateSelectionActions();
	void refreshExportProgress();
	void updateResponsiveLayout();
	[[nodiscard]] QString triggerText(TriggerType trigger) const;

	TranslationFunction translator_;
	std::unique_ptr<ClipActionService> ownedActionService_;
	ClipLibraryController *controller_{nullptr};
	StatusCard *totalCard_{nullptr};
	StatusCard *favoriteCard_{nullptr};
	StatusCard *verticalCard_{nullptr};
	StatusCard *pendingCard_{nullptr};
	StatusCard *capturedDurationCard_{nullptr};
	QWidget *overviewHost_{nullptr};
	QWidget *cardsHost_{nullptr};
	QFrame *filtersHost_{nullptr};
	QGridLayout *overviewLayout_{nullptr};
	QGridLayout *metricsLayout_{nullptr};
	QGridLayout *filtersLayout_{nullptr};
	std::array<QPushButton *, 4> filterButtons_{};
	QButtonGroup *filterGroup_{nullptr};
	QWidget *exportHost_{nullptr};
	QGridLayout *exportLayout_{nullptr};
	QLineEdit *searchBox_{nullptr};
	QComboBox *historyScope_{nullptr};
	QComboBox *sortCombo_{nullptr};
	QWidget *batchBadge_{nullptr};
	QLabel *loadingLabel_{nullptr};
	QLabel *errorLabel_{nullptr};
	QWidget *emptyState_{nullptr};
	QWidget *listContainer_{nullptr};
	QVBoxLayout *listLayout_{nullptr};
	QLabel *summaryLabel_{nullptr};
	QComboBox *batchOrientation_{nullptr};
	QPushButton *batchExportButton_{nullptr};
	QPushButton *cancelExportButton_{nullptr};
	QPushButton *batchDeleteButton_{nullptr};
	QProgressBar *exportProgress_{nullptr};
	QLabel *exportStatusLabel_{nullptr};
	QTimer *exportPollTimer_{nullptr};
	ExportManager *exportManager_{nullptr};
	SettingsManager *settingsManager_{nullptr};
	FeatureGateService *featureGates_{nullptr};
	CaptionGenerator captionGenerator_;
	CaptionBusyCallback captionBusyCallback_;
	std::vector<std::string> selectedClipIds_;
	std::vector<std::string> handledTerminalExportIds_;
	std::vector<ClipMetadata> pendingRender_;
	std::size_t nextRenderIndex_{0};
	std::uint64_t renderGeneration_{0};
	bool compactLayout_{false};
	int metricColumnCount_{0};
};

} // namespace clipcoach::ui
