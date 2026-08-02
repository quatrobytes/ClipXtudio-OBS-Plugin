#pragma once

#include <clipcoach/core/clip-metadata.hpp>
#include <clipcoach/core/session-metadata.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach {

class FeatureGateService;

enum class ClipFilter {
	All,
	Favorites,
	Vertical,
	Pending,
};

enum class ClipSort {
	DateDescending,
	ScoreDescending,
};

struct ClipLibrarySummary {
	std::size_t totalClips{0};
	int bestScore{0};
	int sessionDurationSeconds{0};
	std::size_t favoriteClips{0};
	std::size_t verticalClips{0};
	std::size_t pendingClips{0};
	int capturedDurationSeconds{0};
};

class ClipLibraryViewModel final {
public:
	using Clock = std::function<std::chrono::system_clock::time_point()>;

	explicit ClipLibraryViewModel(
		Clock clock = [] { return std::chrono::system_clock::now(); },
		const FeatureGateService *featureGates = nullptr);

	void setClips(std::vector<ClipMetadata> clips);
	void setSession(std::optional<SessionMetadata> session);
	void setFilter(ClipFilter filter) noexcept;
	void setSort(ClipSort sort) noexcept;
	void setSearchQuery(std::string query);
	void updateFavorite(const std::string &clipId, bool favorite);

	[[nodiscard]] std::vector<ClipMetadata> visibleClips() const;
	[[nodiscard]] ClipLibrarySummary summary() const;
	[[nodiscard]] ClipFilter filter() const noexcept;
	[[nodiscard]] ClipSort sort() const noexcept;

private:
	std::vector<ClipMetadata> clips_;
	std::optional<SessionMetadata> session_;
	Clock clock_;
	const FeatureGateService *featureGates_{nullptr};
	ClipFilter filter_{ClipFilter::All};
	ClipSort sort_{ClipSort::DateDescending};
	std::string normalizedQuery_;
};

} // namespace clipcoach
