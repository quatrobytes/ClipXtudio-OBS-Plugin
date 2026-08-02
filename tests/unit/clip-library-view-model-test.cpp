#include "test-support.hpp"

#include <clipcoach/core/clip-library-view-model.hpp>
#include <clipcoach/core/feature-gate-service.hpp>

#include <algorithm>
#include <chrono>
#include <vector>

namespace {

using namespace std::chrono;
using clipcoach::ClipFilter;
using clipcoach::ClipLibraryViewModel;
using clipcoach::ClipMetadata;
using clipcoach::ClipOrientation;
using clipcoach::ClipSort;
using clipcoach::ExportStatus;

ClipMetadata makeClip(const char *id, const char *title, int score, int minute,
		      ClipOrientation orientation = ClipOrientation::Horizontal, bool favorite = false,
		      ExportStatus exportStatus = ExportStatus::Exported)
{
	ClipMetadata clip;
	clip.id = id;
	clip.sessionId = "session-a";
	clip.fileName = std::string(id) + ".mkv";
	clip.title = title;
	clip.score = score;
	clip.createdAt = system_clock::time_point{} + minutes(minute);
	clip.durationSeconds = 30;
	clip.orientation = orientation;
	clip.isFavorite = favorite;
	clip.exportStatus = exportStatus;
	return clip;
}

std::vector<std::string> ids(const std::vector<ClipMetadata> &clips)
{
	std::vector<std::string> result;
	std::transform(clips.begin(), clips.end(), std::back_inserter(result),
		       [](const ClipMetadata &clip) { return clip.id; });
	return result;
}

} // namespace

int main()
{
	const auto start = system_clock::time_point{} + minutes(5);
	ClipLibraryViewModel viewModel([start] { return start + hours(2); });

	auto favorite = makeClip("favorite", "Epic Finish", 50, 20, ClipOrientation::Horizontal, true);
	auto vertical = makeClip("vertical", "Vertical clutch", 90, 10, ClipOrientation::Vertical, false,
				 ExportStatus::Pending);
	auto both = makeClip("both", "Dual canvas moment", 70, 30, ClipOrientation::Both);
	viewModel.setClips({favorite, vertical, both});

	clipcoach::SessionMetadata session;
	session.id = "session-a";
	session.startedAt = start;
	session.endedAt = start + hours(1);
	viewModel.setSession(session);

	viewModel.setFilter(ClipFilter::Favorites);
	clipcoach::test::expect(ids(viewModel.visibleClips()) == std::vector<std::string>{"favorite"},
				"favorites filter must return only favorite clips");

	viewModel.setFilter(ClipFilter::Vertical);
	clipcoach::test::expect(ids(viewModel.visibleClips()) == std::vector<std::string>({"both", "vertical"}),
				"vertical filter must include vertical and both orientations");

	viewModel.setFilter(ClipFilter::All);
	viewModel.setSearchQuery("EPIC");
	clipcoach::test::expect(ids(viewModel.visibleClips()) == std::vector<std::string>{"favorite"},
				"title search must be case insensitive");

	viewModel.setSearchQuery("");
	viewModel.setSort(ClipSort::ScoreDescending);
	clipcoach::test::expect(ids(viewModel.visibleClips()) ==
					std::vector<std::string>({"vertical", "both", "favorite"}),
				"score sort must order highest score first");

	viewModel.setSort(ClipSort::DateDescending);
	clipcoach::test::expect(ids(viewModel.visibleClips()) ==
					std::vector<std::string>({"both", "favorite", "vertical"}),
				"date sort must order newest clip first");

	const auto summary = viewModel.summary();
	clipcoach::test::expect(summary.totalClips == 3, "summary must count all loaded session clips");
	clipcoach::test::expect(summary.bestScore == 90, "summary must expose the best score");
	clipcoach::test::expect(summary.sessionDurationSeconds == 3600,
				"summary must use the persisted session duration");
	clipcoach::test::expect(summary.favoriteClips == 1 && summary.verticalClips == 2 && summary.pendingClips == 1,
				"summary must count filter categories");

	session.endedAt.reset();
	viewModel.setSession(session);
	clipcoach::test::expect(viewModel.summary().sessionDurationSeconds == 7200,
				"active session duration must use the injected clock");

	clipcoach::FeatureGateService recapGates;
	ClipLibraryViewModel gatedViewModel([start] { return start + hours(2); }, &recapGates);
	gatedViewModel.setClips({favorite, vertical, both});
	gatedViewModel.setSession(session);
	const auto freeSummary = gatedViewModel.summary();
	clipcoach::test::expect(freeSummary.totalClips == 3 && freeSummary.bestScore == 90 &&
					freeSummary.favoriteClips == 0 && freeSummary.verticalClips == 0 &&
					freeSummary.pendingClips == 0 && freeSummary.capturedDurationSeconds == 0,
				"Free may see library basics but Session Recap data must remain gated in core");
	recapGates.setEntitlementState(clipcoach::EntitlementState::ProActive);
	const auto proSummary = gatedViewModel.summary();
	clipcoach::test::expect(proSummary.favoriteClips == 1 && proSummary.verticalClips == 2 &&
					proSummary.pendingClips == 1 && proSummary.capturedDurationSeconds == 90,
				"Pro must receive the complete Session Recap");

	return clipcoach::test::pass("clip-library-view-model-test");
}
