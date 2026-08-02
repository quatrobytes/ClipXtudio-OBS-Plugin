#pragma once

#include <clipcoach/core/clip-action-service.hpp>
#include <clipcoach/core/clip-library-view-model.hpp>
#include <clipcoach/storage/repository-result.hpp>

#include <QObject>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace clipcoach {
class FeatureGateService;
namespace storage {
class ClipLibraryService;
}
} // namespace clipcoach

namespace clipcoach::ui {

enum class ClipLibraryErrorType {
	None,
	Load,
	Favorite,
};

struct ClipLibraryUiState {
	bool loading{false};
	ClipLibraryErrorType errorType{ClipLibraryErrorType::None};
	QString error;
	std::vector<ClipMetadata> clips;
	ClipLibrarySummary summary;
};

class ClipLibraryController final : public QObject {
public:
	using StateCallback = std::function<void(ClipLibraryUiState)>;
	using DeleteCallback = std::function<void(storage::StorageStatus)>;

	ClipLibraryController(storage::ClipLibraryService *libraryService, ClipActionService *actionService,
			      std::string sessionId, QObject *parent = nullptr,
			      const FeatureGateService *featureGates = nullptr);

	void setStateCallback(StateCallback callback);
	void reload();
	void setFullHistory(bool enabled);
	void setFilter(ClipFilter filter);
	void setSort(ClipSort sort);
	void setSearchQuery(std::string query);
	void setFavorite(const std::string &clipId, bool favorite);
	void deleteClips(std::vector<std::string> clipIds, DeleteCallback callback);
	[[nodiscard]] ClipActionResult preview(const std::string &clipId);
	[[nodiscard]] ClipActionResult openFolder(const std::string &clipId);
	[[nodiscard]] ClipActionResult openCaption(const std::string &clipId);
	[[nodiscard]] ClipActionResult openSubtitles(const std::string &clipId);
	[[nodiscard]] ClipActionResult exportVertical(const std::string &clipId);
	[[nodiscard]] ClipActionResult exportSelection(const std::vector<std::string> &clipIds,
						       ExportOrientation orientation);

private:
	void publish(bool loading = false, ClipLibraryErrorType errorType = ClipLibraryErrorType::None,
		     QString error = {});
	[[nodiscard]] const ClipMetadata *findClip(const std::string &clipId) const;

	storage::ClipLibraryService *libraryService_{nullptr};
	ClipActionService *actionService_{nullptr};
	std::string sessionId_;
	const FeatureGateService *featureGates_{nullptr};
	ClipLibraryViewModel viewModel_;
	std::vector<ClipMetadata> loadedClips_;
	StateCallback stateCallback_;
	std::uint64_t requestGeneration_{0};
	bool fullHistory_{false};
};

} // namespace clipcoach::ui
