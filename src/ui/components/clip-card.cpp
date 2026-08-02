#include <clipcoach/ui/components/clip-card.hpp>
#include <clipcoach/ui/components/score-badge.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QFileInfo>
#include <QCheckBox>
#include <QFontMetrics>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QPixmap>
#include <QResizeEvent>
#include <QStyle>
#include <QTimer>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace clipcoach::ui {
namespace {

enum class ClipActionIcon {
	Preview,
	Edit,
	Vertical,
	Caption,
	Subtitles,
	Folder,
	Delete,
};

QIcon actionIcon(ClipActionIcon type)
{
	QPixmap pixmap(20, 20);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	QPen pen(QColor(QStringLiteral("#E8EDF5")));
	if (type == ClipActionIcon::Delete)
		pen.setColor(QColor(QStringLiteral("#FF6B78")));
	pen.setWidthF(1.6);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	painter.setPen(pen);
	painter.setBrush(Qt::NoBrush);

	switch (type) {
	case ClipActionIcon::Preview: {
		QPainterPath path;
		path.moveTo(7, 5);
		path.lineTo(15, 10);
		path.lineTo(7, 15);
		path.closeSubpath();
		painter.setBrush(QColor(QStringLiteral("#E8EDF5")));
		painter.drawPath(path);
		break;
	}
	case ClipActionIcon::Edit:
		painter.drawLine(QPointF(5, 15), QPointF(7, 10));
		painter.drawLine(QPointF(7, 10), QPointF(13.5, 3.5));
		painter.drawLine(QPointF(13.5, 3.5), QPointF(16.5, 6.5));
		painter.drawLine(QPointF(16.5, 6.5), QPointF(10, 13));
		painter.drawLine(QPointF(10, 13), QPointF(5, 15));
		painter.drawLine(QPointF(7, 10), QPointF(10, 13));
		break;
	case ClipActionIcon::Vertical:
		painter.drawRoundedRect(QRectF(6, 3, 8, 14), 1.5, 1.5);
		painter.drawLine(QPointF(10, 6), QPointF(10, 13));
		painter.drawLine(QPointF(7.5, 10.5), QPointF(10, 13));
		painter.drawLine(QPointF(12.5, 10.5), QPointF(10, 13));
		break;
	case ClipActionIcon::Caption:
		painter.drawRoundedRect(QRectF(3, 4, 14, 10), 2, 2);
		painter.drawLine(QPointF(7, 14), QPointF(5, 17));
		painter.drawLine(QPointF(7, 8), QPointF(13, 8));
		painter.drawLine(QPointF(7, 11), QPointF(11, 11));
		break;
	case ClipActionIcon::Subtitles:
		painter.drawRoundedRect(QRectF(3, 4, 14, 12), 2, 2);
		painter.drawLine(QPointF(6, 10), QPointF(14, 10));
		painter.drawLine(QPointF(6, 13), QPointF(11, 13));
		break;
	case ClipActionIcon::Folder: {
		QPainterPath path;
		path.moveTo(3, 6);
		path.lineTo(8, 6);
		path.lineTo(10, 8);
		path.lineTo(17, 8);
		path.lineTo(16, 16);
		path.lineTo(3, 16);
		path.closeSubpath();
		painter.drawPath(path);
		break;
	}
	case ClipActionIcon::Delete:
		painter.drawRoundedRect(QRectF(6, 6, 8, 10), 1, 1);
		painter.drawLine(QPointF(5, 5), QPointF(15, 5));
		painter.drawLine(QPointF(8, 3), QPointF(12, 3));
		painter.drawLine(QPointF(8.5, 8), QPointF(8.5, 13.5));
		painter.drawLine(QPointF(11.5, 8), QPointF(11.5, 13.5));
		break;
	}
	return QIcon(pixmap);
}

QIcon favoriteIcon(bool filled)
{
	QPixmap pixmap(20, 20);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing, true);
	QPen pen(QColor(QStringLiteral("#D8C7FF")));
	pen.setWidthF(1.5);
	pen.setJoinStyle(Qt::RoundJoin);
	painter.setPen(pen);
	painter.setBrush(filled ? QColor(QStringLiteral("#A78BFA")) : Qt::NoBrush);
	QPainterPath star;
	star.moveTo(10, 2.5);
	star.lineTo(12.3, 7.2);
	star.lineTo(17.5, 7.9);
	star.lineTo(13.7, 11.6);
	star.lineTo(14.7, 16.8);
	star.lineTo(10, 14.3);
	star.lineTo(5.3, 16.8);
	star.lineTo(6.3, 11.6);
	star.lineTo(2.5, 7.9);
	star.lineTo(7.7, 7.2);
	star.closeSubpath();
	painter.drawPath(star);
	return QIcon(pixmap);
}

} // namespace

class ClipCard::Impl final {
public:
	QGridLayout *layout{nullptr};
	QWidget *thumbnail{nullptr};
	QWidget *details{nullptr};
	QWidget *actions{nullptr};
	QWidget *scoreStatus{nullptr};
	ScoreBadge *score{nullptr};
	QLabel *pendingDot{nullptr};
	QTimer *pendingPulseTimer{nullptr};
	QPushButton *favorite{nullptr};
	QPushButton *preview{nullptr};
	QPushButton *edit{nullptr};
	QPushButton *exportVertical{nullptr};
	QPushButton *caption{nullptr};
	QPushButton *subtitles{nullptr};
	QPushButton *folder{nullptr};
	QPushButton *deleteClip{nullptr};
	QCheckBox *selected{nullptr};
	bool compact{false};
};

ClipCard::ClipCard(const QString &title, const QString &metadata, int score, QWidget *parent)
	: QFrame(parent)
{
	setObjectName(QStringLiteral("ClipCard"));
	setFrameShape(QFrame::NoFrame);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(tokens::kSpaceMd, tokens::kSpaceMd, tokens::kSpaceMd,
				   tokens::kSpaceMd);
	layout->setSpacing(tokens::kSpaceMd);

	auto *thumbnail = new QFrame(this);
	thumbnail->setObjectName(QStringLiteral("ClipThumbnail"));
	thumbnail->setFixedSize(72, 44);

	auto *copyLayout = new QVBoxLayout();
	copyLayout->setSpacing(tokens::kSpaceXs);
	auto *titleLabel = new QLabel(title, this);
	titleLabel->setObjectName(QStringLiteral("SectionLabel"));
	auto *metadataLabel = new QLabel(metadata, this);
	metadataLabel->setObjectName(QStringLiteral("SupportingText"));
	copyLayout->addWidget(titleLabel);
	copyLayout->addWidget(metadataLabel);

	layout->addWidget(thumbnail);
	layout->addLayout(copyLayout, 1);
	if (score >= 0) {
		layout->addWidget(new ScoreBadge(score, this), 0, Qt::AlignTop);
	}
}

ClipCard::ClipCard(ClipCardViewData data, QWidget *parent)
	: QFrame(parent), impl_(new Impl)
{
	setObjectName(QStringLiteral("ClipCard"));
	setProperty("libraryRole", QStringLiteral("persistedClip"));
	setFrameShape(QFrame::NoFrame);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	setMinimumHeight(82);

	impl_->layout = new QGridLayout(this);
	impl_->layout->setContentsMargins(tokens::kSpaceSm, tokens::kSpaceSm,
					 tokens::kSpaceSm, tokens::kSpaceSm);
	impl_->layout->setHorizontalSpacing(tokens::kSpaceSm);
	impl_->layout->setVerticalSpacing(tokens::kSpaceXs);
	impl_->selected = new QCheckBox(this);
	impl_->selected->setObjectName(QStringLiteral("clipSelectionCheck"));
	impl_->selected->setChecked(data.selected);

	impl_->thumbnail = new QFrame(this);
	impl_->thumbnail->setObjectName(QStringLiteral("ClipThumbnail"));
	impl_->thumbnail->setFixedSize(112, 64);
	auto *thumbnailLayout = new QGridLayout(impl_->thumbnail);
	thumbnailLayout->setContentsMargins(0, 0, 0, 0);
	auto *thumbnailImage = new QLabel(impl_->thumbnail);
	thumbnailImage->setObjectName(QStringLiteral("clipThumbnailImage"));
	thumbnailImage->setFixedSize(impl_->thumbnail->size());
	thumbnailImage->setAlignment(Qt::AlignCenter);
	const QPixmap image(data.thumbnailPath);
	if (!image.isNull()) {
		const auto scaled = image.scaled(
			impl_->thumbnail->size(), Qt::KeepAspectRatioByExpanding,
			Qt::SmoothTransformation);
		const auto x = std::max(0, (scaled.width() - impl_->thumbnail->width()) / 2);
		const auto y = std::max(0, (scaled.height() - impl_->thumbnail->height()) / 2);
		thumbnailImage->setPixmap(
			scaled.copy(x, y, impl_->thumbnail->width(), impl_->thumbnail->height()));
	} else {
		thumbnailImage->setText(QStringLiteral("\u25B6"));
		thumbnailImage->setProperty("thumbnailState",
					    QStringLiteral("placeholder"));
	}
	auto *duration = new QLabel(data.duration, impl_->thumbnail);
	duration->setObjectName(QStringLiteral("clipDurationBadge"));
	duration->setAlignment(Qt::AlignCenter);
	duration->setProperty("badgeRole", QStringLiteral("duration"));
	duration->setContentsMargins(tokens::kSpaceXs, 1, tokens::kSpaceXs, 1);
	thumbnailLayout->addWidget(thumbnailImage, 0, 0);
	thumbnailLayout->addWidget(duration, 0, 0, Qt::AlignRight | Qt::AlignBottom);

	impl_->details = new QWidget(this);
	impl_->details->setObjectName(QStringLiteral("clipDetails"));
	impl_->details->setMinimumWidth(150);
	impl_->details->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	auto *details = new QVBoxLayout(impl_->details);
	details->setContentsMargins(0, 0, 0, 0);
	details->setSpacing(tokens::kSpaceXs);
	auto *title = new QLabel(data.title, this);
	title->setObjectName(QStringLiteral("SectionLabel"));
	title->setWordWrap(false);
	title->setText(title->fontMetrics().elidedText(data.title, Qt::ElideRight, 420));
	title->setToolTip(data.title);
	auto *date = new QLabel(data.dateTime, this);
	date->setObjectName(QStringLiteral("SupportingText"));
	auto *trigger = new QLabel(data.trigger, this);
	trigger->setObjectName(QStringLiteral("TriggerBadge"));
	auto *tags = new QHBoxLayout();
	tags->setContentsMargins(0, 0, 0, 0);
	tags->setSpacing(tokens::kSpaceXs);
	tags->addWidget(trigger);
	if (!data.orientation.isEmpty()) {
		auto *orientation = new QLabel(data.orientation, this);
		orientation->setObjectName(QStringLiteral("clipOrientationBadge"));
		tags->addWidget(orientation);
	}
	tags->addStretch(1);
	details->addWidget(title);
	details->addWidget(date);
	details->addLayout(tags);

	impl_->favorite = new QPushButton(this);
	impl_->favorite->setObjectName(QStringLiteral("clipFavoriteButton"));
	impl_->favorite->setProperty("controlRole", QStringLiteral("icon"));
	impl_->favorite->setCheckable(true);
	impl_->favorite->setChecked(data.favorite);
	impl_->favorite->setToolTip(data.favoriteTooltip);
	impl_->favorite->setAccessibleName(data.favoriteTooltip);
	impl_->favorite->setFixedSize(28, 28);
	impl_->favorite->setIcon(favoriteIcon(data.favorite));
	impl_->favorite->setIconSize(QSize(18, 18));
	connect(impl_->favorite, &QPushButton::toggled, this,
		[this](bool checked) {
			impl_->favorite->setIcon(favoriteIcon(checked));
		});

	auto makeAction = [this](const QString &name, const QString &tooltip,
				 ClipActionIcon icon) {
		auto *button = new QPushButton(this);
		button->setObjectName(name);
		button->setProperty("controlRole", QStringLiteral("compact"));
		button->setProperty("clipAction", true);
		button->setToolTip(tooltip);
		button->setAccessibleName(tooltip);
		button->setFixedSize(34, 30);
		button->setIcon(actionIcon(icon));
		button->setIconSize(QSize(18, 18));
		return button;
	};
	impl_->preview = makeAction(QStringLiteral("clipPreviewButton"),
				    data.previewLabel, ClipActionIcon::Preview);
	impl_->edit = makeAction(QStringLiteral("clipQuickEditorButton"),
			       data.editLabel, ClipActionIcon::Edit);
	impl_->exportVertical = makeAction(QStringLiteral("clipExportVerticalButton"),
					   data.exportLabel, ClipActionIcon::Vertical);
	impl_->caption = makeAction(QStringLiteral("clipCaptionButton"),
				    data.captionLabel, ClipActionIcon::Caption);
	impl_->subtitles = makeAction(QStringLiteral("clipSubtitlesButton"),
				      data.subtitlesLabel, ClipActionIcon::Subtitles);
	impl_->folder = makeAction(QStringLiteral("clipOpenFolderButton"),
				   data.openFolderLabel, ClipActionIcon::Folder);
	impl_->deleteClip = makeAction(QStringLiteral("clipDeleteButton"),
				       data.deleteLabel, ClipActionIcon::Delete);
	impl_->deleteClip->setProperty("destructiveAction", true);
	impl_->caption->setEnabled(data.captionAvailable);
	impl_->subtitles->setEnabled(data.subtitlesAvailable);
	impl_->exportVertical->setEnabled(data.exportVerticalEnabled);

	impl_->actions = new QWidget(this);
	impl_->actions->setObjectName(QStringLiteral("clipActions"));
	auto *actions = new QHBoxLayout(impl_->actions);
	actions->setContentsMargins(0, 0, 0, 0);
	actions->setSpacing(2);
	actions->addWidget(impl_->preview);
	actions->addWidget(impl_->edit);
	actions->addWidget(impl_->exportVertical);
	actions->addWidget(impl_->caption);
	actions->addWidget(impl_->subtitles);
	actions->addWidget(impl_->folder);
	actions->addWidget(impl_->favorite);
	actions->addWidget(impl_->deleteClip);

	impl_->score = new ScoreBadge(data.score, this);
	impl_->score->setLabel(data.scoreLabel);
	impl_->scoreStatus = new QWidget(this);
	auto *scoreStatusLayout = new QHBoxLayout(impl_->scoreStatus);
	scoreStatusLayout->setContentsMargins(0, 0, 0, 0);
	scoreStatusLayout->setSpacing(tokens::kSpaceSm);
	impl_->pendingDot = new QLabel(impl_->scoreStatus);
	impl_->pendingDot->setObjectName(QStringLiteral("clipPendingDot"));
	impl_->pendingDot->setFixedSize(10, 10);
	impl_->pendingDot->setProperty("pulseOn", true);
	impl_->pendingDot->setToolTip(data.processingLabel);
	impl_->pendingDot->setAccessibleName(data.processingLabel);
	impl_->pendingDot->setVisible(data.processing);
	scoreStatusLayout->addWidget(impl_->pendingDot, 0, Qt::AlignVCenter);
	scoreStatusLayout->addWidget(impl_->score, 0, Qt::AlignVCenter);
	if (data.processing) {
		impl_->pendingPulseTimer = new QTimer(impl_->scoreStatus);
		impl_->pendingPulseTimer->setInterval(420);
		connect(impl_->pendingPulseTimer, &QTimer::timeout, this,
			[this] {
				const bool on = !impl_->pendingDot
							->property("pulseOn")
							.toBool();
				impl_->pendingDot->setProperty("pulseOn", on);
				impl_->pendingDot->style()->unpolish(
					impl_->pendingDot);
				impl_->pendingDot->style()->polish(
					impl_->pendingDot);
			});
		impl_->pendingPulseTimer->start();
	}
	updateResponsiveLayout();
}

ClipCard::~ClipCard()
{
	delete impl_;
}

void ClipCard::setFavoriteCallback(std::function<void(bool)> callback)
{
	connect(impl_->favorite, &QPushButton::toggled, this, std::move(callback));
}

void ClipCard::setPreviewCallback(std::function<void()> callback)
{
	connect(impl_->preview, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setEditCallback(std::function<void()> callback)
{
	connect(impl_->edit, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setExportCallback(std::function<void()> callback)
{
	connect(impl_->exportVertical, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setCaptionCallback(std::function<void()> callback)
{
	connect(impl_->caption, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setCaptionBusy(bool busy, const QString &tooltip)
{
	impl_->caption->setEnabled(!busy);
	impl_->caption->setProperty("busy", busy);
	if (!tooltip.isEmpty()) {
		impl_->caption->setToolTip(tooltip);
		impl_->caption->setAccessibleName(tooltip);
	}
	impl_->caption->style()->unpolish(impl_->caption);
	impl_->caption->style()->polish(impl_->caption);
}

void ClipCard::setSubtitlesCallback(std::function<void()> callback)
{
	connect(impl_->subtitles, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setOpenFolderCallback(std::function<void()> callback)
{
	connect(impl_->folder, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setDeleteCallback(std::function<void()> callback)
{
	connect(impl_->deleteClip, &QPushButton::clicked, this, std::move(callback));
}

void ClipCard::setSelectionCallback(std::function<void(bool)> callback)
{
	connect(impl_->selected, &QCheckBox::toggled, this,
		std::move(callback));
}

void ClipCard::resizeEvent(QResizeEvent *event)
{
	QFrame::resizeEvent(event);
	updateResponsiveLayout();
}

void ClipCard::updateResponsiveLayout()
{
	if (impl_ == nullptr || impl_->layout == nullptr)
		return;
	const bool compact = width() < 760;
	if (compact == impl_->compact && impl_->layout->indexOf(impl_->selected) >= 0)
		return;
	impl_->compact = compact;

	for (auto *widget : {static_cast<QWidget *>(impl_->selected), impl_->thumbnail,
			     impl_->details, impl_->scoreStatus,
			     impl_->actions})
		impl_->layout->removeWidget(widget);

	impl_->layout->addWidget(impl_->selected, 0, 0, 1, 1, Qt::AlignVCenter);
	impl_->layout->addWidget(impl_->thumbnail, 0, 1, 1, 1, Qt::AlignVCenter);
	impl_->layout->addWidget(impl_->details, 0, 2, 1, 1, Qt::AlignVCenter);
	impl_->layout->setColumnStretch(2, 1);
	if (compact) {
		setMinimumHeight(122);
		setMaximumHeight(132);
		impl_->layout->addWidget(impl_->scoreStatus, 0, 3, 1, 1, Qt::AlignVCenter);
		impl_->layout->addWidget(impl_->actions, 1, 1, 1, 3, Qt::AlignRight);
	} else {
		setMinimumHeight(82);
		setMaximumHeight(92);
		impl_->layout->addWidget(impl_->scoreStatus, 0, 3, 1, 1, Qt::AlignVCenter);
		impl_->layout->addWidget(impl_->actions, 0, 4, 1, 1, Qt::AlignVCenter);
	}
}

} // namespace clipcoach::ui
