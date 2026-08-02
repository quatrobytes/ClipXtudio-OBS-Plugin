#include <clipcoach/core/clip-library-view-model.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <algorithm>
#include <cctype>
#include <numeric>
#include <utility>

namespace clipcoach {
namespace {

std::string lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		       [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
	return value;
}

bool isVertical(const ClipMetadata &clip)
{
	return clip.orientation == ClipOrientation::Vertical || clip.orientation == ClipOrientation::Both;
}

} // namespace

ClipLibraryViewModel::ClipLibraryViewModel(Clock clock, const FeatureGateService *featureGates)
	: clock_(std::move(clock)),
	  featureGates_(featureGates)
{
}

void ClipLibraryViewModel::setClips(std::vector<ClipMetadata> clips)
{
	clips_ = std::move(clips);
}

void ClipLibraryViewModel::setSession(std::optional<SessionMetadata> session)
{
	session_ = std::move(session);
}

void ClipLibraryViewModel::setFilter(ClipFilter filter) noexcept
{
	filter_ = filter;
}

void ClipLibraryViewModel::setSort(ClipSort sort) noexcept
{
	sort_ = sort;
}

void ClipLibraryViewModel::setSearchQuery(std::string query)
{
	normalizedQuery_ = lower(std::move(query));
}

void ClipLibraryViewModel::updateFavorite(const std::string &clipId, bool favorite)
{
	const auto clip = std::find_if(clips_.begin(), clips_.end(),
				       [&clipId](const ClipMetadata &candidate) { return candidate.id == clipId; });
	if (clip != clips_.end()) {
		clip->isFavorite = favorite;
	}
}

std::vector<ClipMetadata> ClipLibraryViewModel::visibleClips() const
{
	std::vector<ClipMetadata> result;
	result.reserve(clips_.size());
	std::copy_if(clips_.begin(), clips_.end(), std::back_inserter(result), [this](const ClipMetadata &clip) {
		bool matchesFilter = true;
		switch (filter_) {
		case ClipFilter::Favorites:
			matchesFilter = clip.isFavorite;
			break;
		case ClipFilter::Vertical:
			matchesFilter = isVertical(clip);
			break;
		case ClipFilter::Pending:
			matchesFilter = clip.exportStatus == ExportStatus::Pending ||
					clip.exportStatus == ExportStatus::Exporting;
			break;
		case ClipFilter::All:
			break;
		}
		if (!matchesFilter || normalizedQuery_.empty()) {
			return matchesFilter;
		}
		const auto searchable = lower(clip.title + "\n" + clip.fileName);
		return searchable.find(normalizedQuery_) != std::string::npos;
	});

	std::stable_sort(result.begin(), result.end(), [this](const ClipMetadata &left, const ClipMetadata &right) {
		if (sort_ == ClipSort::ScoreDescending && left.score != right.score) {
			return left.score > right.score;
		}
		if (left.createdAt != right.createdAt) {
			return left.createdAt > right.createdAt;
		}
		return left.id > right.id;
	});
	return result;
}

ClipLibrarySummary ClipLibraryViewModel::summary() const
{
	ClipLibrarySummary result;
	result.totalClips = clips_.size();
	for (const auto &clip : clips_) {
		result.bestScore = std::max(result.bestScore, clip.score);
		result.favoriteClips += clip.isFavorite ? 1U : 0U;
		result.verticalClips += isVertical(clip) ? 1U : 0U;
		result.pendingClips +=
			clip.exportStatus == ExportStatus::Pending ||
					clip.exportStatus == ExportStatus::Exporting
				? 1U
				: 0U;
		result.capturedDurationSeconds += std::max(clip.durationSeconds, 0);
	}

	if (session_) {
		const auto end = session_->endedAt.value_or(clock_());
		result.sessionDurationSeconds = static_cast<int>(
			std::max<std::chrono::system_clock::duration>(end - session_->startedAt,
								      std::chrono::system_clock::duration::zero()) /
			std::chrono::seconds(1));
	}
	if (featureGates_ != nullptr && !featureGates_->isAllowed(Feature::SessionRecap)) {
		result.favoriteClips = 0;
		result.verticalClips = 0;
		result.pendingClips = 0;
		result.capturedDurationSeconds = 0;
	}
	return result;
}

ClipFilter ClipLibraryViewModel::filter() const noexcept
{
	return filter_;
}

ClipSort ClipLibraryViewModel::sort() const noexcept
{
	return sort_;
}

} // namespace clipcoach
