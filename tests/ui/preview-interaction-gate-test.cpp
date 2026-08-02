#include <clipcoach/ui/components/preview-interaction-gate.hpp>

#include "../unit/test-support.hpp"

int main()
{
	clipcoach::ui::PreviewInteractionGate gate;

	clipcoach::test::expect(!gate.isActive() && !gate.acceptsWheel(),
				"Vertical preview wheel interaction must start disabled");

	gate.activate();
	clipcoach::test::expect(gate.isActive() && gate.acceptsWheel(),
				"A click must activate vertical preview wheel interaction");

	gate.deactivate();
	clipcoach::test::expect(!gate.isActive() && !gate.acceptsWheel(),
				"Leaving the preview must return the wheel to page scrolling");

	return 0;
}
