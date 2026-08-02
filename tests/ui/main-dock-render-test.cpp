#include "../unit/test-support.hpp"

#include <clipcoach/core/settings-manager.hpp>
#include <clipcoach/core/vertical-canvas-manager.hpp>
#include <clipcoach/ui/main-dock.hpp>

#include <QApplication>
#include <QDir>
#include <QComboBox>
#include <QPixmap>
#include <QSettings>
#include <QTabBar>
#include <QWizard>

#include <array>
#include <filesystem>

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);

	QSettings locale(QStringLiteral(CLIPCOACH_SOURCE_DIR "/data/locale/es-ES.ini"),
			 QSettings::IniFormat);
	const auto translator = [&locale](const char *key) {
		return locale.value(QString::fromUtf8(key), QString::fromUtf8(key)).toString();
	};

	const auto settingsPath =
		std::filesystem::temp_directory_path() /
		"clipcoach-main-dock-render-settings.json";
	std::error_code cleanupError;
	std::filesystem::remove(settingsPath, cleanupError);
	clipcoach::SettingsManager settings(settingsPath);
	std::string error;
	clipcoach::test::expect(settings.load(&error),
			       "render fixture settings must load");
	clipcoach::VerticalCanvasManager vertical(settings, false);
	clipcoach::ui::MainDock dock(translator, nullptr, &settings, nullptr,
				    nullptr, {}, &vertical);
	bool languageChangeRequested = false;
	dock.setLanguageChangedCallback(
		[&languageChangeRequested] { languageChangeRequested = true; });
	dock.resize(520, 820);
	dock.show();
	application.processEvents();

	auto *tabBar = dock.findChild<QTabBar *>(QStringLiteral("mainTabBar"));
	clipcoach::test::expect(tabBar != nullptr, "render test requires the main tab bar");

	const std::array<const char *, 5> names = {
		"vertical",
		"capture",
		"triggers",
		"clips",
		"settings",
	};
	QDir().mkpath(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR));

	for (int index = 0; index < tabBar->count(); ++index) {
		tabBar->setCurrentIndex(index);
		application.processEvents();
		const auto image = dock.grab();
		clipcoach::test::expect(!image.isNull(), "each tab must render to a non-null image");
		const auto outputPath =
			QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/%1.png")
				.arg(QString::fromLatin1(names.at(index)));
		clipcoach::test::expect(image.save(outputPath), "each tab preview must be saved");
	}
	dock.resize(1400, 1000);
	tabBar->setCurrentIndex(0);
	application.processEvents();
	clipcoach::test::expect(
		dock.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/vertical-wide.png")),
		"wide Vertical designer preview must be saved");
	tabBar->setCurrentIndex(1);
	application.processEvents();
	clipcoach::test::expect(
		dock.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/capture-wide.png")),
		"wide Capture dashboard preview must be saved");
	tabBar->setCurrentIndex(2);
	application.processEvents();
	clipcoach::test::expect(
		dock.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/triggers-wide.png")),
		"wide Triggers dashboard preview must be saved");
	tabBar->setCurrentIndex(4);
	application.processEvents();
	clipcoach::test::expect(
		dock.grab().save(QStringLiteral(CLIPCOACH_UI_ARTIFACT_DIR "/settings-wide.png")),
		"wide Settings card grid preview must be saved");
	dock.showInitialSetupIfNeeded();
	auto *setupWizard = dock.findChild<QWizard *>(
		QStringLiteral("clipXtudioInitialSetup"));
	application.processEvents();
	clipcoach::test::expect(
		setupWizard != nullptr &&
			setupWizard->grab().save(QStringLiteral(
				CLIPCOACH_UI_ARTIFACT_DIR "/initial-setup.png")),
		"the five-step onboarding assistant must render to a visual QA artifact");
	if (setupWizard != nullptr)
		setupWizard->reject();
	auto *language =
		dock.findChild<QComboBox *>(QStringLiteral("pluginLanguageCombo"));
	clipcoach::test::expect(language != nullptr,
			       "render fixture must expose the plugin language selector");
	language->setCurrentIndex(2);
	application.processEvents();
	clipcoach::test::expect(
		languageChangeRequested,
		"changing the plugin language must request an immediate dock rebuild");
	std::filesystem::remove(settingsPath, cleanupError);

	return clipcoach::test::pass("main-dock-render-test");
}
