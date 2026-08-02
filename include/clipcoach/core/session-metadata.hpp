#pragma once

#include <chrono>
#include <optional>
#include <string>

namespace clipcoach {

struct SessionMetadata {
	std::string id;
	std::chrono::system_clock::time_point startedAt;
	std::optional<std::chrono::system_clock::time_point> endedAt;
	std::string profileName;
	std::string sceneCollection;
	std::string appVersion;
	std::string aiSummary;
	std::string aiLanguage;
};

} // namespace clipcoach
