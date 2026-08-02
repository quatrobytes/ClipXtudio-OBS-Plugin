#pragma once

#include <QComboBox>
#include <QSlider>
#include <QSpinBox>
#include <QWheelEvent>

namespace clipcoach::ui {

// These controls deliberately ignore wheel input so that scrolling a page
// cannot modify a setting accidentally. Values remain editable through click,
// drag, arrow buttons and the keyboard.
class WheelSafeSlider final : public QSlider {
public:
	explicit WheelSafeSlider(Qt::Orientation orientation, QWidget *parent = nullptr) : QSlider(orientation, parent)
	{
	}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

class WheelSafeSpinBox final : public QSpinBox {
public:
	explicit WheelSafeSpinBox(QWidget *parent = nullptr) : QSpinBox(parent) {}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

class WheelSafeComboBox final : public QComboBox {
public:
	explicit WheelSafeComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
	void wheelEvent(QWheelEvent *event) override { event->ignore(); }
};

} // namespace clipcoach::ui
