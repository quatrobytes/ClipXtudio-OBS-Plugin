#include "storage-test-support.hpp"
#include "test-support.hpp"

#include <clipcoach/storage/clip-repository.hpp>
#include <clipcoach/storage/session-repository.hpp>
#include <clipcoach/storage/sqlite-database.hpp>

int main()
{
	using clipcoach::ClipOrientation;
	using clipcoach::ExportStatus;
	using clipcoach::test::expect;

	clipcoach::test::TemporaryDatabase temporary;
	{
		clipcoach::storage::SqliteDatabase database(temporary.path());
		expect(database.open().success, "repository database must open");

		clipcoach::storage::SessionRepository sessions(database);
		clipcoach::storage::ClipRepository clips(database);

		const auto session = clipcoach::test::sampleSession();
		expect(sessions.insert(session).success, "session must insert");
		const auto storedSession = sessions.findById(session.id);
		expect(storedSession.success && storedSession.value.has_value(), "inserted session must be queryable");
		expect(storedSession.value->sceneCollection == "Gaming", "session metadata must round-trip");

		auto horizontal = clipcoach::test::sampleClip("clip-horizontal");
		expect(clips.insert(horizontal).success, "horizontal clip must insert");

		auto vertical = clipcoach::test::sampleClip("clip-vertical");
		vertical.orientation = ClipOrientation::Vertical;
		vertical.isFavorite = true;
		vertical.score = 77;
		expect(clips.insert(vertical).success, "vertical favorite clip must insert");

		const auto bySession = clips.listBySession(session.id);
		expect(bySession.success && bySession.value.size() == 2, "session query must return both clips");
		clipcoach::AiPersistedClipResult aiResult;
		aiResult.clipId = horizontal.id;
		aiResult.suggestedTitles = {"AI title", "Second title"};
		aiResult.selectedTitle = "AI title";
		aiResult.caption = "Generated caption";
		aiResult.hashtags = {"#obs", "#clip"};
		aiResult.summary = "Generated summary";
		aiResult.subtitlePath = "/tmp/generated.srt";
		aiResult.language = clipcoach::AiLanguage::English;
		aiResult.score = 86;
		aiResult.hookStrength = 91;
		aiResult.qualityReason = "Strong hook and clear payoff";
		expect(clips.updateAiResult(aiResult).success, "AI result must update the owning clip");
		const auto withAi = clips.findById(horizontal.id);
		expect(withAi.success && withAi.value && withAi.value->title == "AI title" &&
			       withAi.value->caption == "Generated caption" &&
			       withAi.value->suggestedTitles.size() == 2 && withAi.value->hashtags.size() == 2 &&
			       withAi.value->subtitlePath.filename() == "generated.srt" && withAi.value->score == 86,
		       "AI metadata must round-trip through SQLite");

		const auto favorites = clips.listFavorites();
		expect(favorites.success && favorites.value.size() == 1, "favorite query must return only favorites");
		expect(favorites.value.front().id == vertical.id, "favorite query must return the expected clip");

		expect(clips.setFavorite(horizontal.id, true).success, "favorite update must succeed");
		const auto newlyFavorite = clips.findById(horizontal.id);
		expect(newlyFavorite.success && newlyFavorite.value->isFavorite, "favorite update must persist");
		expect(clips.updateThumbnail(horizontal.id, "/tmp/generated-thumbnail.jpg").success,
		       "thumbnail update must succeed");
		const auto withThumbnail = clips.findById(horizontal.id);
		expect(withThumbnail.success && withThumbnail.value &&
			       withThumbnail.value->thumbnailPath.filename() == "generated-thumbnail.jpg",
		       "generated thumbnail path must persist");

		const auto verticals = clips.listByOrientation(ClipOrientation::Vertical);
		expect(verticals.success && verticals.value.size() == 1,
		       "vertical filter must exclude horizontal clips");

		expect(clips.updateScore(horizontal.id, 91).success, "score update must succeed");
		const auto rescored = clips.findById(horizontal.id);
		expect(rescored.success && rescored.value->score == 91, "updated score must persist");

		expect(clips.markExported(horizontal.id).success, "mark exported update must succeed");
		const auto exported = clips.findById(horizontal.id);
		expect(exported.success && exported.value->exportStatus == ExportStatus::Exported,
		       "export status must persist");

		expect(clips.remove(vertical.id).success, "clip deletion must remove stored metadata");
		const auto deleted = clips.findById(vertical.id);
		expect(deleted.success && !deleted.value.has_value(), "deleted clip must no longer be queryable");
		expect(!clips.remove(vertical.id).success, "deleting a missing clip must return a controlled failure");
	}

	{
		clipcoach::storage::SqliteDatabase reopened(temporary.path());
		expect(reopened.open().success, "persisted database must reopen");
		clipcoach::storage::ClipRepository clips(reopened);
		const auto restored = clips.listBySession("session-test-001");
		expect(restored.success && restored.value.size() == 1 && restored.value.front().id == "clip-horizontal",
		       "clip deletion must remain persisted after database reopen");
	}

	return clipcoach::test::pass("repositories-test");
}
