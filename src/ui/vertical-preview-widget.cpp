#include <clipcoach/ui/vertical-preview-widget.hpp>

#include <clipcoach/ui/design-tokens.hpp>

#include <QPainter>
#include <QPaintEvent>

namespace clipcoach::ui {

VerticalPreviewWidget::VerticalPreviewWidget(QWidget *parent)
	: QWidget(parent)
{
	setObjectName(QStringLiteral("verticalCanvasPreview"));
	setMinimumSize(180, 320);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
}

void VerticalPreviewWidget::setCanvasSettings(VerticalCanvasSettings settings)
{
	settings_ = std::move(settings);
	update();
}

QSize VerticalPreviewWidget::sizeHint() const
{
	return {225, 400};
}

int VerticalPreviewWidget::heightForWidth(int width) const
{
	return width * 16 / 9;
}

bool VerticalPreviewWidget::hasHeightForWidth() const
{
	return true;
}

void VerticalPreviewWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.fillRect(rect(), QColor(tokens::kSurface));

	const int availableHeight = height() - 16;
	const int availableWidth = width() - 16;
	int canvasHeight = availableHeight;
	int canvasWidth = canvasHeight * 9 / 16;
	if (canvasWidth > availableWidth) {
		canvasWidth = availableWidth;
		canvasHeight = canvasWidth * 16 / 9;
	}
	const QRectF canvas((width() - canvasWidth) / 2.0,
			    (height() - canvasHeight) / 2.0, canvasWidth,
			    canvasHeight);
	painter.setPen(QPen(QColor(tokens::kBorder), 2));
	painter.setBrush(QColor(tokens::kBackground));
	painter.drawRoundedRect(canvas, tokens::kRadiusMd, tokens::kRadiusMd);

	static const QColor colors[6] = {
		QColor("#4F46E5"), QColor("#7C3AED"), QColor("#16A34A"),
		QColor("#2563EB"), QColor("#D97706"), QColor("#DB2777"),
	};
	for (const auto &item : settings_.elements) {
		if (!item.enabled) {
			continue;
		}
		const QRectF elementRect(
			canvas.x() + item.x * canvas.width(),
			canvas.y() + item.y * canvas.height(),
			item.width * canvas.width(),
			item.height * canvas.height());
		auto color = colors[static_cast<int>(item.type)];
		color.setAlpha(145);
		painter.setPen(QPen(color.lighter(145), 1));
		painter.setBrush(color);
		painter.drawRoundedRect(elementRect, tokens::kRadiusSm,
					tokens::kRadiusSm);
	}
}

} // namespace clipcoach::ui

