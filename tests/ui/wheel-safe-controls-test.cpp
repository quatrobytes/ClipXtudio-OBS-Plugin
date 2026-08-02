#include <clipcoach/ui/components/wheel-safe-controls.hpp>

#include "../unit/test-support.hpp"

#include <QApplication>
#include <QWheelEvent>

namespace {

QWheelEvent wheelEvent()
{
	return QWheelEvent(QPointF(10, 10), QPointF(10, 10), QPoint(0, 0),
			   QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
			   Qt::ScrollUpdate, false);
}

} // namespace

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication app(argc, argv);
	using namespace clipcoach::ui;

	WheelSafeSlider slider(Qt::Horizontal);
	slider.setRange(0, 100);
	slider.setValue(50);
	auto sliderWheel = wheelEvent();
	QApplication::sendEvent(&slider, &sliderWheel);
	clipcoach::test::expect(
		slider.value() == 50,
		"Mouse wheel must not change a slider value");
	clipcoach::test::expect(
		!sliderWheel.isAccepted(),
		"Slider wheel input must remain available to its scroll parent");

	WheelSafeSpinBox spin;
	spin.setRange(0, 100);
	spin.setValue(50);
	auto spinWheel = wheelEvent();
	QApplication::sendEvent(&spin, &spinWheel);
	clipcoach::test::expect(
		spin.value() == 50,
		"Mouse wheel must not change a spin box value");
	clipcoach::test::expect(
		!spinWheel.isAccepted(),
		"Spin box wheel input must remain available to its scroll parent");

	WheelSafeComboBox combo;
	combo.addItems({QStringLiteral("First"), QStringLiteral("Second")});
	combo.setCurrentIndex(0);
	auto comboWheel = wheelEvent();
	QApplication::sendEvent(&combo, &comboWheel);
	clipcoach::test::expect(
		combo.currentIndex() == 0,
		"Mouse wheel must not change a combo box selection");
	clipcoach::test::expect(
		!comboWheel.isAccepted(),
		"Combo box wheel input must remain available to its scroll parent");

	return 0;
}
