#pragma once

#include <clipcoach/core/vertical-canvas.hpp>

#include <QWidget>

namespace clipcoach::ui {

class VerticalPreviewWidget final : public QWidget {
public:
	explicit VerticalPreviewWidget(QWidget *parent = nullptr);

	void setCanvasSettings(VerticalCanvasSettings settings);
	[[nodiscard]] QSize sizeHint() const override;
	[[nodiscard]] int heightForWidth(int width) const override;
	[[nodiscard]] bool hasHeightForWidth() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	VerticalCanvasSettings settings_;
};

} // namespace clipcoach::ui

