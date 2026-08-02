#pragma once

#include <QFrame>

#include <functional>

class QResizeEvent;

namespace clipcoach::ui {

struct ClipCardViewData {
	QString clipId;
	QString title;
	QString duration;
	QString dateTime;
	QString trigger;
	QString orientation;
	QString thumbnailPath;
	QString favoriteTooltip;
	QString previewLabel;
	QString editLabel;
	QString exportLabel;
	QString captionLabel;
	QString subtitlesLabel;
	QString openFolderLabel;
	QString deleteLabel;
	QString scoreLabel;
	QString processingLabel;
	int score{0};
	bool favorite{false};
	bool captionAvailable{false};
	bool subtitlesAvailable{false};
	bool exportVerticalEnabled{true};
	bool selected{false};
	bool processing{false};
};

class ClipCard final : public QFrame {
public:
	explicit ClipCard(const QString &title, const QString &metadata, int score = -1, QWidget *parent = nullptr);
	explicit ClipCard(ClipCardViewData data, QWidget *parent = nullptr);
	~ClipCard() override;

	void setFavoriteCallback(std::function<void(bool)> callback);
	void setPreviewCallback(std::function<void()> callback);
	void setEditCallback(std::function<void()> callback);
	void setExportCallback(std::function<void()> callback);
	void setCaptionCallback(std::function<void()> callback);
	void setCaptionBusy(bool busy, const QString &tooltip = {});
	void setSubtitlesCallback(std::function<void()> callback);
	void setOpenFolderCallback(std::function<void()> callback);
	void setDeleteCallback(std::function<void()> callback);
	void setSelectionCallback(std::function<void(bool)> callback);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	void updateResponsiveLayout();

	class Impl;
	Impl *impl_{nullptr};
};

} // namespace clipcoach::ui
