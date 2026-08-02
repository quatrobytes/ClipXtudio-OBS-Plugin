#pragma once

#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

namespace clipcoach::test {

inline void expect(bool condition, std::string_view message)
{
	if (condition) {
		return;
	}

	std::cerr << "FAILED: " << message << '\n';
	std::exit(EXIT_FAILURE);
}

inline int pass(std::string_view testName)
{
	std::cout << "PASSED: " << testName << '\n';
	return EXIT_SUCCESS;
}

} // namespace clipcoach::test
