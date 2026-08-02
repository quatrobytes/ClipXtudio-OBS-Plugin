#include "test-support.hpp"

#include <clipcoach/core/clip-manager.hpp>

#include <chrono>

int main()
{
	using namespace std::chrono;
	using clipcoach::test::expect;

	const auto fixedTime = system_clock::from_time_t(1785247509);
	const auto fileName = clipcoach::ClipManager::createReadableFileName(fixedTime, "manual", ".mp4");

	expect(fileName == "ClipX_2026-07-28_14-05-09_manual.mp4",
	       "clip file name must be stable, readable and filesystem-safe");

	return clipcoach::test::pass("clip-filename-test");
}
