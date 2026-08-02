#include "../unit/test-support.hpp"

#include <clipcoach/core/clip-library-view-model.hpp>

#include <chrono>
#include <string>
#include <vector>

int main()
{
	using namespace std::chrono;
	using clipcoach::ClipFilter;
	using clipcoach::ClipLibraryViewModel;
	using clipcoach::ClipMetadata;
	using clipcoach::ClipOrientation;
	using clipcoach::ClipSort;
	using clipcoach::test::expect;

	std::vector<ClipMetadata> clips;
	clips.reserve(10'000);
	for (int index = 0; index < 10'000; ++index) {
		ClipMetadata clip;
		clip.id = "clip-" + std::to_string(index);
		clip.sessionId = "performance-session";
		clip.title = index % 20 == 0 ? "highlight clutch" : "regular moment";
		clip.createdAt = system_clock::time_point{} + seconds(index);
		clip.score = index % 101;
		clip.orientation = index % 3 == 0 ? ClipOrientation::Vertical : ClipOrientation::Horizontal;
		clip.isFavorite = index % 10 == 0;
		clips.push_back(std::move(clip));
	}

	ClipLibraryViewModel viewModel;
	const auto started = steady_clock::now();
	viewModel.setClips(std::move(clips));
	viewModel.setFilter(ClipFilter::Vertical);
	viewModel.setSearchQuery("clutch");
	viewModel.setSort(ClipSort::ScoreDescending);
	const auto visible = viewModel.visibleClips();
	const auto summary = viewModel.summary();
	const auto elapsed = duration_cast<milliseconds>(steady_clock::now() - started);

	expect(!visible.empty(), "10k dataset must produce filtered results");
	expect(summary.totalClips == 10'000, "summary must cover all 10k clips");
	expect(elapsed < milliseconds(1'500),
	       "10k filter/sort/summary budget exceeded: " + std::to_string(elapsed.count()) + " ms");
	return clipcoach::test::pass("library-performance-test");
}
