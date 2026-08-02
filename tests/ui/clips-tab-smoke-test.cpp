#include "../unit/test-support.hpp"

#include <clipcoach/core/export-manager.hpp>
#include <clipcoach/ui/components/pro-badge.hpp>
#include <clipcoach/ui/clips-tab.hpp>
#include <clipcoach/ui/design-tokens.hpp>

#include <QApplication>
#include <QFrame>
#include <QGridLayout>
#include <QPushButton>
#include <QProgressBar>
#include <QComboBox>
#include <QSettings>
#include <QElapsedTimer>
#include <QLineEdit>

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

QRect relativeRect(QWidget *widget, QWidget *ancestor)
{
	return {widget->mapTo(ancestor, QPoint(0, 0)), widget->size()};
}

void expectNoOverlap(const std::vector<QWidget *> &widgets, QWidget *ancestor, const char *message)
{
	for (std::size_t left = 0; left < widgets.size(); ++left) {
		for (std::size_t right = left + 1; right < widgets.size(); ++right) {
			clipcoach::test::expect(
				!relativeRect(widgets[left], ancestor).intersects(relativeRect(widgets[right], ancestor)),
				message);
		}
	}
}

class UiExportBackend final : public clipcoach::ExportBackend {
public:
	clipcoach::ExportBackendResult execute(const clipcoach::ExportJob &, const std::filesystem::path &temporaryPath,
					       ProgressCallback progress, const std::atomic_bool &) override
	{
		progress(55);
		std::ofstream output(temporaryPath);
		output << "mp4";
		return clipcoach::ExportBackendResult::ok();
	}
};

} // namespace

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	QSettings locale(QStringLiteral(CLIPCOACH_SOURCE_DIR "/data/locale/es-ES.ini"), QSettings::IniFormat);
	const auto translator = [&locale](const char *key) {
		return locale.value(QString::fromUtf8(key), QString::fromUtf8(key)).toString();
	};

	clipcoach::ExportManager exports(std::make_unique<UiExportBackend>());
	clipcoach::ui::ClipsTab tab(translator, nullptr, QStringLiteral("session-a"), nullptr, nullptr, &exports);
	tab.setMaximumWidth(520);
	tab.resize(520, 820);
	tab.show();
	application.processEvents();

	clipcoach::test::expect(tab.objectName() == QStringLiteral("clipsTab"),
				"ClipsTab must expose a stable object name");
	clipcoach::test::expect(tab.findChild<QFrame *>(QStringLiteral("clipsTotalCard")) != nullptr &&
					tab.findChild<QFrame *>(QStringLiteral("clipsFavoritesCard")) != nullptr &&
					tab.findChild<QFrame *>(QStringLiteral("clipsVerticalCard")) != nullptr &&
					tab.findChild<QFrame *>(QStringLiteral("clipsPendingCard")) != nullptr &&
					tab.findChild<QFrame *>(QStringLiteral("clipsCapturedDurationCard")) != nullptr,
				"ClipsTab must build the five library summary cards");
	clipcoach::test::expect(tab.findChild<QLineEdit *>(QStringLiteral("clipsSearchBox")) != nullptr &&
					tab.findChild<QComboBox *>(QStringLiteral("clipsSort")) != nullptr,
				"ClipsTab must expose title search and sorting above the list");
	clipcoach::test::expect(
		tab.findChild<QWidget *>(QStringLiteral("clipsSummaryCards")) != nullptr &&
			tab.findChild<QFrame *>(QStringLiteral("clipsCompactFilters")) != nullptr,
		"summary indicators and compact filters must share the overview row");
	const auto *metricsLayout =
		tab.findChild<QGridLayout *>(QStringLiteral("clipsMetricsRow"));
	const auto *filtersLayout =
		tab.findChild<QGridLayout *>(QStringLiteral("clipsFiltersGrid"));
	const auto *exportLayout =
		tab.findChild<QGridLayout *>(QStringLiteral("clipsExportGrid"));
	const auto *sort = tab.findChild<QComboBox *>(QStringLiteral("clipsSort"));
	clipcoach::test::expect(metricsLayout != nullptr && filtersLayout != nullptr &&
				       exportLayout != nullptr && sort != nullptr &&
				       exportLayout->indexOf(sort) >= 0,
			       "metrics, wrapping filters and export controls must use responsive layouts");
	clipcoach::test::expect(tab.findChild<QComboBox *>(QStringLiteral("batchExportOrientation")) != nullptr &&
					tab.findChild<QComboBox *>(QStringLiteral("clipsHistoryScope")) != nullptr &&
					tab.findChild<QPushButton *>(QStringLiteral("batchExportButton")) != nullptr &&
					tab.findChild<QPushButton *>(QStringLiteral("cancelExportButton")) != nullptr &&
					tab.findChild<QPushButton *>(QStringLiteral("batchDeleteButton")) != nullptr &&
					tab.findChild<QProgressBar *>(QStringLiteral("exportProgressBar")) != nullptr,
				"ClipsTab must expose batch export, deletion, progress and cancellation");
	auto *exportHost = tab.findChild<QFrame *>(QStringLiteral("clipsExportHost"));
	const auto exportMargins = exportLayout->contentsMargins();
	clipcoach::test::expect(
		exportMargins.top() == clipcoach::ui::tokens::kSpaceXs &&
			exportMargins.bottom() == clipcoach::ui::tokens::kSpaceXs,
		"library toolbar must reserve equal top and bottom margins");
	for (const auto *name :
	     {"batchExportButton", "cancelExportButton", "batchDeleteButton"}) {
		auto *button = tab.findChild<QPushButton *>(QString::fromUtf8(name));
		clipcoach::test::expect(
			button != nullptr &&
				button->height() == clipcoach::ui::tokens::kFooterControlHeight &&
				button->property("toolbarAction").toBool(),
			"library global actions must use the compact toolbar height");
	}
	clipcoach::test::expect(exportHost != nullptr,
				"library global actions must remain inside the toolbar host");
	for (const auto *name :
	     {"clipsFilterAll", "clipsFilterFavorites", "clipsFilterVertical", "clipsFilterPending"}) {
		clipcoach::test::expect(tab.findChild<QPushButton *>(QString::fromUtf8(name)) != nullptr,
					"ClipsTab must build every required filter");
	}
	std::vector<QWidget *> metricCards{
		tab.findChild<QFrame *>(QStringLiteral("clipsTotalCard")),
		tab.findChild<QFrame *>(QStringLiteral("clipsFavoritesCard")),
		tab.findChild<QFrame *>(QStringLiteral("clipsVerticalCard")),
		tab.findChild<QFrame *>(QStringLiteral("clipsPendingCard")),
		tab.findChild<QFrame *>(QStringLiteral("clipsCapturedDurationCard"))};
	std::vector<QWidget *> filterButtons;
	for (const auto *name :
	     {"clipsFilterAll", "clipsFilterFavorites", "clipsFilterVertical", "clipsFilterPending"})
		filterButtons.push_back(tab.findChild<QPushButton *>(QString::fromUtf8(name)));
	expectNoOverlap(metricCards, &tab, "summary indicators must never overlap in a narrow dock");
	expectNoOverlap(filterButtons, &tab, "filters must wrap instead of overlapping in a narrow dock");
	for (auto *card : metricCards)
		clipcoach::test::expect(card != nullptr && card->height() >= 58,
				       "all five indicators must remain readable in a narrow dock");
	clipcoach::test::expect(
		tab.findChild<QWidget *>(QStringLiteral("clipsOverviewHost"))
				->property("layoutMode")
				.toString() == QStringLiteral("compact"),
		"narrow dock must activate the wrapping filter layout");

	tab.setMaximumWidth(QWIDGETSIZE_MAX);
	tab.resize(1300, 820);
	application.processEvents();
	expectNoOverlap(metricCards, &tab, "wide summary indicators must remain separate");
	expectNoOverlap(filterButtons, &tab, "wide filters must remain separate");
	clipcoach::test::expect(
		relativeRect(metricCards.front(), &tab).top() ==
			relativeRect(metricCards.back(), &tab).top(),
		"wide library must arrange all five summary cards in one row");
	clipcoach::test::expect(relativeRect(filterButtons[0], &tab).top() ==
					       relativeRect(filterButtons[3], &tab).top(),
			       "wide dock must arrange all filters in one row");
	clipcoach::test::expect(tab.findChild<QWidget *>(QStringLiteral("clipsListContainer")) != nullptr &&
					tab.findChild<QFrame *>(QStringLiteral("clipsSessionSummaryPanel")) != nullptr,
				"ClipsTab must build its list and session summary panel");
	clipcoach::test::expect(
		tab.findChild<clipcoach::ui::ProBadge *>(QStringLiteral("batchExportProBadge")) != nullptr &&
			tab.findChild<clipcoach::ui::ProBadge *>(QStringLiteral("sessionRecapProBadge")) != nullptr,
		"locked Pro capabilities must remain visible with ProBadge");

	const auto directory = std::filesystem::temp_directory_path() / "clipcoach-export-ui-smoke";
	std::filesystem::create_directories(directory);
	clipcoach::ExportRequest request;
	request.clipId = "ui-clip";
	request.sourcePath = directory / "source.mkv";
	request.outputDirectory = directory;
	request.outputBaseName = "ui";
	request.durationSeconds = 1;
	const auto ids = exports.enqueue(request);
	clipcoach::test::expect(ids.size() == 1 && exports.waitUntilIdle(std::chrono::seconds(2)),
				"UI export fixture must finish");
	QElapsedTimer timeout;
	timeout.start();
	auto *progress = tab.findChild<QProgressBar *>(QStringLiteral("exportProgressBar"));
	while ((progress == nullptr || progress->isHidden()) && timeout.elapsed() < 1000) {
		application.processEvents();
	}
	clipcoach::test::expect(progress != nullptr && !progress->isHidden() && progress->value() == 100,
				"ClipsTab must display completed export progress");
	std::error_code cleanupError;
	std::filesystem::remove_all(directory, cleanupError);

	return clipcoach::test::pass("clips-tab-smoke-test");
}
