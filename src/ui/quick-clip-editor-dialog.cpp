#include <clipcoach/ui/quick-clip-editor-dialog.hpp>

#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/ui/caption-generator.hpp>
#include <clipcoach/ui/design-tokens.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QImage>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProgressDialog>
#include <QProcess>
#include <QPixmap>
#include <QPushButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSplitter>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace clipcoach::ui {
namespace {

QString formatTime(qint64 milliseconds)
{
	milliseconds = std::max<qint64>(0, milliseconds);
	const qint64 totalSeconds = milliseconds / 1000;
	const qint64 minutes = totalSeconds / 60;
	const qint64 seconds = totalSeconds % 60;
	const qint64 tenths = (milliseconds % 1000) / 100;
	return QStringLiteral("%1:%2.%3").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0')).arg(tenths);
}

class TrimRange final : public QWidget {
public:
	struct Segment {
		qint64 start{0};
		qint64 end{0};
		bool suggested{false};
	};
	using Changed = std::function<void(qint64, qint64)>;
	using Seeked = std::function<void(qint64)>;

	explicit TrimRange(QWidget *parent = nullptr) : QWidget(parent)
	{
		setObjectName(QStringLiteral("quickEditorTrimRange"));
		setMinimumHeight(150);
		setMouseTracking(true);
	}

	void setDuration(qint64 duration)
	{
		const bool coveredPreviousDuration = end_ <= 0 || end_ == duration_;
		duration_ = std::max<qint64>(1000, duration);
		if (coveredPreviousDuration || end_ > duration_)
			end_ = duration_;
		start_ = std::clamp(start_, qint64{0}, end_ - 500);
		setProperty("startMilliseconds", start_);
		setProperty("endMilliseconds", end_);
		update();
	}

	void setPosition(qint64 position)
	{
		position_ = std::clamp(position, qint64{0}, duration_);
		setProperty("positionMilliseconds", position_);
		update();
	}

	qint64 start() const { return start_; }
	qint64 end() const { return end_; }
	qint64 position() const { return position_; }
	std::vector<Segment> visibleSegments() const
	{
		std::vector<qint64> boundaries{start_, end_};
		for (const auto point : splitPoints_)
			if (point > start_ && point < end_)
				boundaries.push_back(point);
		for (const auto &[rangeStart, rangeEnd] : suggestedRanges_) {
			boundaries.push_back(std::clamp(rangeStart, start_, end_));
			boundaries.push_back(std::clamp(rangeEnd, start_, end_));
		}
		for (const auto &[rangeStart, rangeEnd] : removedRanges_) {
			boundaries.push_back(std::clamp(rangeStart, start_, end_));
			boundaries.push_back(std::clamp(rangeEnd, start_, end_));
		}
		std::sort(boundaries.begin(), boundaries.end());
		boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
		std::vector<Segment> segments;
		for (std::size_t index = 1; index < boundaries.size(); ++index) {
			const qint64 segmentStart = boundaries[index - 1];
			const qint64 segmentEnd = boundaries[index];
			if (segmentEnd - segmentStart < 100)
				continue;
			const qint64 midpoint = segmentStart + (segmentEnd - segmentStart) / 2;
			const bool removed = std::any_of(
				removedRanges_.begin(), removedRanges_.end(), [midpoint](const auto &range) {
					return midpoint >= range.first && midpoint < range.second;
				});
			if (removed)
				continue;
			const bool suggested = std::any_of(
				suggestedRanges_.begin(), suggestedRanges_.end(), [midpoint](const auto &range) {
					return midpoint >= range.first && midpoint < range.second;
				});
			segments.push_back({segmentStart, segmentEnd, suggested});
		}
		return segments;
	}
	bool splitAt(qint64 position)
	{
		for (const auto &segment : visibleSegments()) {
			if (position <= segment.start + 200 || position >= segment.end - 200)
				continue;
			splitPoints_.push_back(position);
			std::sort(splitPoints_.begin(), splitPoints_.end());
			splitPoints_.erase(std::unique(splitPoints_.begin(), splitPoints_.end()), splitPoints_.end());
			selectedSourceStarts_ = {position};
			updateSelectionProperty();
			update();
			return true;
		}
		return false;
	}
	bool deleteSelected()
	{
		const auto segments = visibleSegments();
		std::vector<Segment> selected;
		for (const auto &segment : segments)
			if (isSelected(segment.start))
				selected.push_back(segment);
		if (selected.empty() || selected.size() >= segments.size())
			return false;
		qint64 deletedEnd = 0;
		for (const auto &segment : selected) {
			removedRanges_.emplace_back(segment.start, segment.end);
			deletedEnd = std::max(deletedEnd, segment.end);
		}
		std::sort(removedRanges_.begin(), removedRanges_.end());
		std::vector<std::pair<qint64, qint64>> merged;
		for (const auto &range : removedRanges_) {
			if (!merged.empty() && range.first <= merged.back().second)
				merged.back().second = std::max(merged.back().second, range.second);
			else
				merged.push_back(range);
		}
		removedRanges_ = std::move(merged);
		selectedSourceStarts_.clear();
		const auto remaining = visibleSegments();
		const auto next =
			std::find_if(remaining.begin(), remaining.end(),
				     [deletedEnd](const Segment &segment) { return segment.start >= deletedEnd; });
		position_ = next != remaining.end() ? next->start : remaining.back().start;
		setProperty("positionMilliseconds", position_);
		setProperty("removedRangeCount", static_cast<int>(removedRanges_.size()));
		updateSelectionProperty();
		update();
		return true;
	}
	void setSuggestedRanges(std::vector<std::pair<qint64, qint64>> ranges)
	{
		suggestedRanges_ = std::move(ranges);
		selectedSourceStarts_.clear();
		for (const auto &segment : visibleSegments())
			if (segment.suggested)
				selectedSourceStarts_.push_back(segment.start);
		updateSelectionProperty();
		update();
	}
	void clearEdits()
	{
		removedRanges_.clear();
		suggestedRanges_.clear();
		splitPoints_.clear();
		selectedSourceStarts_.clear();
		setProperty("removedRangeCount", 0);
		updateSelectionProperty();
		update();
	}
	bool hasSelectedSegment() const { return selectedSegmentCount() > 0; }
	std::size_t selectedSegmentCount() const
	{
		const auto segments = visibleSegments();
		return static_cast<std::size_t>(
			std::count_if(segments.begin(), segments.end(),
				      [this](const Segment &segment) { return isSelected(segment.start); }));
	}
	std::size_t selectedSuggestedCount() const
	{
		const auto segments = visibleSegments();
		return static_cast<std::size_t>(
			std::count_if(segments.begin(), segments.end(), [this](const Segment &segment) {
				return segment.suggested && isSelected(segment.start);
			}));
	}
	std::size_t suggestedRangeCount() const { return suggestedRanges_.size(); }
	bool hasEdits() const { return !splitPoints_.empty() || !removedRanges_.empty() || !suggestedRanges_.empty(); }
	const std::vector<std::pair<qint64, qint64>> &removedRanges() const { return removedRanges_; }
	void setRemovedRanges(std::vector<std::pair<qint64, qint64>> ranges)
	{
		removedRanges_ = std::move(ranges);
		setProperty("removedRangeCount", static_cast<int>(removedRanges_.size()));
		update();
	}
	void setWaveform(std::vector<qreal> waveform)
	{
		waveform_ = std::move(waveform);
		update();
	}
	void setThumbnail(QPixmap thumbnail)
	{
		thumbnail_ = std::move(thumbnail);
		update();
	}
	void setChanged(Changed callback) { changed_ = std::move(callback); }
	void setSeeked(Seeked callback) { seeked_ = std::move(callback); }

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing, true);
		const QRectF content(16, 31, width() - 32, 88);
		const auto segments = visibleSegments();
		qint64 retainedDuration = 0;
		for (const auto &segment : segments)
			retainedDuration += segment.end - segment.start;
		const int tickCount = std::clamp(static_cast<int>(retainedDuration / 10'000) + 1, 2, 12);
		painter.setPen(QColor(QStringLiteral("#7F91AC")));
		for (int tick = 0; tick < tickCount; ++tick) {
			const qreal ratio = static_cast<qreal>(tick) / (tickCount - 1);
			const qreal x = content.left() + ratio * content.width();
			painter.drawText(QRectF(x - 26, 8, 52, 18), Qt::AlignCenter,
					 formatTime(static_cast<qint64>(ratio * retainedDuration)));
		}
		qreal segmentX = content.left();
		for (std::size_t segmentIndex = 0; segmentIndex < segments.size(); ++segmentIndex) {
			const auto &segment = segments[segmentIndex];
			const qreal segmentWidth =
				retainedDuration > 0
					? content.width() * (segment.end - segment.start) / retainedDuration
					: 0;
			const QRectF segmentRect(segmentX, content.top(), std::max(2.0, segmentWidth - 3),
						 content.height());
			painter.save();
			painter.setClipRect(segmentRect);
			const int frames = std::max(1, static_cast<int>(segmentRect.width() / 80));
			for (int frame = 0; frame < frames; ++frame) {
				const QRectF frameRect(segmentRect.left() + frame * segmentRect.width() / frames,
						       segmentRect.top(), segmentRect.width() / frames + 1, 58);
				if (!thumbnail_.isNull())
					painter.drawPixmap(frameRect.toRect(), thumbnail_, thumbnail_.rect());
				else
					painter.fillRect(frameRect, QColor(QStringLiteral("#172337")));
			}
			const QRectF waveformRect(segmentRect.left(), segmentRect.top() + 59, segmentRect.width(), 28);
			painter.fillRect(waveformRect, QColor(8, 55, 61, 235));
			if (!waveform_.empty()) {
				painter.setPen(QPen(QColor(QStringLiteral("#12B8C4")), 1));
				const int firstBin =
					std::clamp(static_cast<int>(segment.start * waveform_.size() / duration_), 0,
						   static_cast<int>(waveform_.size() - 1));
				const int lastBin =
					std::clamp(static_cast<int>(segment.end * waveform_.size() / duration_),
						   firstBin + 1, static_cast<int>(waveform_.size()));
				for (int bin = firstBin; bin < lastBin; ++bin) {
					const qreal ratio = static_cast<qreal>(bin - firstBin) /
							    std::max(1, lastBin - firstBin - 1);
					const qreal x = waveformRect.left() + ratio * waveformRect.width();
					const qreal half =
						std::clamp(waveform_[static_cast<std::size_t>(bin)], 0.0, 1.0) *
						waveformRect.height() / 2.0;
					painter.drawLine(QPointF(x, waveformRect.center().y() - half),
							 QPointF(x, waveformRect.center().y() + half));
				}
			}
			painter.restore();
			const bool selected = isSelected(segment.start);
			painter.setPen(QPen(selected ? QColor(QStringLiteral("#FFD166"))
						     : (segment.suggested ? QColor(QStringLiteral("#FF9F43"))
									  : QColor(QStringLiteral("#4D6A88"))),
					    selected ? 3 : 1));
			painter.setBrush(segment.suggested ? QColor(255, 159, 67, 45) : Qt::NoBrush);
			painter.drawRect(segmentRect);
			segmentX += segmentWidth;
		}
		const QRectF track(16, 130, width() - 32, 8);
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(QStringLiteral("#28364B")));
		painter.drawRoundedRect(track, 4, 4);
		const qreal startX = 16.0;
		const qreal endX = width() - 16.0;
		painter.setBrush(QColor(QStringLiteral("#8247F5")));
		painter.drawRoundedRect(QRectF(startX, track.top(), endX - startX, track.height()), 4, 4);
		painter.setPen(QPen(QColor(QStringLiteral("#62D6FF")), 2));
		const qreal playX = sourceX(position_, segments, content);
		painter.drawLine(QPointF(playX, 26), QPointF(playX, height() - 5));
		const QString positionText = formatTime(position_);
		const QFontMetrics metrics(painter.font());
		const qreal bubbleWidth = metrics.horizontalAdvance(positionText) + 16;
		const qreal bubbleX = std::clamp(playX - bubbleWidth / 2.0, 2.0, width() - bubbleWidth - 2.0);
		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor(QStringLiteral("#12344A")));
		painter.drawRoundedRect(QRectF(bubbleX, 1, bubbleWidth, 22), 6, 6);
		painter.setPen(QColor(QStringLiteral("#9BE8FF")));
		painter.drawText(QRectF(bubbleX, 1, bubbleWidth, 22), Qt::AlignCenter, positionText);
		for (const qreal x : {startX, endX}) {
			painter.setPen(QPen(QColor(QStringLiteral("#DCCBFF")), 2));
			painter.setBrush(QColor(QStringLiteral("#8B5CF6")));
			painter.drawRoundedRect(QRectF(x - 7, track.center().y() - 15, 14, 30), 5, 5);
		}
	}

	void mousePressEvent(QMouseEvent *event) override
	{
		const auto x = event->position().x();
		const auto startDistance = std::abs(x - 16.0);
		const auto endDistance = std::abs(x - (width() - 16.0));
		if (std::min(startDistance, endDistance) <= 12.0)
			dragMode_ = startDistance <= endDistance ? DragMode::Start : DragMode::End;
		else
			dragMode_ = DragMode::Playhead;
		applyPointer(x, event->modifiers(), true);
	}

	void mouseMoveEvent(QMouseEvent *event) override
	{
		if (event->buttons().testFlag(Qt::LeftButton))
			applyPointer(event->position().x(), event->modifiers(), false);
	}

private:
	qreal valueX(qint64 value) const { return 16.0 + (width() - 32.0) * (static_cast<qreal>(value) / duration_); }

	qreal sourceX(qint64 source, const std::vector<Segment> &segments, const QRectF &content) const
	{
		qint64 total = 0;
		for (const auto &segment : segments)
			total += segment.end - segment.start;
		qint64 before = 0;
		for (const auto &segment : segments) {
			if (source >= segment.start && source <= segment.end)
				return content.left() +
				       content.width() * (before + source - segment.start) / std::max<qint64>(1, total);
			before += segment.end - segment.start;
		}
		return content.left();
	}

	qint64 compactXValue(qreal x) const
	{
		const auto segments = visibleSegments();
		qint64 total = 0;
		for (const auto &segment : segments)
			total += segment.end - segment.start;
		const qint64 offset =
			static_cast<qint64>(std::clamp((x - 16.0) / std::max(1.0, width() - 32.0), 0.0, 1.0) * total);
		qint64 cursor = 0;
		for (const auto &segment : segments) {
			const qint64 length = segment.end - segment.start;
			if (offset <= cursor + length)
				return segment.start + std::clamp(offset - cursor, qint64{0}, length);
			cursor += length;
		}
		return segments.empty() ? start_ : segments.back().end;
	}

	bool isSelected(qint64 sourceStart) const
	{
		return std::find(selectedSourceStarts_.begin(), selectedSourceStarts_.end(), sourceStart) !=
		       selectedSourceStarts_.end();
	}

	void updateSelectionProperty()
	{
		setProperty("hasSelectedSegment", hasSelectedSegment());
		setProperty("selectedSegmentCount", static_cast<int>(selectedSegmentCount()));
		setProperty("selectedSuggestedCount", static_cast<int>(selectedSuggestedCount()));
		setProperty("visibleSegmentCount", static_cast<int>(visibleSegments().size()));
		setProperty("suggestedRangeCount", static_cast<int>(suggestedRanges_.size()));
	}

	qint64 xValue(qreal x) const
	{
		const qreal normalized = std::clamp((x - 16.0) / std::max(1.0, width() - 32.0), 0.0, 1.0);
		return static_cast<qint64>(normalized * duration_);
	}

	void applyPointer(qreal x, Qt::KeyboardModifiers modifiers, bool updateSelection)
	{
		const auto value = dragMode_ == DragMode::Playhead ? compactXValue(x) : xValue(x);
		if (dragMode_ == DragMode::Start)
			start_ = std::clamp(value, qint64{0}, end_ - 500);
		else if (dragMode_ == DragMode::End)
			end_ = std::clamp(value, start_ + 500, duration_);
		else {
			position_ = std::clamp(value, start_, end_);
			const auto segments = visibleSegments();
			for (const auto &segment : segments)
				if (position_ >= segment.start && position_ <= segment.end) {
					if (updateSelection && modifiers.testFlag(Qt::ControlModifier)) {
						auto selected = std::find(selectedSourceStarts_.begin(),
									  selectedSourceStarts_.end(), segment.start);
						if (selected == selectedSourceStarts_.end())
							selectedSourceStarts_.push_back(segment.start);
						else
							selectedSourceStarts_.erase(selected);
					} else if (updateSelection) {
						selectedSourceStarts_ = {segment.start};
					}
					break;
				}
			updateSelectionProperty();
			setProperty("positionMilliseconds", position_);
			update();
			if (seeked_)
				seeked_(position_);
			return;
		}
		setProperty("startMilliseconds", start_);
		setProperty("endMilliseconds", end_);
		update();
		if (changed_)
			changed_(start_, end_);
	}

	qint64 duration_{1000};
	qint64 start_{0};
	qint64 end_{1000};
	qint64 position_{0};
	std::vector<std::pair<qint64, qint64>> removedRanges_;
	std::vector<std::pair<qint64, qint64>> suggestedRanges_;
	std::vector<qint64> splitPoints_;
	std::vector<qint64> selectedSourceStarts_;
	std::vector<qreal> waveform_;
	QPixmap thumbnail_;
	enum class DragMode { Start, End, Playhead };
	DragMode dragMode_{DragMode::Playhead};
	Changed changed_;
	Seeked seeked_;
};

} // namespace

class QuickClipEditorDialog::Impl final {
public:
	TranslationFunction translator;
	ClipMetadata clip;
	ExportManager *exportManager{nullptr};
	SettingsManager *settingsManager{nullptr};
	QLabel *video{nullptr};
	QProcess *playbackProcess{nullptr};
	QProcess *previewProcess{nullptr};
	QTimer *playbackTimer{nullptr};
	QTimer *previewDebounce{nullptr};
	QByteArray frameBuffer;
	QString ffmpegExecutable;
	qint64 playbackPosition{0};
	qint64 playbackBasePosition{0};
	qint64 playbackSegmentEnd{0};
	QElapsedTimer playbackClock;
	bool playing{false};
	TrimRange *range{nullptr};
	QLabel *startLabel{nullptr};
	QLabel *endLabel{nullptr};
	QLabel *selectionLabel{nullptr};
	QPushButton *play{nullptr};
	QPlainTextEdit *socialCaption{nullptr};
	QPlainTextEdit *shortsCaption{nullptr};
	QPushButton *generateCaption{nullptr};
	QComboBox *quality{nullptr};
	QComboBox *fps{nullptr};
	QLineEdit *directory{nullptr};
	QPushButton *exportButton{nullptr};
	QPushButton *cancelButton{nullptr};
	QProgressBar *progress{nullptr};
	QLabel *status{nullptr};
	QTimer *jobTimer{nullptr};
	QDialog *captionProgressDialog{nullptr};
	QLabel *captionProgressStatus{nullptr};
	QLabel *captionProgressPercent{nullptr};
	QLabel *captionProgressEta{nullptr};
	QProgressBar *captionProgressBar{nullptr};
	QTimer *captionTimeoutTimer{nullptr};
	QProcess *waveformProcess{nullptr};
	QProcess *smartTrimProcess{nullptr};
	QProgressDialog *smartTrimProgress{nullptr};
	QPushButton *markCutStart{nullptr};
	QPushButton *removeCut{nullptr};
	QPushButton *undoCuts{nullptr};
	QPushButton *smartTrim{nullptr};
	qint64 pendingCutStart{-1};
	std::string jobId;
	CaptionRequest captionRequest;
	qint64 durationMs{1000};
	static constexpr int kPreviewWidth = 960;
	static constexpr int kPreviewHeight = 540;
	static constexpr int kPreviewFps = 30;
	static constexpr int kFrameBytes = kPreviewWidth * kPreviewHeight * 4;

	QString text(const char *key) const { return translator ? translator(key) : QString::fromUtf8(key); }

	QString locateFfmpeg() const
	{
		const auto configured = qEnvironmentVariable("CLIPXTUDIO_FFMPEG_PATH");
		if (!configured.isEmpty() && QFileInfo(configured).isExecutable())
			return configured;
		for (const auto &libraryPath : QCoreApplication::libraryPaths()) {
			QDir directory(libraryPath);
			if (directory.dirName().compare(QStringLiteral("qt-plugins"), Qt::CaseInsensitive) != 0)
				continue;
			directory.cdUp();
			const auto candidate = directory.filePath(QStringLiteral("tools/ffmpeg/ffmpeg.exe"));
			if (QFileInfo(candidate).isExecutable())
				return candidate;
		}
		const QStringList roots{
			qEnvironmentVariable("ProgramData") + QStringLiteral("/obs-studio/plugins/clipxtudio/data"),
			qEnvironmentVariable("APPDATA") + QStringLiteral("/obs-studio/plugins/clipxtudio/data")};
		for (const auto &root : roots) {
			const auto candidate = QDir(root).filePath(QStringLiteral("tools/ffmpeg/ffmpeg.exe"));
			if (QFileInfo(candidate).isExecutable())
				return candidate;
		}
		return QStringLiteral("ffmpeg");
	}

	QStringList decodeArguments(qint64 start, qint64 duration, bool singleFrame) const
	{
		QStringList arguments{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
				      QStringLiteral("error")};
		if (!singleFrame)
			arguments << QStringLiteral("-re");
		arguments << QStringLiteral("-ss") << QString::number(start / 1000.0, 'f', 3) << QStringLiteral("-i")
			  << QString::fromStdString(clip.filePath.u8string());
		if (!singleFrame)
			arguments << QStringLiteral("-t") << QString::number(duration / 1000.0, 'f', 3);
		arguments << QStringLiteral("-an") << QStringLiteral("-vf")
			  << QStringLiteral("scale=960:540:force_original_aspect_ratio=decrease,"
					    "pad=960:540:(ow-iw)/2:(oh-ih)/2:color=black,"
					    "fps=30,format=bgra");
		if (singleFrame)
			arguments << QStringLiteral("-frames:v") << QStringLiteral("1");
		arguments << QStringLiteral("-f") << QStringLiteral("rawvideo") << QStringLiteral("-pix_fmt")
			  << QStringLiteral("bgra") << QStringLiteral("pipe:1");
		return arguments;
	}

	void showFrame(const QByteArray &bytes)
	{
		if (bytes.size() < kFrameBytes)
			return;
		const QImage frame(reinterpret_cast<const uchar *>(bytes.constData()), kPreviewWidth, kPreviewHeight,
				   QImage::Format_ARGB32);
		const auto pixmap = QPixmap::fromImage(frame.copy());
		video->setPixmap(pixmap.scaled(video->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
		if (range != nullptr)
			range->setThumbnail(
				pixmap.scaled(160, 90, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
	}
};

QuickClipEditorDialog::QuickClipEditorDialog(TranslationFunction translator, ClipMetadata clip,
					     ExportManager *exportManager, SettingsManager *settingsManager,
					     QWidget *parent)
	: QDialog(parent),
	  impl_(new Impl)
{
	impl_->translator = std::move(translator);
	impl_->clip = std::move(clip);
	impl_->exportManager = exportManager;
	impl_->settingsManager = settingsManager;
	impl_->durationMs = std::max(1, impl_->clip.durationSeconds) * 1000LL;
	setObjectName(QStringLiteral("quickClipEditorDialog"));
	setAttribute(Qt::WA_DeleteOnClose, true);
	setAttribute(Qt::WA_StyledBackground, true);
	setStyleSheet(tokens::styleSheet());
	setWindowTitle(impl_->text(strings::kClipsQuickEditorTitle));
	setModal(false);
	resize(1120, 720);
	setMinimumSize(880, 620);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(tokens::kSpaceLg, tokens::kSpaceLg, tokens::kSpaceLg, tokens::kSpaceLg);
	root->setSpacing(tokens::kSpaceMd);
	auto *heading = new QHBoxLayout();
	auto *copy = new QVBoxLayout();
	auto *title = new QLabel(impl_->text(strings::kClipsQuickEditorTitle), this);
	title->setObjectName(QStringLiteral("quickEditorTitle"));
	title->setProperty("textRole", QStringLiteral("pageTitle"));
	auto *subtitle = new QLabel(impl_->text(strings::kClipsQuickEditorSubtitle), this);
	subtitle->setProperty("textRole", QStringLiteral("muted"));
	subtitle->setWordWrap(true);
	copy->addWidget(title);
	copy->addWidget(subtitle);
	heading->addLayout(copy, 1);
	auto *close = new QPushButton(impl_->text(strings::kClipsQuickEditorClose), this);
	close->setObjectName(QStringLiteral("quickEditorCloseButton"));
	close->setProperty("controlRole", QStringLiteral("secondary"));
	heading->addWidget(close, 0, Qt::AlignTop);
	root->addLayout(heading);

	auto *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->setObjectName(QStringLiteral("quickEditorSplitter"));
	splitter->setChildrenCollapsible(false);

	auto *mediaCard = new QFrame(splitter);
	mediaCard->setObjectName(QStringLiteral("quickEditorMediaCard"));
	mediaCard->setProperty("cardRole", QStringLiteral("editor"));
	auto *mediaLayout = new QVBoxLayout(mediaCard);
	mediaLayout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd);
	mediaLayout->setSpacing(tokens::kSpaceSm);
	impl_->video = new QLabel(mediaCard);
	impl_->video->setObjectName(QStringLiteral("quickEditorVideo"));
	impl_->video->setMinimumSize(520, 300);
	impl_->video->setAlignment(Qt::AlignCenter);
	impl_->video->setText(impl_->text(strings::kClipsQuickEditorLoadingPreview));
	impl_->video->setStyleSheet(QStringLiteral("background:#05070D;border-radius:10px;"));
	mediaLayout->addWidget(impl_->video, 1);

	impl_->range = new TrimRange(mediaCard);
	impl_->range->setDuration(impl_->durationMs);
	mediaLayout->addWidget(impl_->range);
	auto *rangeInfo = new QHBoxLayout();
	impl_->startLabel = new QLabel(formatTime(0), mediaCard);
	impl_->startLabel->setObjectName(QStringLiteral("quickEditorStartLabel"));
	impl_->selectionLabel = new QLabel(mediaCard);
	impl_->selectionLabel->setObjectName(QStringLiteral("quickEditorSelectionLabel"));
	impl_->selectionLabel->setAlignment(Qt::AlignCenter);
	impl_->endLabel = new QLabel(formatTime(impl_->durationMs), mediaCard);
	impl_->endLabel->setObjectName(QStringLiteral("quickEditorEndLabel"));
	rangeInfo->addWidget(impl_->startLabel);
	rangeInfo->addWidget(impl_->selectionLabel, 1);
	rangeInfo->addWidget(impl_->endLabel);
	mediaLayout->addLayout(rangeInfo);
	auto *cutActions = new QHBoxLayout();
	impl_->markCutStart = new QPushButton(impl_->text(strings::kClipsQuickEditorSplit), mediaCard);
	impl_->markCutStart->setObjectName(QStringLiteral("quickEditorMarkCutStartButton"));
	impl_->removeCut = new QPushButton(impl_->text(strings::kClipsQuickEditorRemoveCut), mediaCard);
	impl_->removeCut->setObjectName(QStringLiteral("quickEditorRemoveCutButton"));
	impl_->removeCut->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
	impl_->removeCut->setIconSize(QSize(17, 17));
	impl_->removeCut->setEnabled(false);
	impl_->undoCuts = new QPushButton(impl_->text(strings::kClipsQuickEditorUndoCuts), mediaCard);
	impl_->undoCuts->setObjectName(QStringLiteral("quickEditorUndoCutsButton"));
	impl_->undoCuts->setEnabled(false);
	impl_->smartTrim = new QPushButton(impl_->text(strings::kClipsQuickEditorSmartTrim), mediaCard);
	impl_->smartTrim->setObjectName(QStringLiteral("quickEditorSmartTrimButton"));
	impl_->smartTrim->setProperty("buttonRole", QStringLiteral("primary"));
	cutActions->addWidget(impl_->markCutStart);
	cutActions->addWidget(impl_->removeCut);
	cutActions->addWidget(impl_->undoCuts);
	cutActions->addStretch();
	cutActions->addWidget(impl_->smartTrim);
	mediaLayout->addLayout(cutActions);
	auto *cutHelp = new QLabel(impl_->text(strings::kClipsQuickEditorCutHelp), mediaCard);
	cutHelp->setWordWrap(true);
	cutHelp->setProperty("textRole", QStringLiteral("muted"));
	mediaLayout->addWidget(cutHelp);

	auto *transport = new QHBoxLayout();
	impl_->play = new QPushButton(mediaCard);
	impl_->play->setObjectName(QStringLiteral("quickEditorPlayButton"));
	impl_->play->setProperty("controlRole", QStringLiteral("primary"));
	impl_->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	impl_->play->setIconSize(QSize(18, 18));
	impl_->play->setFixedSize(46, 36);
	impl_->play->setStyleSheet(QStringLiteral("QPushButton#quickEditorPlayButton{min-height:0px;max-height:36px;"
						  "min-width:46px;max-width:46px;padding:0px;}"));
	impl_->play->setToolTip(impl_->text(strings::kClipsQuickEditorPlay));
	impl_->play->setAccessibleName(impl_->text(strings::kClipsQuickEditorPlay));
	transport->addWidget(impl_->play);
	auto *fileName = new QLabel(QString::fromStdString(impl_->clip.fileName), mediaCard);
	fileName->setProperty("textRole", QStringLiteral("muted"));
	fileName->setTextInteractionFlags(Qt::TextSelectableByMouse);
	transport->addWidget(fileName, 1);
	mediaLayout->addLayout(transport);

	auto *side = new QFrame(splitter);
	side->setObjectName(QStringLiteral("quickEditorSideCard"));
	side->setProperty("cardRole", QStringLiteral("editor"));
	auto *sideLayout = new QVBoxLayout(side);
	sideLayout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd);
	sideLayout->setSpacing(tokens::kSpaceSm);
	auto *captionTitle = new QLabel(impl_->text(strings::kClipsQuickEditorCaption), side);
	captionTitle->setProperty("textRole", QStringLiteral("sectionTitle"));
	sideLayout->addWidget(captionTitle);
	impl_->socialCaption = new QPlainTextEdit(side);
	impl_->socialCaption->setObjectName(QStringLiteral("quickEditorSocialCaption"));
	impl_->socialCaption->setPlaceholderText(impl_->text(strings::kClipsQuickEditorNoCaption));
	impl_->socialCaption->setMinimumHeight(120);
	sideLayout->addWidget(impl_->socialCaption, 1);
	auto *captionActions = new QHBoxLayout();
	impl_->generateCaption = new QPushButton(impl_->text(strings::kClipsQuickEditorGenerateCaption), side);
	impl_->generateCaption->setObjectName(QStringLiteral("quickEditorGenerateCaptionButton"));
	auto *copyCaption = new QPushButton(impl_->text(strings::kClipsQuickEditorCopyCaption), side);
	copyCaption->setObjectName(QStringLiteral("quickEditorCopyCaptionButton"));
	captionActions->addWidget(impl_->generateCaption, 1);
	captionActions->addWidget(copyCaption);
	sideLayout->addLayout(captionActions);
	auto *shortsLabel = new QLabel(impl_->text(strings::kClipsQuickEditorShortsCaption), side);
	shortsLabel->setProperty("textRole", QStringLiteral("muted"));
	sideLayout->addWidget(shortsLabel);
	impl_->shortsCaption = new QPlainTextEdit(side);
	impl_->shortsCaption->setObjectName(QStringLiteral("quickEditorShortsCaption"));
	impl_->shortsCaption->setMaximumHeight(72);
	sideLayout->addWidget(impl_->shortsCaption);

	auto *exportTitle = new QLabel(impl_->text(strings::kClipsQuickEditorExport), side);
	exportTitle->setProperty("textRole", QStringLiteral("sectionTitle"));
	sideLayout->addWidget(exportTitle);
	auto *options = new QGridLayout();
	options->addWidget(new QLabel(impl_->text(strings::kClipsQuickEditorQuality), side), 0, 0);
	options->addWidget(new QLabel(impl_->text(strings::kClipsQuickEditorFps), side), 0, 1);
	impl_->quality = new QComboBox(side);
	impl_->quality->setObjectName(QStringLiteral("quickEditorQualityCombo"));
	impl_->quality->addItem(impl_->text(strings::kClipsQuickEditorQualityMedium),
				static_cast<int>(ExportQualityPreset::Medium));
	impl_->quality->addItem(impl_->text(strings::kClipsQuickEditorQualityHigh),
				static_cast<int>(ExportQualityPreset::High));
	impl_->quality->addItem(impl_->text(strings::kClipsQuickEditorQualityMaximum),
				static_cast<int>(ExportQualityPreset::Maximum));
	impl_->quality->setCurrentIndex(1);
	impl_->fps = new QComboBox(side);
	impl_->fps->setObjectName(QStringLiteral("quickEditorFpsCombo"));
	impl_->fps->addItem(impl_->text(strings::kClipsQuickEditorFpsOriginal), 0);
	impl_->fps->addItem(QStringLiteral("30 FPS"), 30);
	impl_->fps->addItem(QStringLiteral("60 FPS"), 60);
	options->addWidget(impl_->quality, 1, 0);
	options->addWidget(impl_->fps, 1, 1);
	sideLayout->addLayout(options);

	impl_->directory = new QLineEdit(side);
	impl_->directory->setObjectName(QStringLiteral("quickEditorDirectory"));
	const auto fallbackDirectory = impl_->clip.filePath.parent_path() / "ClipXtudio Exports";
	const auto exportDirectory = impl_->settingsManager != nullptr &&
						     !impl_->settingsManager->settings().exportDirectory.empty()
					     ? impl_->settingsManager->settings().exportDirectory
					     : fallbackDirectory;
	impl_->directory->setText(QString::fromStdString(exportDirectory.u8string()));
	auto *directoryRow = new QHBoxLayout();
	directoryRow->addWidget(impl_->directory, 1);
	auto *browse = new QPushButton(impl_->text(strings::kClipsQuickEditorBrowse), side);
	browse->setObjectName(QStringLiteral("quickEditorBrowseButton"));
	directoryRow->addWidget(browse);
	sideLayout->addLayout(directoryRow);
	impl_->progress = new QProgressBar(side);
	impl_->progress->setObjectName(QStringLiteral("quickEditorExportProgress"));
	impl_->progress->setRange(0, 100);
	impl_->progress->hide();
	sideLayout->addWidget(impl_->progress);
	impl_->status = new QLabel(side);
	impl_->status->setObjectName(QStringLiteral("quickEditorStatus"));
	impl_->status->setWordWrap(true);
	impl_->status->hide();
	sideLayout->addWidget(impl_->status);
	auto *exportActions = new QHBoxLayout();
	impl_->cancelButton = new QPushButton(impl_->text(strings::kClipsQuickEditorCancelExport), side);
	impl_->cancelButton->setObjectName(QStringLiteral("quickEditorCancelExportButton"));
	impl_->cancelButton->hide();
	impl_->exportButton = new QPushButton(impl_->text(strings::kClipsQuickEditorExportButton), side);
	impl_->exportButton->setObjectName(QStringLiteral("quickEditorExportButton"));
	impl_->exportButton->setProperty("buttonRole", QStringLiteral("primary"));
	exportActions->addWidget(impl_->cancelButton);
	exportActions->addWidget(impl_->exportButton, 1);
	sideLayout->addLayout(exportActions);

	splitter->addWidget(mediaCard);
	splitter->addWidget(side);
	splitter->setStretchFactor(0, 3);
	splitter->setStretchFactor(1, 2);
	root->addWidget(splitter, 1);

	impl_->ffmpegExecutable = impl_->locateFfmpeg();
	impl_->playbackProcess = new QProcess(this);
	impl_->playbackProcess->setProcessChannelMode(QProcess::SeparateChannels);
	impl_->previewProcess = new QProcess(this);
	impl_->previewProcess->setProcessChannelMode(QProcess::SeparateChannels);
	impl_->playbackTimer = new QTimer(this);
	impl_->playbackTimer->setInterval(1000 / Impl::kPreviewFps);
	impl_->previewDebounce = new QTimer(this);
	impl_->previewDebounce->setSingleShot(true);
	impl_->previewDebounce->setInterval(90);
	impl_->jobTimer = new QTimer(this);
	impl_->jobTimer->setInterval(250);
	impl_->waveformProcess = new QProcess(this);
	impl_->waveformProcess->setProcessChannelMode(QProcess::SeparateChannels);
	impl_->smartTrimProcess = new QProcess(this);
	impl_->smartTrimProcess->setProcessChannelMode(QProcess::SeparateChannels);

	auto stopPlayback = [this] {
		impl_->playing = false;
		impl_->playbackTimer->stop();
		if (impl_->playbackProcess->state() != QProcess::NotRunning)
			impl_->playbackProcess->kill();
		impl_->frameBuffer.clear();
		impl_->playbackSegmentEnd = 0;
		impl_->play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
		impl_->play->setToolTip(impl_->text(strings::kClipsQuickEditorPlay));
		impl_->play->setAccessibleName(impl_->text(strings::kClipsQuickEditorPlay));
	};
	auto requestPreview = [this] {
		if (impl_->playing)
			return;
		if (impl_->previewProcess->state() != QProcess::NotRunning)
			impl_->previewProcess->kill();
		impl_->previewProcess->setProperty("frameBuffer", QByteArray{});
		impl_->previewProcess->start(impl_->ffmpegExecutable,
					     impl_->decodeArguments(impl_->playbackPosition, 0, true),
					     QIODevice::ReadOnly);
	};

	auto updateRangeLabels = [this](qint64 start, qint64 end) {
		if (impl_->range->hasEdits()) {
			impl_->range->clearEdits();
			impl_->undoCuts->setEnabled(false);
			impl_->removeCut->setEnabled(false);
		}
		impl_->startLabel->setText(formatTime(start));
		impl_->endLabel->setText(formatTime(end));
		impl_->selectionLabel->setText(
			impl_->text(strings::kClipsQuickEditorSelection).arg(formatTime(end - start)));
		if (impl_->playbackPosition < start || impl_->playbackPosition > end)
			impl_->playbackPosition = start;
		impl_->previewDebounce->start();
	};
	auto updateDeleteAction = [this] {
		const auto selected = impl_->range->selectedSegmentCount();
		impl_->removeCut->setEnabled(selected > 0);
		if (selected > 1)
			impl_->removeCut->setText(impl_->text(strings::kClipsQuickEditorRemoveSelected).arg(selected));
		else
			impl_->removeCut->setText(impl_->text(strings::kClipsQuickEditorRemoveCut));
	};
	impl_->range->setChanged(updateRangeLabels);
	impl_->range->setSeeked([this, stopPlayback, updateDeleteAction](qint64 position) {
		if (impl_->playing)
			stopPlayback();
		impl_->playbackPosition = position;
		updateDeleteAction();
		impl_->previewDebounce->start();
	});
	updateRangeLabels(0, impl_->durationMs);
	auto normalizeCuts = [this](std::vector<std::pair<qint64, qint64>> ranges) {
		for (auto &[start, end] : ranges) {
			start = std::clamp(start, impl_->range->start(), impl_->range->end());
			end = std::clamp(end, impl_->range->start(), impl_->range->end());
			if (end < start)
				std::swap(start, end);
		}
		ranges.erase(std::remove_if(ranges.begin(), ranges.end(),
					    [](const auto &range) { return range.second - range.first < 250; }),
			     ranges.end());
		std::sort(ranges.begin(), ranges.end());
		std::vector<std::pair<qint64, qint64>> merged;
		for (const auto &range : ranges) {
			if (!merged.empty() && range.first <= merged.back().second + 50)
				merged.back().second = std::max(merged.back().second, range.second);
			else
				merged.push_back(range);
		}
		return merged;
	};
	auto refreshCutSummary = [this, updateDeleteAction] {
		qint64 removed = 0;
		for (const auto &[start, end] : impl_->range->removedRanges())
			removed += end - start;
		const qint64 kept = std::max<qint64>(0, impl_->range->end() - impl_->range->start() - removed);
		impl_->selectionLabel->setText(impl_->text(strings::kClipsQuickEditorSelectionAfterCuts)
						       .arg(formatTime(kept))
						       .arg(impl_->range->removedRanges().size()));
		impl_->undoCuts->setEnabled(impl_->range->hasEdits());
		updateDeleteAction();
	};

	connect(close, &QPushButton::clicked, this, &QDialog::close);
	connect(impl_->markCutStart, &QPushButton::clicked, this, [this, refreshCutSummary] {
		if (!impl_->range->splitAt(impl_->range->position())) {
			QMessageBox::information(this, impl_->text(strings::kClipsQuickEditorSplit),
						 impl_->text(strings::kClipsQuickEditorSplitUnavailable));
			return;
		}
		refreshCutSummary();
	});
	connect(impl_->removeCut, &QPushButton::clicked, this, [this, refreshCutSummary] {
		if (!impl_->range->deleteSelected()) {
			QMessageBox::information(this, impl_->text(strings::kClipsQuickEditorRemoveCut),
						 impl_->text(strings::kClipsQuickEditorDeleteUnavailable));
			return;
		}
		impl_->playbackPosition = impl_->range->position();
		impl_->previewDebounce->start();
		refreshCutSummary();
	});
	connect(impl_->undoCuts, &QPushButton::clicked, this, [this, refreshCutSummary] {
		impl_->range->clearEdits();
		refreshCutSummary();
	});
	auto *splitShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), this);
	splitShortcut->setContext(Qt::WindowShortcut);
	connect(splitShortcut, &QShortcut::activated, impl_->markCutStart, &QPushButton::click);
	auto *deleteShortcut = new QShortcut(QKeySequence(Qt::Key_Delete), this);
	deleteShortcut->setContext(Qt::WindowShortcut);
	connect(deleteShortcut, &QShortcut::activated, this, [this] {
		if (impl_->removeCut->isEnabled())
			impl_->removeCut->click();
	});
	connect(impl_->previewDebounce, &QTimer::timeout, this, requestPreview);
	connect(impl_->previewProcess, &QProcess::readyReadStandardOutput, this, [this] {
		auto bytes = impl_->previewProcess->property("frameBuffer").toByteArray();
		bytes.append(impl_->previewProcess->readAllStandardOutput());
		impl_->previewProcess->setProperty("frameBuffer", bytes);
		if (bytes.size() >= Impl::kFrameBytes)
			impl_->showFrame(bytes.left(Impl::kFrameBytes));
	});
	connect(impl_->waveformProcess, &QProcess::finished, this, [this](int exitCode) {
		if (exitCode != 0)
			return;
		const auto pcm = impl_->waveformProcess->readAllStandardOutput();
		if (pcm.size() < 2)
			return;
		constexpr int bins = 180;
		std::vector<qreal> waveform(bins, 0.0);
		const auto sampleCount = pcm.size() / 2;
		const auto *bytes = reinterpret_cast<const unsigned char *>(pcm.constData());
		for (qsizetype sample = 0; sample < sampleCount; ++sample) {
			const auto raw =
				static_cast<std::int16_t>(static_cast<std::uint16_t>(bytes[sample * 2]) |
							  (static_cast<std::uint16_t>(bytes[sample * 2 + 1]) << 8));
			const int bin = std::min(bins - 1, static_cast<int>((sample * bins) / sampleCount));
			waveform[bin] = std::max(waveform[bin], std::abs(static_cast<qreal>(raw)) / 32768.0);
		}
		impl_->range->setWaveform(std::move(waveform));
	});
	connect(impl_->smartTrim, &QPushButton::clicked, this, [this] {
		if (impl_->smartTrimProcess->state() != QProcess::NotRunning)
			return;
		impl_->smartTrim->setEnabled(false);
		impl_->smartTrim->setText(impl_->text(strings::kClipsQuickEditorSmartTrimAnalyzing));
		impl_->smartTrimProgress = new QProgressDialog(impl_->text(strings::kClipsQuickEditorSmartTrimProgress),
							       QString(), 0, 0, this);
		impl_->smartTrimProgress->setWindowTitle(impl_->text(strings::kClipsQuickEditorSmartTrim));
		impl_->smartTrimProgress->setWindowModality(Qt::WindowModal);
		impl_->smartTrimProgress->setCancelButton(nullptr);
		impl_->smartTrimProgress->setMinimumDuration(0);
		impl_->smartTrimProgress->setMinimumWidth(480);
		impl_->smartTrimProgress->show();
		impl_->smartTrimProcess->setProperty("analysisOutput", QByteArray{});
		impl_->smartTrimProcess->start(
			impl_->ffmpegExecutable,
			{QStringLiteral("-hide_banner"), QStringLiteral("-nostdin"), QStringLiteral("-i"),
			 QString::fromStdString(impl_->clip.filePath.u8string()), QStringLiteral("-af"),
			 QStringLiteral("silencedetect=noise=-50dB:d=0.65"), QStringLiteral("-f"),
			 QStringLiteral("null"), QStringLiteral("-")},
			QIODevice::ReadOnly);
	});
	connect(impl_->smartTrimProcess, &QProcess::readyReadStandardError, this, [this] {
		auto output = impl_->smartTrimProcess->property("analysisOutput").toByteArray();
		output += impl_->smartTrimProcess->readAllStandardError();
		impl_->smartTrimProcess->setProperty("analysisOutput", output);
	});
	connect(impl_->smartTrimProcess, &QProcess::finished, this,
		[this, normalizeCuts, refreshCutSummary](int exitCode) {
			if (impl_->smartTrimProgress != nullptr) {
				impl_->smartTrimProgress->close();
				impl_->smartTrimProgress->deleteLater();
				impl_->smartTrimProgress = nullptr;
			}
			impl_->smartTrim->setEnabled(true);
			impl_->smartTrim->setText(impl_->text(strings::kClipsQuickEditorSmartTrim));
			if (exitCode != 0) {
				QMessageBox::warning(this, impl_->text(strings::kClipsQuickEditorSmartTrim),
						     impl_->text(strings::kClipsQuickEditorSmartTrimFailed));
				return;
			}
			const auto output =
				QString::fromUtf8(impl_->smartTrimProcess->property("analysisOutput").toByteArray());
			const QRegularExpression startPattern(QStringLiteral("silence_start:\\s*([0-9.]+)"));
			const QRegularExpression endPattern(QStringLiteral("silence_end:\\s*([0-9.]+)"));
			std::vector<qint64> starts;
			std::vector<qint64> ends;
			for (auto match = startPattern.globalMatch(output); match.hasNext();)
				starts.push_back(static_cast<qint64>(match.next().captured(1).toDouble() * 1000.0));
			for (auto match = endPattern.globalMatch(output); match.hasNext();)
				ends.push_back(static_cast<qint64>(match.next().captured(1).toDouble() * 1000.0));
			std::vector<std::pair<qint64, qint64>> cuts;
			for (std::size_t index = 0; index < starts.size(); ++index) {
				const qint64 end = index < ends.size() ? ends[index] : impl_->durationMs;
				cuts.emplace_back(starts[index] + 100, std::max(starts[index] + 100, end - 100));
			}
			impl_->range->setSuggestedRanges(normalizeCuts(std::move(cuts)));
			refreshCutSummary();
			QMessageBox::information(this, impl_->text(strings::kClipsQuickEditorSmartTrim),
						 impl_->text(strings::kClipsQuickEditorSmartTrimDone)
							 .arg(impl_->range->suggestedRangeCount()));
		});
	connect(impl_->playbackProcess, &QProcess::readyReadStandardOutput, this, [this] {
		impl_->frameBuffer.append(impl_->playbackProcess->readAllStandardOutput());
		const qsizetype maximumBuffered = Impl::kFrameBytes * 4;
		if (impl_->frameBuffer.size() > maximumBuffered)
			impl_->frameBuffer.remove(0, impl_->frameBuffer.size() - maximumBuffered);
	});
	auto startPlaybackSegment = [this](qint64 requestedPosition) {
		const auto segments = impl_->range->visibleSegments();
		const auto next = std::find_if(
			segments.begin(), segments.end(), [requestedPosition](const TrimRange::Segment &segment) {
				return (requestedPosition >= segment.start && requestedPosition < segment.end) ||
				       segment.start >= requestedPosition;
			});
		if (next == segments.end())
			return false;
		if (impl_->playbackProcess->state() != QProcess::NotRunning) {
			impl_->playbackProcess->kill();
			impl_->playbackProcess->waitForFinished(40);
		}
		impl_->frameBuffer.clear();
		impl_->playbackPosition = std::clamp(requestedPosition, next->start, next->end - 1);
		impl_->playbackBasePosition = impl_->playbackPosition;
		impl_->playbackSegmentEnd = next->end;
		impl_->playbackClock.restart();
		impl_->range->setProperty("playbackSegmentEndMilliseconds", impl_->playbackSegmentEnd);
		impl_->range->setProperty("playbackSegmentStartCount",
					  impl_->range->property("playbackSegmentStartCount").toInt() + 1);
		impl_->playbackProcess->start(
			impl_->ffmpegExecutable,
			impl_->decodeArguments(impl_->playbackPosition,
					       impl_->playbackSegmentEnd - impl_->playbackPosition, false),
			QIODevice::ReadOnly);
		return true;
	};
	connect(impl_->playbackTimer, &QTimer::timeout, this, [this, stopPlayback, startPlaybackSegment] {
		if (impl_->frameBuffer.size() >= Impl::kFrameBytes) {
			const auto completeFrames = impl_->frameBuffer.size() / Impl::kFrameBytes;
			const auto latestOffset = (completeFrames - 1) * Impl::kFrameBytes;
			impl_->showFrame(impl_->frameBuffer.mid(latestOffset, Impl::kFrameBytes));
			impl_->frameBuffer.remove(0, completeFrames * Impl::kFrameBytes);
		}
		impl_->playbackPosition = impl_->playbackBasePosition + impl_->playbackClock.elapsed();
		if (impl_->playbackPosition >= impl_->playbackSegmentEnd) {
			const qint64 completedSegmentEnd = impl_->playbackSegmentEnd;
			if (startPlaybackSegment(completedSegmentEnd + 1)) {
				impl_->range->setPosition(impl_->playbackPosition);
				return;
			}
			stopPlayback();
			impl_->playbackPosition = impl_->range->start();
			impl_->range->setPosition(impl_->playbackPosition);
			return;
		}
		impl_->range->setPosition(impl_->playbackPosition);
	});
	connect(impl_->play, &QPushButton::clicked, this, [this, stopPlayback, startPlaybackSegment] {
		if (impl_->playing) {
			stopPlayback();
			return;
		}
		if (impl_->playbackPosition < impl_->range->start() || impl_->playbackPosition >= impl_->range->end())
			impl_->playbackPosition = impl_->range->start();
		if (!startPlaybackSegment(impl_->playbackPosition))
			return;
		impl_->playing = true;
		impl_->play->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
		impl_->play->setToolTip(impl_->text(strings::kClipsQuickEditorPause));
		impl_->play->setAccessibleName(impl_->text(strings::kClipsQuickEditorPause));
		impl_->playbackTimer->start();
	});
	auto *spaceShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
	spaceShortcut->setContext(Qt::WindowShortcut);
	connect(spaceShortcut, &QShortcut::activated, impl_->play, &QPushButton::click);
	impl_->waveformProcess->start(
		impl_->ffmpegExecutable,
		{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"), QStringLiteral("error"),
		 QStringLiteral("-i"), QString::fromStdString(impl_->clip.filePath.u8string()), QStringLiteral("-vn"),
		 QStringLiteral("-ac"), QStringLiteral("1"), QStringLiteral("-ar"), QStringLiteral("1000"),
		 QStringLiteral("-f"), QStringLiteral("s16le"), QStringLiteral("pipe:1")},
		QIODevice::ReadOnly);
	impl_->previewDebounce->start(0);
	connect(browse, &QPushButton::clicked, this, [this] {
		const auto selected = QFileDialog::getExistingDirectory(
			this, impl_->text(strings::kClipsQuickEditorChooseFolder), impl_->directory->text());
		if (!selected.isEmpty())
			impl_->directory->setText(selected);
	});
	connect(copyCaption, &QPushButton::clicked, this, [this, copyCaption] {
		QApplication::clipboard()->setText(impl_->socialCaption->toPlainText());
		copyCaption->setText(impl_->text(strings::kClipsCaptionCopied));
	});
	connect(impl_->generateCaption, &QPushButton::clicked, this, [this] {
		if (impl_->captionRequest)
			impl_->captionRequest();
	});
	connect(impl_->exportButton, &QPushButton::clicked, this, [this] {
		if (impl_->exportManager == nullptr) {
			QMessageBox::warning(this, impl_->text(strings::kClipsQuickEditorExportFailed),
					     impl_->text(strings::kClipsQuickEditorExportUnavailable));
			return;
		}
		const qint64 selectionMs = impl_->range->end() - impl_->range->start();
		ExportRequest request;
		request.clipId = impl_->clip.id + "-trim";
		request.sourcePath = impl_->clip.filePath;
		request.outputDirectory = std::filesystem::path(impl_->directory->text().toStdWString());
		request.outputBaseName =
			(impl_->clip.title.empty() ? impl_->clip.filePath.stem().string() : impl_->clip.title) +
			"_edited";
		request.orientation = impl_->clip.orientation == ClipOrientation::Vertical
					      ? ExportOrientation::Vertical
					      : ExportOrientation::Horizontal;
		// A library item marked Vertical must always leave the editor as a real
		// 9:16 file. Keeping the source frame here preserved OBS' 16:9 replay
		// canvas and produced a portrait image embedded between black bars.
		request.preserveSourceFrame = impl_->clip.orientation != ClipOrientation::Vertical;
		qint64 exportedDurationMs = selectionMs;
		if (impl_->range->removedRanges().empty()) {
			request.trimStartMilliseconds = impl_->range->start();
			request.trimDurationMilliseconds = selectionMs;
		} else {
			qint64 cursor = impl_->range->start();
			exportedDurationMs = 0;
			for (const auto &[removedStart, removedEnd] : impl_->range->removedRanges()) {
				const qint64 cutStart = std::clamp(removedStart, cursor, impl_->range->end());
				const qint64 cutEnd = std::clamp(removedEnd, cutStart, impl_->range->end());
				if (cutStart - cursor >= 100) {
					request.keepSegments.push_back({cursor, cutStart - cursor});
					exportedDurationMs += cutStart - cursor;
				}
				cursor = std::max(cursor, cutEnd);
			}
			if (impl_->range->end() - cursor >= 100) {
				request.keepSegments.push_back({cursor, impl_->range->end() - cursor});
				exportedDurationMs += impl_->range->end() - cursor;
			}
			if (request.keepSegments.empty()) {
				QMessageBox::warning(this, impl_->text(strings::kClipsQuickEditorExportFailed),
						     impl_->text(strings::kClipsQuickEditorNoContent));
				return;
			}
			request.trimDurationMilliseconds = exportedDurationMs;
		}
		request.durationSeconds = std::max(1, static_cast<int>(std::ceil(exportedDurationMs / 1000.0)));
		request.outputFps = impl_->fps->currentData().toInt();
		request.preset = static_cast<ExportQualityPreset>(impl_->quality->currentData().toInt());
		std::string error;
		const auto jobs = impl_->exportManager->enqueue(request, &error);
		if (jobs.empty()) {
			QMessageBox::warning(this, impl_->text(strings::kClipsQuickEditorExportFailed),
					     QString::fromStdString(error));
			return;
		}
		impl_->jobId = jobs.front();
		impl_->exportButton->setEnabled(false);
		impl_->cancelButton->show();
		impl_->progress->setValue(0);
		impl_->progress->show();
		impl_->status->setText(impl_->text(strings::kClipsQuickEditorExporting));
		impl_->status->setProperty("notificationTone", QStringLiteral("info"));
		impl_->status->show();
		impl_->jobTimer->start();
	});
	connect(impl_->cancelButton, &QPushButton::clicked, this, [this] {
		if (!impl_->jobId.empty()) {
			std::string ignored;
			(void)impl_->exportManager->cancel(impl_->jobId, &ignored);
		}
	});
	connect(impl_->jobTimer, &QTimer::timeout, this, [this] {
		const auto job = impl_->exportManager->job(impl_->jobId);
		impl_->progress->setValue(job.progressPercent);
		if (job.state != ExportJobState::Done && job.state != ExportJobState::Error &&
		    job.state != ExportJobState::Cancelled)
			return;
		impl_->jobTimer->stop();
		impl_->exportButton->setEnabled(true);
		impl_->cancelButton->hide();
		if (job.state == ExportJobState::Done) {
			const QString outputPath = QString::fromStdString(job.outputPath.u8string());
			const QFileInfo outputInfo(outputPath);
			const QString selectionDuration = formatTime(job.request.trimDurationMilliseconds);
			impl_->status->setProperty("notificationTone", QStringLiteral("success"));
			impl_->status->setText(impl_->text(strings::kClipsQuickEditorExportDone).arg(outputPath));
			impl_->progress->setValue(100);

			QDialog completion(this);
			completion.setObjectName(QStringLiteral("quickEditorExportCompleteDialog"));
			completion.setWindowTitle(impl_->text(strings::kClipsQuickEditorExportCompleteTitle));
			completion.setModal(true);
			completion.setMinimumSize(700, 330);

			auto *completionLayout = new QVBoxLayout(&completion);
			completionLayout->setContentsMargins(28, 26, 28, 26);
			completionLayout->setSpacing(16);

			auto *completionTitle =
				new QLabel(impl_->text(strings::kClipsQuickEditorExportCompleteTitle), &completion);
			completionTitle->setObjectName(QStringLiteral("quickEditorExportCompleteTitle"));
			completionTitle->setProperty("heading", true);
			completionLayout->addWidget(completionTitle);

			auto *completionMessage =
				new QLabel(impl_->text(strings::kClipsQuickEditorExportCompleteMessage)
						   .arg(selectionDuration, outputInfo.fileName()),
					   &completion);
			completionMessage->setObjectName(QStringLiteral("quickEditorExportCompleteMessage"));
			completionMessage->setTextFormat(Qt::PlainText);
			completionMessage->setTextInteractionFlags(Qt::TextSelectableByMouse);
			completionMessage->setWordWrap(true);
			completionLayout->addWidget(completionMessage);

			auto *locationLabel =
				new QLabel(impl_->text(strings::kClipsQuickEditorFileLocation), &completion);
			locationLabel->setProperty("fieldLabel", true);
			completionLayout->addWidget(locationLabel);

			auto *locationRow = new QHBoxLayout();
			locationRow->setSpacing(10);
			auto *location = new QLineEdit(QDir::toNativeSeparators(outputPath), &completion);
			location->setObjectName(QStringLiteral("quickEditorExportPath"));
			location->setReadOnly(true);
			location->setCursorPosition(0);
			location->setMinimumHeight(40);
			auto *copyPath = new QPushButton(impl_->text(strings::kClipsQuickEditorCopyPath), &completion);
			copyPath->setMinimumHeight(40);
			locationRow->addWidget(location, 1);
			locationRow->addWidget(copyPath);
			completionLayout->addLayout(locationRow);
			completionLayout->addStretch(1);

			auto *footer = new QHBoxLayout();
			footer->setSpacing(12);
			footer->addStretch(1);
			auto *viewFile = new QPushButton(impl_->text(strings::kClipsQuickEditorViewFile), &completion);
			auto *openFolder =
				new QPushButton(impl_->text(strings::kClipsQuickEditorOpenFolder), &completion);
			auto *closeButton = new QPushButton(impl_->text(strings::kClipsQuickEditorClose), &completion);
			for (auto *button : {viewFile, openFolder, closeButton}) {
				button->setMinimumHeight(40);
				button->setMinimumWidth(140);
				footer->addWidget(button);
			}
			completionLayout->addLayout(footer);

			connect(copyPath, &QPushButton::clicked, &completion, [outputPath] {
				QApplication::clipboard()->setText(QDir::toNativeSeparators(outputPath));
			});
			connect(viewFile, &QPushButton::clicked, &completion,
				[outputPath] { (void)QDesktopServices::openUrl(QUrl::fromLocalFile(outputPath)); });
			connect(openFolder, &QPushButton::clicked, &completion, [outputPath, outputInfo] {
#if defined(Q_OS_WIN)
				if (!QProcess::startDetached(QStringLiteral("explorer.exe"),
							     {QStringLiteral("/select,"),
							      QDir::toNativeSeparators(outputPath)}))
					(void)QDesktopServices::openUrl(QUrl::fromLocalFile(outputInfo.absolutePath()));
#else
				(void)QDesktopServices::openUrl(QUrl::fromLocalFile(outputInfo.absolutePath()));
#endif
			});
			connect(closeButton, &QPushButton::clicked, &completion, &QDialog::accept);
			completion.exec();
		} else {
			impl_->status->setProperty("notificationTone", QStringLiteral("error"));
			impl_->status->setText(job.state == ExportJobState::Cancelled
						       ? impl_->text(strings::kClipsQuickEditorExportCancelled)
						       : impl_->text(strings::kClipsQuickEditorExportError)
								 .arg(QString::fromStdString(job.error)));
		}
		impl_->status->style()->unpolish(impl_->status);
		impl_->status->style()->polish(impl_->status);
	});

	QStringList hashtags;
	for (const auto &hashtag : impl_->clip.hashtags)
		hashtags.push_back(QString::fromStdString(hashtag));
	setCaption(QString::fromStdString(impl_->clip.caption));
}

QuickClipEditorDialog::~QuickClipEditorDialog()
{
	if (impl_ != nullptr) {
		if (impl_->playbackTimer != nullptr)
			impl_->playbackTimer->stop();
		if (impl_->previewDebounce != nullptr)
			impl_->previewDebounce->stop();
		for (auto *process :
		     {impl_->playbackProcess, impl_->previewProcess, impl_->waveformProcess, impl_->smartTrimProcess}) {
			if (process == nullptr)
				continue;
			process->disconnect(this);
			if (process->state() != QProcess::NotRunning) {
				process->kill();
				process->waitForFinished(1000);
			}
		}
	}
	delete impl_;
}

void QuickClipEditorDialog::setCaptionRequest(CaptionRequest callback)
{
	impl_->captionRequest = std::move(callback);
	impl_->generateCaption->setEnabled(static_cast<bool>(impl_->captionRequest));
}

void QuickClipEditorDialog::setCaption(const QString &socialCaption, const QString &youtubeShortsCaption)
{
	impl_->socialCaption->setPlainText(socialCaption);
	impl_->shortsCaption->setPlainText(youtubeShortsCaption);
	setCaptionBusy(false);
}

void QuickClipEditorDialog::setCaptionBusy(bool busy)
{
	impl_->generateCaption->setEnabled(!busy && static_cast<bool>(impl_->captionRequest));
	impl_->generateCaption->setText(busy ? impl_->text(strings::kClipsCaptionGenerating)
					     : impl_->text(strings::kClipsQuickEditorGenerateCaption));
	if (!busy) {
		if (impl_->captionTimeoutTimer != nullptr)
			impl_->captionTimeoutTimer->stop();
		if (impl_->captionProgressDialog != nullptr) {
			impl_->captionProgressDialog->hide();
			impl_->captionProgressDialog->deleteLater();
			impl_->captionProgressDialog = nullptr;
			impl_->captionProgressStatus = nullptr;
			impl_->captionProgressPercent = nullptr;
			impl_->captionProgressEta = nullptr;
			impl_->captionProgressBar = nullptr;
		}
		return;
	}
	if (impl_->captionProgressDialog != nullptr)
		return;

	auto *dialog = new QDialog(this);
	dialog->setObjectName(QStringLiteral("quickEditorCaptionProgressDialog"));
	dialog->setWindowTitle(impl_->text(strings::kClipsCaptionProgressTitle));
	dialog->setWindowModality(Qt::WindowModal);
	dialog->setModal(true);
	dialog->setWindowFlag(Qt::WindowCloseButtonHint, false);
	dialog->setMinimumSize(540, 250);
	auto *layout = new QVBoxLayout(dialog);
	layout->setContentsMargins(tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl, tokens::kSpaceXl);
	layout->setSpacing(tokens::kSpaceMd);
	auto *title = new QLabel(impl_->text(strings::kClipsCaptionProgressTitle), dialog);
	title->setProperty("textRole", QStringLiteral("sectionTitle"));
	layout->addWidget(title);
	impl_->captionProgressStatus = new QLabel(impl_->text(strings::kClipsCaptionValidatingLicense), dialog);
	impl_->captionProgressStatus->setObjectName(QStringLiteral("quickEditorCaptionProgressStatus"));
	impl_->captionProgressStatus->setWordWrap(true);
	impl_->captionProgressStatus->setMinimumHeight(44);
	layout->addWidget(impl_->captionProgressStatus);
	impl_->captionProgressPercent = new QLabel(QStringLiteral("1%"), dialog);
	impl_->captionProgressPercent->setObjectName(QStringLiteral("quickEditorCaptionProgressPercent"));
	impl_->captionProgressPercent->setAlignment(Qt::AlignCenter);
	impl_->captionProgressPercent->setProperty("textRole", QStringLiteral("heroValue"));
	layout->addWidget(impl_->captionProgressPercent);
	impl_->captionProgressBar = new QProgressBar(dialog);
	impl_->captionProgressBar->setObjectName(QStringLiteral("quickEditorCaptionProgressBar"));
	impl_->captionProgressBar->setRange(0, 100);
	impl_->captionProgressBar->setValue(1);
	impl_->captionProgressBar->setTextVisible(false);
	impl_->captionProgressBar->setMinimumHeight(12);
	layout->addWidget(impl_->captionProgressBar);
	impl_->captionProgressEta = new QLabel(impl_->text(strings::kClipsCaptionStillWorking), dialog);
	impl_->captionProgressEta->setObjectName(QStringLiteral("quickEditorCaptionProgressEta"));
	impl_->captionProgressEta->setWordWrap(true);
	impl_->captionProgressEta->setAlignment(Qt::AlignCenter);
	impl_->captionProgressEta->setProperty("textRole", QStringLiteral("muted"));
	layout->addWidget(impl_->captionProgressEta);

	impl_->captionProgressDialog = dialog;
	if (impl_->captionTimeoutTimer == nullptr) {
		impl_->captionTimeoutTimer = new QTimer(this);
		impl_->captionTimeoutTimer->setObjectName(QStringLiteral("quickEditorCaptionTimeoutTimer"));
		impl_->captionTimeoutTimer->setSingleShot(true);
		connect(impl_->captionTimeoutTimer, &QTimer::timeout, this,
			[this] { setCaptionError(impl_->text(strings::kClipsQuickEditorCaptionTimeout)); });
	}
	impl_->captionTimeoutTimer->start(360'000);
	dialog->open();
}

void QuickClipEditorDialog::setCaptionProgress(const CaptionGenerationProgress &progress)
{
	if (impl_->captionProgressDialog == nullptr)
		setCaptionBusy(true);
	const int percentage = std::clamp(progress.percentage, 0, 100);
	if (impl_->captionProgressBar != nullptr)
		impl_->captionProgressBar->setValue(percentage);
	if (impl_->captionProgressPercent != nullptr)
		impl_->captionProgressPercent->setText(QStringLiteral("%1%").arg(percentage));
	if (impl_->captionProgressStatus != nullptr && !progress.status.trimmed().isEmpty())
		impl_->captionProgressStatus->setText(progress.status);
	if (impl_->captionProgressEta != nullptr) {
		impl_->captionProgressEta->setText(
			progress.estimatedSecondsRemaining > 0
				? impl_->text(strings::kClipsCaptionEtaSeconds).arg(progress.estimatedSecondsRemaining)
				: impl_->text(percentage >= 80 ? strings::kClipsCaptionAlmostThere
							       : strings::kClipsCaptionStillWorking));
	}
}

void QuickClipEditorDialog::setCaptionError(const QString &message)
{
	setCaptionBusy(false);
	QMessageBox::warning(this, impl_->text(strings::kClipsCaptionFailed),
			     message.trimmed().isEmpty() ? impl_->text(strings::kClipsCaptionFailed) : message);
}

} // namespace clipcoach::ui
