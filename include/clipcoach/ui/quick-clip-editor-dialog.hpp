#pragma once

#include <clipcoach/core/clip-metadata.hpp>

#include <QDialog>

#include <functional>

class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QTimer;

namespace clipcoach {
class ExportManager;
class SettingsManager;
}

namespace clipcoach::ui {

struct CaptionGenerationProgress;

class QuickClipEditorDialog final : public QDialog {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using CaptionRequest = std::function<void()>;

	QuickClipEditorDialog(TranslationFunction translator, ClipMetadata clip,
			      ExportManager *exportManager,
			      SettingsManager *settingsManager,
			      QWidget *parent = nullptr);
	~QuickClipEditorDialog() override;

	void setCaptionRequest(CaptionRequest callback);
	void setCaption(const QString &socialCaption,
			const QString &youtubeShortsCaption = {});
	void setCaptionBusy(bool busy);
	void setCaptionProgress(const CaptionGenerationProgress &progress);
	void setCaptionError(const QString &message);

private:
	class Impl;
	Impl *impl_{nullptr};
};

} // namespace clipcoach::ui
