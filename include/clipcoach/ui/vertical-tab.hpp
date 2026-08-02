#pragma once

#include <clipcoach/core/vertical-canvas-manager.hpp>
#include <clipcoach/ui/vertical-obs-bridge.hpp>

#include <QWidget>

#include <functional>

class QBoxLayout;
class QComboBox;
class QLabel;
class QPushButton;
class QResizeEvent;
class QSlider;
class QSpinBox;

namespace clipcoach::ui {

class VerticalPreviewWidget;

class VerticalTab final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using CanvasChangedCallback = std::function<void(const VerticalCanvasSettings &)>;
	using ActionCallback = std::function<void()>;

	VerticalTab(TranslationFunction translator, VerticalCanvasManager *manager, VerticalObsBridge obsBridge = {},
		    QWidget *parent = nullptr);

	void refresh();
	void refreshObsSceneOptions();
	void setCanvasChangedCallback(CanvasChangedCallback callback);
	void setCaptureActions(ActionCallback startReplay, ActionCallback saveClip);
	void setReplayState(bool active, bool transition = false);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	[[nodiscard]] QString text(const char *key) const;
	void build();
	void bind();
	void applyResolution();
	void refreshObsScenes();
	void refreshObsSources();
	void updateLivePreview();
	void updateResponsiveLayout();
	void setPreviewOnly(bool enabled);
	void syncCompactActions();
	void notifyCanvasChanged();
	void showError(const std::string &error);

	TranslationFunction translator_;
	VerticalCanvasManager *manager_{nullptr};
	VerticalObsBridge obsBridge_;
	CanvasChangedCallback canvasChangedCallback_;
	QWidget *preview_{nullptr};
	QWidget *pageHeader_{nullptr};
	QWidget *previewColumn_{nullptr};
	QWidget *compositionSection_{nullptr};
	QWidget *controlsSection_{nullptr};
	QWidget *compactActionBar_{nullptr};
	QBoxLayout *bodyLayout_{nullptr};
	QBoxLayout *headerLayout_{nullptr};
	QBoxLayout *headerActionsLayout_{nullptr};
	QBoxLayout *framingLayout_{nullptr};
	QLabel *widthHint_{nullptr};
	QLabel *verticalStateBadge_{nullptr};
	VerticalPreviewWidget *fallbackPreview_{nullptr};
	QPushButton *startReplayButton_{nullptr};
	QPushButton *saveClipButton_{nullptr};
	QPushButton *compactReplayButton_{nullptr};
	QPushButton *compactSaveButton_{nullptr};
	QPushButton *compactCreateSceneButton_{nullptr};
	QPushButton *previewOnlyButton_{nullptr};
	QComboBox *sceneCombo_{nullptr};
	QComboBox *sourceCombo_{nullptr};
	QPushButton *createSceneButton_{nullptr};
	QSpinBox *zoom_{nullptr};
	QSlider *zoomSlider_{nullptr};
	QSpinBox *panX_{nullptr};
	QSlider *panXSlider_{nullptr};
	QSpinBox *panY_{nullptr};
	QSlider *panYSlider_{nullptr};
	QComboBox *outputMode_{nullptr};
	QComboBox *resolution_{nullptr};
	QSpinBox *customWidth_{nullptr};
	QSpinBox *customHeight_{nullptr};
	QLabel *message_{nullptr};
	ActionCallback startReplayCallback_;
	ActionCallback saveClipCallback_;
	bool replayActive_{false};
	bool replayTransition_{false};
	bool refreshing_{false};
	bool compactLayout_{false};
	bool previewOnly_{false};
};

} // namespace clipcoach::ui
