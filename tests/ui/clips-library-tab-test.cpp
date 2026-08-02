#include "../unit/storage-test-support.hpp"
#include "../unit/test-support.hpp"

#include <clipcoach/storage/clip-library-service.hpp>
#include <clipcoach/core/feature-gate-service.hpp>
#include <clipcoach/ui/components/clip-card.hpp>
#include <clipcoach/ui/caption-generator.hpp>
#include <clipcoach/ui/main-dock.hpp>

#include <QApplication>
#include <QAbstractButton>
#include <QClipboard>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>

#include <future>

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);
	qputenv("CLIPXTUDIO_FFMPEG_PATH",
		QByteArrayLiteral(CLIPCOACH_TEST_FFMPEG_EXECUTABLE));

	clipcoach::test::TemporaryDatabase temporary;
	clipcoach::storage::ClipLibraryService library(temporary.path());

	std::promise<clipcoach::storage::StorageStatus> sessionPromise;
	library.storeSession(clipcoach::test::sampleSession(),
			     [&sessionPromise](auto status) { sessionPromise.set_value(std::move(status)); });
	clipcoach::test::expect(sessionPromise.get_future().get().success, "UI library fixture session must persist");

	auto clip = clipcoach::test::sampleClip();
	clip.title = "Persistent highlight";
	clip.durationSeconds = 2;
	clip.caption.clear();
	clip.transcriptPath.clear();
	const auto clipPath =
		temporary.path().parent_path() / "clipcoach-ui-clip.mkv";
	QProcess mediaGenerator;
	mediaGenerator.start(
		QStringLiteral(CLIPCOACH_TEST_FFMPEG_EXECUTABLE),
		{QStringLiteral("-hide_banner"), QStringLiteral("-loglevel"),
		 QStringLiteral("error"), QStringLiteral("-f"),
		 QStringLiteral("lavfi"), QStringLiteral("-i"),
		 QStringLiteral("testsrc2=size=640x360:rate=30:duration=2"),
		 QStringLiteral("-c:v"), QStringLiteral("mpeg4"),
		 QStringLiteral("-y"), QString::fromStdString(clipPath.u8string())});
	clipcoach::test::expect(
		mediaGenerator.waitForFinished(10000) && mediaGenerator.exitCode() == 0 &&
			QFileInfo::exists(QString::fromStdString(clipPath.u8string())),
		"UI clip fixture must be a decodable video");
	clip.filePath = clipPath;
	clip.fileName = clipPath.filename().u8string();
	const auto thumbnailPath =
		temporary.path().parent_path() / "clipcoach-ui-thumbnail.jpg";
	QImage thumbnail(320, 180, QImage::Format_RGB32);
	thumbnail.fill(QColor(QStringLiteral("#6D35D9")));
	clip.thumbnailPath = thumbnailPath;
	clipcoach::test::expect(
		thumbnail.save(QString::fromStdString(thumbnailPath.u8string())),
		"UI clip fixture thumbnail must be created");
	std::promise<clipcoach::storage::StorageStatus> clipPromise;
	library.storeClip(clip, [&clipPromise](auto status) { clipPromise.set_value(std::move(status)); });
	clipcoach::test::expect(clipPromise.get_future().get().success, "UI library fixture clip must persist");
	auto previousSession = clipcoach::test::sampleSession();
	previousSession.id = "session-previous";
	previousSession.endedAt = previousSession.startedAt +
				  std::chrono::minutes(45);
	std::promise<clipcoach::storage::StorageStatus> previousSessionPromise;
	library.storeSession(previousSession, [&previousSessionPromise](auto status) {
		previousSessionPromise.set_value(std::move(status));
	});
	clipcoach::test::expect(
		previousSessionPromise.get_future().get().success,
		"previous Pro-history session must persist");
	auto previousClip = clipcoach::test::sampleClip("clip-previous");
	previousClip.sessionId = previousSession.id;
	previousClip.title = "Previous session highlight";
	const auto previousClipPath =
		temporary.path().parent_path() / "clipcoach-ui-previous-clip.mkv";
	QFile previousClipFile(
		QString::fromStdString(previousClipPath.u8string()));
	clipcoach::test::expect(
		previousClipFile.open(QIODevice::WriteOnly) &&
			previousClipFile.write("previous-clip-fixture") > 0,
		"previous UI clip fixture media file must be created");
	previousClipFile.close();
	previousClip.filePath = previousClipPath;
	previousClip.fileName = previousClipPath.filename().u8string();
	std::promise<clipcoach::storage::StorageStatus> previousClipPromise;
	library.storeClip(previousClip, [&previousClipPromise](auto status) {
		previousClipPromise.set_value(std::move(status));
	});
	clipcoach::test::expect(previousClipPromise.get_future().get().success,
			       "previous Pro-history clip must persist");

	QSettings locale(QStringLiteral(CLIPCOACH_SOURCE_DIR "/data/locale/es-ES.ini"), QSettings::IniFormat);
	const auto translator = [&locale](const char *key) {
		return locale.value(QString::fromUtf8(key), QString::fromUtf8(key)).toString();
	};
	clipcoach::FeatureGateService gates(clipcoach::EntitlementState::ProActive);
	clipcoach::ui::CaptionGenerationCompletion pendingCaptionCompletion;
	clipcoach::ui::CaptionGenerationProgressCallback pendingCaptionProgress;
	clipcoach::ui::MainDock dock(translator, nullptr, nullptr, &library, nullptr,
				     QStringLiteral("session-test-001").toStdString(), nullptr, nullptr, nullptr,
				     nullptr, nullptr, &gates, nullptr, {}, {},
				     [&pendingCaptionCompletion, &pendingCaptionProgress](const clipcoach::ClipMetadata &,
					clipcoach::ui::CaptionGenerationProgressCallback progress,
					clipcoach::ui::CaptionGenerationCompletion
						completion) {
					     pendingCaptionProgress = std::move(progress);
					     pendingCaptionCompletion = std::move(completion);
				     });
	dock.resize(520, 820);
	dock.show();
	auto *stack = dock.findChild<QStackedWidget *>(QStringLiteral("mainTabStack"));
	auto *tabBar = dock.findChild<QTabBar *>(QStringLiteral("mainTabBar"));
	auto *loading = dock.findChild<QLabel *>(QStringLiteral("clipsLoadingLabel"));
	clipcoach::test::expect(stack != nullptr && tabBar != nullptr && loading != nullptr,
				"Clips tab async state widgets must exist");

	tabBar->setCurrentIndex(3);
	const auto persistedCardCount = [&dock] {
		int count = 0;
		for (auto *frame : dock.findChildren<QFrame *>()) {
			if (frame->property("libraryRole") == QStringLiteral("persistedClip")) {
				++count;
			}
		}
		return count;
	};
	QElapsedTimer timeout;
	timeout.start();
	while (persistedCardCount() == 0 && timeout.elapsed() < 2000) {
		application.processEvents();
	}

	clipcoach::test::expect(
		persistedCardCount() == 1,
		"Clips tab must open with the current OBS session instead of surprising the user with old clips");
	dock.resize(1000, 820);
	application.processEvents();
	auto *thumbnailFrame =
		dock.findChild<QFrame *>(QStringLiteral("ClipThumbnail"));
	auto *thumbnailImage =
		dock.findChild<QLabel *>(QStringLiteral("clipThumbnailImage"));
	auto *details =
		dock.findChild<QWidget *>(QStringLiteral("clipDetails"));
	auto *trigger =
		dock.findChild<QLabel *>(QStringLiteral("TriggerBadge"));
	auto *previewAction =
		dock.findChild<QPushButton *>(QStringLiteral("clipPreviewButton"));
	auto *editAction =
		dock.findChild<QPushButton *>(QStringLiteral("clipQuickEditorButton"));
	auto *selectionForGeometry =
		dock.findChild<QCheckBox *>(QStringLiteral("clipSelectionCheck"));
	clipcoach::test::expect(
		thumbnailFrame != nullptr && thumbnailImage != nullptr &&
			!thumbnailImage->pixmap().isNull(),
		"persisted ClipCard must render the saved thumbnail image");
	clipcoach::test::expect(
		selectionForGeometry != nullptr && details != nullptr &&
			previewAction != nullptr && editAction != nullptr &&
			selectionForGeometry->mapTo(&dock, QPoint()).x() <
				thumbnailFrame->mapTo(&dock, QPoint()).x() &&
			thumbnailFrame->mapTo(&dock, QPoint()).x() <
				details->mapTo(&dock, QPoint()).x() &&
			details->mapTo(&dock, QPoint()).x() <
				previewAction->mapTo(&dock, QPoint()).x(),
		"selection, thumbnail and metadata must stay grouped on the left before actions");
	editAction->click();
	application.processEvents();
	auto *quickEditor = dock.findChild<QDialog *>(QStringLiteral("quickClipEditorDialog"));
	clipcoach::test::expect(
		quickEditor != nullptr &&
			quickEditor->findChild<QWidget *>(QStringLiteral("quickEditorVideo")) != nullptr &&
			quickEditor->findChild<QWidget *>(QStringLiteral("quickEditorTrimRange")) != nullptr &&
			quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorMarkCutStartButton")) != nullptr &&
			quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorRemoveCutButton")) != nullptr &&
			quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorUndoCutsButton")) != nullptr &&
			quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorSmartTrimButton")) != nullptr &&
			quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorExportButton")) != nullptr &&
			quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorGenerateCaptionButton")) != nullptr,
		"the pencil action must open an editor with preview, timeline cuts, smart trim, caption and MP4 export");
	auto *editorVideo = quickEditor->findChild<QLabel *>(QStringLiteral("quickEditorVideo"));
	auto *editorRange = quickEditor->findChild<QWidget *>(QStringLiteral("quickEditorTrimRange"));
	auto *editorPlay = quickEditor->findChild<QPushButton *>(QStringLiteral("quickEditorPlayButton"));
	const auto editorShortcuts = quickEditor->findChildren<QShortcut *>();
	clipcoach::test::expect(editorRange != nullptr && editorPlay != nullptr,
		"the editor playhead and play control must exist");
	clipcoach::test::expect(editorRange->minimumHeight() >= 150,
		"the editor timeline must reserve enough height for time ticks, filmstrip and waveform");
	clipcoach::test::expect(
		editorRange->property("endMilliseconds").toLongLong() ==
			static_cast<qint64>(clip.durationSeconds) * 1000,
		"the initial trim selection must cover the complete clip duration");
	clipcoach::test::expect(editorPlay->height() <= 40,
		"the editor play control must remain compact");
	auto hasShortcut = [&editorShortcuts](const QKeySequence &sequence) {
		return std::any_of(editorShortcuts.begin(), editorShortcuts.end(),
			[&sequence](const QShortcut *shortcut) {
				return shortcut->key().matches(sequence) == QKeySequence::ExactMatch;
			});
	};
	clipcoach::test::expect(hasShortcut(QKeySequence(Qt::Key_Space)),
		"the editor playback keyboard shortcut must use Space");
	clipcoach::test::expect(hasShortcut(QKeySequence(QStringLiteral("Ctrl+B"))) &&
		       hasShortcut(QKeySequence(Qt::Key_Delete)),
		"the editor must expose CapCut-like Split and Delete keyboard shortcuts");
	QElapsedTimer previewTimeout;
	previewTimeout.start();
	while (editorVideo != nullptr && editorVideo->pixmap().isNull() &&
	       previewTimeout.elapsed() < 5000)
		application.processEvents();
	clipcoach::test::expect(editorVideo != nullptr && !editorVideo->pixmap().isNull(),
		"the quick editor must decode and display a real preview frame with bundled FFmpeg");
	auto *quickCaptionButton = quickEditor->findChild<QPushButton *>(
		QStringLiteral("quickEditorGenerateCaptionButton"));
	quickCaptionButton->click();
	application.processEvents();
	auto *quickCaptionProgressDialog = quickEditor->findChild<QDialog *>(
		QStringLiteral("quickEditorCaptionProgressDialog"));
	auto *quickCaptionProgress = quickEditor->findChild<QProgressBar *>(
		QStringLiteral("quickEditorCaptionProgressBar"));
	clipcoach::test::expect(
		quickCaptionProgressDialog != nullptr && quickCaptionProgressDialog->isVisible() &&
			quickCaptionProgressDialog->isModal() && quickCaptionProgress != nullptr,
		"caption generation inside the quick editor must show a blocking progress dialog");
	clipcoach::test::expect(static_cast<bool>(pendingCaptionProgress),
		"quick editor caption generation must preserve the progress callback");
	pendingCaptionProgress({48, QStringLiteral("Analizando audio y video"), 35});
	application.processEvents();
	auto *quickCaptionStatus = quickEditor->findChild<QLabel *>(
		QStringLiteral("quickEditorCaptionProgressStatus"));
	clipcoach::test::expect(
		quickCaptionProgress->value() == 48 && quickCaptionStatus != nullptr &&
			quickCaptionStatus->text().contains(QStringLiteral("audio"), Qt::CaseInsensitive),
		"quick editor caption progress must show the real percentage and current audio stage");
	clipcoach::test::expect(static_cast<bool>(pendingCaptionCompletion),
		"quick editor caption generation must preserve the completion callback");
	pendingCaptionCompletion({true, QStringLiteral("Caption listo #ClipXtudio"), {},
		QStringLiteral("Caption listo #Shorts")});
	pendingCaptionCompletion = {};
	pendingCaptionProgress = {};
	application.processEvents();
	quickCaptionProgressDialog = quickEditor->findChild<QDialog *>(
		QStringLiteral("quickEditorCaptionProgressDialog"));
	clipcoach::test::expect(
		(quickCaptionProgressDialog == nullptr || !quickCaptionProgressDialog->isVisible()) &&
			quickCaptionButton->isEnabled(),
		"successful caption generation must close the progress dialog and re-enable the action");
	editorPlay->click();
	QElapsedTimer playbackTimeout;
	playbackTimeout.start();
	while (editorRange->property("positionMilliseconds").toLongLong() == 0 &&
	       playbackTimeout.elapsed() < 1500)
		application.processEvents();
	clipcoach::test::expect(
		editorRange->property("positionMilliseconds").toLongLong() > 0,
		"playing the selection must advance the timeline playhead in real time");
	editorPlay->click();
	QMouseEvent seekPress(QEvent::MouseButtonPress,
		QPointF(editorRange->width() * 0.5, 45),
		QPointF(editorRange->width() * 0.5, 45), Qt::LeftButton,
		Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(editorRange, &seekPress);
	clipcoach::test::expect(
		editorRange->property("positionMilliseconds").toLongLong() > 0,
		"clicking the timeline away from trim handles must seek the movable playhead");
	auto *splitButton = quickEditor->findChild<QPushButton *>(
		QStringLiteral("quickEditorMarkCutStartButton"));
	auto *deleteRangeButton = quickEditor->findChild<QPushButton *>(
		QStringLiteral("quickEditorRemoveCutButton"));
	splitButton->click();
	QMouseEvent secondSeek(QEvent::MouseButtonPress,
		QPointF(editorRange->width() * 0.75, 45),
		QPointF(editorRange->width() * 0.75, 45), Qt::LeftButton,
		Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(editorRange, &secondSeek);
	splitButton->click();
	QMouseEvent thirdSeek(QEvent::MouseButtonPress,
		QPointF(editorRange->width() * 0.25, 45),
		QPointF(editorRange->width() * 0.25, 45), Qt::LeftButton,
		Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(editorRange, &thirdSeek);
	splitButton->click();
	QMouseEvent selectSecond(QEvent::MouseButtonPress,
		QPointF(editorRange->width() * 0.375, 45),
		QPointF(editorRange->width() * 0.375, 45), Qt::LeftButton,
		Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(editorRange, &selectSecond);
	clipcoach::test::expect(
		deleteRangeButton->isEnabled() &&
			editorRange->property("selectedSegmentCount").toInt() == 1 &&
			!deleteRangeButton->icon().isNull() &&
			!deleteRangeButton->text().contains(QLatin1Char('(')),
		"One selected timeline block must use the singular trash action");
	QMouseEvent addFourth(QEvent::MouseButtonPress,
		QPointF(editorRange->width() * 0.875, 45),
		QPointF(editorRange->width() * 0.875, 45), Qt::LeftButton,
		Qt::LeftButton, Qt::ControlModifier);
	QApplication::sendEvent(editorRange, &addFourth);
	clipcoach::test::expect(
		deleteRangeButton->isEnabled() &&
			editorRange->property("selectedSegmentCount").toInt() == 2 &&
			!deleteRangeButton->icon().isNull() &&
			deleteRangeButton->text().contains(QStringLiteral("2")),
		"Ctrl+click must select multiple non-adjacent timeline blocks for one deletion");
	deleteRangeButton->click();
	clipcoach::test::expect(
		editorRange->property("visibleSegmentCount").toInt() == 2 &&
			editorRange->property("removedRangeCount").toInt() == 2,
		"Delete must remove every selected block and ripple both retained blocks");
	QMouseEvent seekRetainedStart(QEvent::MouseButtonPress,
		QPointF(editorRange->width() * 0.1, 45),
		QPointF(editorRange->width() * 0.1, 45), Qt::LeftButton,
		Qt::LeftButton, Qt::NoModifier);
	QApplication::sendEvent(editorRange, &seekRetainedStart);
	const int segmentStartsBeforePlayback =
		editorRange->property("playbackSegmentStartCount").toInt();
	editorPlay->click();
	QElapsedTimer ripplePlaybackTimeout;
	ripplePlaybackTimeout.start();
	while (editorRange->property("playbackSegmentStartCount").toInt() <=
		       segmentStartsBeforePlayback && ripplePlaybackTimeout.elapsed() < 1500)
		application.processEvents();
	clipcoach::test::expect(
		editorRange->property("playbackSegmentStartCount").toInt() >
			segmentStartsBeforePlayback,
		"Play and Space must continue into the next retained block regardless of selection");
	QDir().mkpath(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR));
	clipcoach::test::expect(
		quickEditor->grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/quick-clip-editor.png")),
		"quick clip editor preview must be saved for visual QA");
	quickEditor->close();
	trigger = dock.findChild<QLabel *>(QStringLiteral("TriggerBadge"));
	clipcoach::test::expect(
		trigger != nullptr && trigger->text() == QStringLiteral("saca clip"),
		"ClipCard must show the phrase that triggered the clip instead of Manual");
	auto *historyScope =
		dock.findChild<QComboBox *>(QStringLiteral("clipsHistoryScope"));
	clipcoach::test::expect(historyScope != nullptr && historyScope->isEnabled(),
			       "Pro Clips tab must expose the saved-history scope explicitly");
	historyScope->setCurrentIndex(1);
	QElapsedTimer historyTimeout;
	historyTimeout.start();
	while (persistedCardCount() < 2 && historyTimeout.elapsed() < 2000)
		application.processEvents();
	clipcoach::test::expect(
		persistedCardCount() == 2,
		"selecting saved history must reconstruct current and previous session ClipCards");
	clipcoach::FeatureGateService freeGates;
	clipcoach::ui::MainDock freeDock(
		translator, nullptr, nullptr, &library, nullptr,
		QStringLiteral("session-test-001").toStdString(), nullptr, nullptr,
		nullptr, nullptr, nullptr, &freeGates);
	freeDock.resize(520, 820);
	freeDock.show();
	auto *freeTabs =
		freeDock.findChild<QTabBar *>(QStringLiteral("mainTabBar"));
	freeTabs->setCurrentIndex(3);
	const auto freeCardCount = [&freeDock] {
		int count = 0;
		for (auto *frame : freeDock.findChildren<QFrame *>()) {
			if (frame->property("libraryRole") ==
			    QStringLiteral("persistedClip"))
				++count;
		}
		return count;
	};
	QElapsedTimer freeTimeout;
	freeTimeout.start();
	while (freeCardCount() == 0 && freeTimeout.elapsed() < 2000)
		application.processEvents();
	clipcoach::test::expect(
		freeCardCount() == 1,
		"Free Clips tab must remain limited to the current session");
	for (const auto *name : {"clipFavoriteButton", "clipPreviewButton", "clipExportVerticalButton",
				 "clipCaptionButton", "clipSubtitlesButton", "clipOpenFolderButton",
				 "clipDeleteButton"}) {
		clipcoach::test::expect(dock.findChild<QPushButton *>(QString::fromLatin1(name)) != nullptr,
					"persisted ClipCard must expose every required action");
	}
	auto *captionAction = dock.findChild<QPushButton *>(
		QStringLiteral("clipCaptionButton"));
	auto *subtitlesAction = dock.findChild<QPushButton *>(
		QStringLiteral("clipSubtitlesButton"));
	clipcoach::test::expect(
		captionAction != nullptr && captionAction->isEnabled(),
		"Pro clip cards must enable caption generation when a generator is connected");
	clipcoach::test::expect(
		subtitlesAction != nullptr && !subtitlesAction->isEnabled() &&
			subtitlesAction->toolTip().contains(
				QStringLiteral("Próximamente")),
		"subtitle action must remain visibly unavailable with a Próximamente tooltip");
	captionAction->click();
	auto *captionOverlay = dock.findChild<QFrame *>(
		QStringLiteral("captionBusyOverlay"));
	auto *captionProgress = dock.findChild<QProgressBar *>(
		QStringLiteral("captionBusyProgress"));
	clipcoach::test::expect(
		captionOverlay != nullptr && captionOverlay->isVisible() &&
			captionProgress != nullptr && captionProgress->minimum() == 0 &&
			captionProgress->maximum() == 100,
		"caption generation must block the whole plugin with a determinate progress overlay");
	auto *captionBusyCard = dock.findChild<QFrame *>(QStringLiteral("captionBusyCard"));
	auto *captionBusyLabel = dock.findChild<QLabel *>(QStringLiteral("captionBusyLabel"));
	auto *captionBusyPercent = dock.findChild<QLabel *>(QStringLiteral("captionBusyPercent"));
	auto *captionBusySpinner = dock.findChild<QLabel *>(QStringLiteral("captionBusySpinner"));
	auto *captionBusyEta = dock.findChild<QLabel *>(QStringLiteral("captionBusyEta"));
	auto *captionProgressTimer = dock.findChild<QTimer *>(QStringLiteral("captionProgressTimer"));
	clipcoach::test::expect(
		captionBusyCard != nullptr && captionBusyCard->minimumWidth() >= 480 &&
			captionBusyLabel != nullptr && captionBusyLabel->wordWrap() &&
			captionBusyLabel->minimumHeight() >= 44 &&
			captionBusySpinner != nullptr && captionProgressTimer != nullptr &&
			captionProgressTimer->interval() == 250,
		"caption progress copy must have enough width and height to remain fully visible");
	clipcoach::test::expect(static_cast<bool>(pendingCaptionProgress),
			       "caption generator must retain its progress callback");
	pendingCaptionProgress(
		{47, QStringLiteral("Analizando video y audio"), 38});
	application.processEvents();
	clipcoach::test::expect(
		captionProgress->value() == 47 && captionBusyPercent != nullptr &&
			captionBusyPercent->text() == QStringLiteral("47%") &&
			captionBusyLabel->text() == QStringLiteral("Analizando video y audio") &&
			captionBusyEta != nullptr && captionBusyEta->text().contains(QStringLiteral("38")),
		"caption overlay must show the reported percentage, media stage and remaining time");
	pendingCaptionProgress(
		{82, QStringLiteral("Generando el caption con IA"), 0});
	for (int tick = 0; tick < 24; ++tick)
		QMetaObject::invokeMethod(captionProgressTimer, "timeout",
					  Qt::DirectConnection);
	clipcoach::test::expect(
		captionBusyPercent->text() == QStringLiteral("82%") &&
			!captionBusySpinner->pixmap().isNull() &&
			captionBusyLabel->text().contains(QStringLiteral("casi"),
							 Qt::CaseInsensitive) &&
			captionBusyEta->text().contains(QStringLiteral("sigue"),
						       Qt::CaseInsensitive),
		"a stalled percentage must keep an animated spinner and rotate to reassuring active-work copy");
	QDir().mkpath(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR));
	clipcoach::test::expect(
		dock.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/caption-progress.png")),
		"caption progress overlay preview must be saved");
	clipcoach::test::expect(static_cast<bool>(pendingCaptionCompletion),
			       "caption generator must retain its asynchronous completion callback");
	pendingCaptionCompletion(
		{true,
		 QStringLiteral("Caption sugerido para redes\n\n#ClipXtudio #OBS #Gaming #Viral #Streamer #Extra #Sobra"),
		 {},
		 QString(120, QLatin1Char('A')) + QStringLiteral(" #Shorts")});
	pendingCaptionCompletion = {};
	pendingCaptionProgress = {};
	application.processEvents();
	auto *captionDialog = dock.findChild<QDialog *>(
		QStringLiteral("clipCaptionDialog"));
	QElapsedTimer captionTimeout;
	captionTimeout.start();
	while (captionDialog == nullptr && captionTimeout.elapsed() < 2000) {
		application.processEvents();
		captionDialog = dock.findChild<QDialog *>(
			QStringLiteral("clipCaptionDialog"));
	}
	if (captionDialog == nullptr) {
		for (auto *widget : QApplication::topLevelWidgets()) {
			auto *candidate = qobject_cast<QDialog *>(widget);
			if (candidate != nullptr &&
			    candidate->objectName() ==
				    QStringLiteral("clipCaptionDialog")) {
				captionDialog = candidate;
				break;
			}
		}
	}
	auto *captionText = captionDialog != nullptr
				    ? captionDialog->findChild<QPlainTextEdit *>(
					      QStringLiteral("captionText"))
				    : nullptr;
	auto *shortsText = captionDialog != nullptr
				   ? captionDialog->findChild<QPlainTextEdit *>(
					     QStringLiteral("youtubeShortsCaptionText"))
				   : nullptr;
	auto *copySocial = captionDialog != nullptr
				   ? captionDialog->findChild<QPushButton *>(
					     QStringLiteral("copyCaptionButton"))
				   : nullptr;
	auto *copyShorts = captionDialog != nullptr
				   ? captionDialog->findChild<QPushButton *>(
					     QStringLiteral("copyYoutubeShortsCaptionButton"))
				   : nullptr;
	clipcoach::test::expect(captionDialog != nullptr,
			       "caption generation must create a result dialog");
	clipcoach::test::expect(captionDialog->isVisible(),
			       "caption generation result dialog must be visible");
	clipcoach::test::expect(captionOverlay->isHidden(),
			       "caption processing overlay must close before presenting the final result");
	clipcoach::test::expect(captionText != nullptr,
			       "caption result dialog must expose copyable text");
	clipcoach::test::expect(
		captionText->toPlainText().contains(
			QStringLiteral("Caption sugerido")) &&
			captionText->toPlainText().count(QLatin1Char('#')) == 5 &&
			captionText->toPlainText().split(
				QStringLiteral("\n\n"), Qt::SkipEmptyParts).size() == 2,
		"social caption must contain one SEO paragraph followed by exactly five hashtags");
	const auto normalizedSocial = clipcoach::ui::formatSocialCaption(
		QString(260, QLatin1Char('A')) + QStringLiteral("\n\n") +
			QString(260, QLatin1Char('B')),
		{QStringLiteral("#Uno"), QStringLiteral("#Dos"),
		 QStringLiteral("#Tres"), QStringLiteral("#Cuatro"),
		 QStringLiteral("#Cinco")});
	const auto normalizedParts = normalizedSocial.split(
		QStringLiteral("\n\n"), Qt::SkipEmptyParts);
	clipcoach::test::expect(
		normalizedParts.size() == 2 &&
			normalizedParts.front().toUcs4().size() <=
				clipcoach::ui::kSocialCaptionMaximumBodyCharacters &&
			!normalizedParts.front().contains(QLatin1Char('\n')) &&
			normalizedParts.back().count(QLatin1Char('#')) == 5,
		"social caption normalization must merge provider paragraphs, bound body length and preserve five hashtags");
	clipcoach::test::expect(
		shortsText != nullptr && shortsText->toPlainText().toUcs4().size() <= 100 &&
			shortsText->toPlainText().contains(QLatin1Char('#')) &&
			copySocial != nullptr && copyShorts != nullptr &&
			copySocial->geometry().top() >= captionText->geometry().bottom() &&
			copyShorts->geometry().top() >= shortsText->geometry().bottom() &&
			copyShorts->text() == QStringLiteral("Copiar para YT Shorts"),
		"YouTube Shorts output must combine title and hashtags within 100 characters and copy separately");
	clipcoach::test::expect(
		captionDialog->grab().save(QStringLiteral(
			CLIPCOACH_UI_ARTIFACT_DIR "/caption-result-two-platforms.png")),
		"two-platform caption dialog preview must be saved");
	copySocial->click();
	clipcoach::test::expect(
		QApplication::clipboard()->text() == captionText->toPlainText(),
		"social copy action must copy only the TikTok, Instagram and Facebook caption");
	copyShorts->click();
	clipcoach::test::expect(
		QApplication::clipboard()->text() == shortsText->toPlainText(),
		"YouTube copy action must copy only the Shorts caption");
	captionDialog->close();
	application.processEvents();
	QElapsedTimer reloadTimeout;
	reloadTimeout.start();
	while ((reloadTimeout.elapsed() < 100 || !loading->isHidden() ||
		persistedCardCount() < 1) &&
	       reloadTimeout.elapsed() < 2000)
		application.processEvents();
	clipcoach::test::expect(
		loading->isHidden() && persistedCardCount() >= 1,
		"caption persistence reload must finish rebuilding the clip list");
	auto *selection = dock.findChild<QCheckBox *>(QStringLiteral("clipSelectionCheck"));
	auto *batchExport = dock.findChild<QPushButton *>(QStringLiteral("batchExportButton"));
	auto *batchDelete = dock.findChild<QPushButton *>(QStringLiteral("batchDeleteButton"));
	clipcoach::test::expect(selection != nullptr && batchExport != nullptr &&
				       batchDelete != nullptr,
				"persisted clips must be selectable for batch export or deletion");
	selection->setChecked(true);
	application.processEvents();
	clipcoach::test::expect(batchExport->isEnabled(), "selecting a clip must enable batch export");
	clipcoach::test::expect(
		selection->isChecked() && batchDelete->isEnabled(),
		"selection must remain checked and enable bulk deletion without reloading the list");
	application.processEvents();
	QDir().mkpath(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR));
	clipcoach::test::expect(dock.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/clips-persisted.png")),
				"persisted Clips tab preview must be saved");

	QTimer::singleShot(0, [] {
		for (auto *widget : QApplication::topLevelWidgets()) {
			if (auto *messageBox = qobject_cast<QMessageBox *>(widget)) {
				if (auto *yesButton =
					    messageBox->button(QMessageBox::Yes))
					yesButton->click();
			}
		}
	});
	batchDelete->click();
	QElapsedTimer deleteTimeout;
	deleteTimeout.start();
	while (std::filesystem::exists(clipPath) &&
	       std::filesystem::exists(previousClipPath) &&
	       deleteTimeout.elapsed() < 2000)
		application.processEvents();
	clipcoach::test::expect(
		!std::filesystem::exists(clipPath) ||
			!std::filesystem::exists(previousClipPath),
		"bulk deletion must forward selected IDs and delete the media file");

	std::promise<clipcoach::storage::RepositoryResult<
		std::vector<clipcoach::ClipMetadata>>> deletedListPromise;
	library.listRecentAsync(
		10,
		[&deletedListPromise](auto result) {
			deletedListPromise.set_value(std::move(result));
		});
	const auto deletedList = deletedListPromise.get_future().get();
	clipcoach::test::expect(
		deletedList.success && deletedList.value.size() == 1,
		"bulk deletion must remove the selected clip metadata from SQLite");

	return clipcoach::test::pass("clips-library-tab-test");
}
