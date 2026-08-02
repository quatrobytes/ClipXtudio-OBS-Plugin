#include "test-support.hpp"

#include <clipcoach/core/clip-manager.hpp>

int main()
{
	using clipcoach::ClipManager;
	using clipcoach::test::expect;

	expect(ClipManager::isValidDuration(15), "15 seconds must be valid");
	expect(ClipManager::isValidDuration(30), "30 seconds must be valid");
	expect(ClipManager::isValidDuration(60), "60 seconds must be valid");
	expect(ClipManager::isValidDuration(120), "2 minutes must be valid");
	expect(ClipManager::isValidDuration(300), "5 minutes must be valid");
	expect(!ClipManager::isValidDuration(0), "zero seconds must be rejected");
	expect(!ClipManager::isValidDuration(301), "durations over five minutes must be rejected");

	return clipcoach::test::pass("clip-duration-test");
}
