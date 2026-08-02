#pragma once

#include <clipcoach/core/settings.hpp>
#include <clipcoach/ui/vertical-obs-bridge.hpp>
#include <clipcoach/remote/remote-clipper-types.hpp>

#include <QWidget>
#include <functional>
#include <vector>

class QCheckBox;
class QComboBox;
class QFrame;
class QGridLayout;
class QKeySequenceEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QResizeEvent;
class QSpinBox;
class QVBoxLayout;

namespace clipcoach {
class SettingsManager;
namespace licensing {
class LicenseManager;
}
namespace integrations {
class ChatIntegrationManager;
}
} // namespace clipcoach

namespace clipcoach::ui {

class SettingsController;
class SettingsSection;

class SettingsTab final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;
	using AppliedCallback = std::function<void(const Settings &before, const Settings &after)>;
	using RemoteAuthenticationRequestedCallback = std::function<void()>;
	using RemoteClipperOpenRequestedCallback = std::function<void()>;
	using ProfileImportedCallback = std::function<void()>;
	using SetupRequestedCallback = std::function<void()>;

	SettingsTab(TranslationFunction translator, SettingsManager *manager, QWidget *parent = nullptr,
		    integrations::ChatIntegrationManager *chatManager = nullptr,
		    licensing::LicenseManager *licenseManager = nullptr,
		    VerticalObsBridge replayObsBridge = {});

	void setAppliedCallback(AppliedCallback callback);
	void setRemoteAuthenticationRequestedCallback(RemoteAuthenticationRequestedCallback callback);
	void setRemoteClipperOpenRequestedCallback(RemoteClipperOpenRequestedCallback callback);
	void setProfileImportedCallback(ProfileImportedCallback callback);
	void setSetupRequestedCallback(SetupRequestedCallback callback);
	[[nodiscard]] bool exportProfileTo(const std::filesystem::path &path,
					   std::string *error = nullptr) const;
	[[nodiscard]] bool importProfileFrom(const std::filesystem::path &path,
					     std::string *error = nullptr);
	void setRemoteAuthenticationBusy(bool busy);
	void refreshLicenseState();
	void setRemoteClipperStatus(const remote::RemoteClipperStatus &status);
	void setRemoteCommandsEnabled(bool enabled);

protected:
	void resizeEvent(QResizeEvent *event) override;

private:
	[[nodiscard]] QString text(const char *key) const;
	void buildUi();
	void bindUi();
	QCheckBox *addToggle(SettingsSection *section, const char *labelKey, const QString &objectName, bool checked);
	QSpinBox *addSpin(SettingsSection *section, const char *labelKey, const QString &objectName, int value,
			  int minimum, int maximum);
	QComboBox *addCombo(SettingsSection *section, const char *labelKey, const QString &objectName,
			    const QStringList &options, int current);
	QLineEdit *addPath(SettingsSection *section, const char *labelKey, const QString &objectName,
			   const std::filesystem::path &value);
	QLineEdit *addText(SettingsSection *section, const char *labelKey, const QString &objectName,
			   const QString &value);
	QKeySequenceEdit *addHotkey(SettingsSection *section, const char *labelKey, const QString &objectName,
				    const std::string &value);
	SettingsSection *addSection(const char *titleKey, const QString &objectName);
	void chooseDirectory(QLineEdit *field,
			     const std::function<void(Settings &, const std::filesystem::path &)> &setter);
	void openDirectory(QLineEdit *field);
	void showError();
	void updateResponsiveLayout();
	void refreshReplayProfile();
	void showReplayRestartDialog();

	TranslationFunction translator_;
	SettingsController *controller_{nullptr};
	SettingsManager *settingsManager_{nullptr};
	QLabel *errorLabel_{nullptr};
	QLabel *widthHint_{nullptr};
	QWidget *sectionsContainer_{nullptr};
	QGridLayout *sectionsGrid_{nullptr};
	std::vector<SettingsSection *> sections_;
	AppliedCallback appliedCallback_;
	RemoteAuthenticationRequestedCallback remoteAuthenticationRequestedCallback_;
	RemoteClipperOpenRequestedCallback remoteClipperOpenRequestedCallback_;
	ProfileImportedCallback profileImportedCallback_;
	SetupRequestedCallback setupRequestedCallback_;
	QLabel *profileStatus_{nullptr};
	integrations::ChatIntegrationManager *chatManager_{nullptr};
	licensing::LicenseManager *licenseManager_{nullptr};
	VerticalObsBridge replayObsBridge_;
	QComboBox *replayEncoder_{nullptr};
	QCheckBox *replayEnabled_{nullptr};
	QPushButton *applyReplayProfile_{nullptr};
	QLabel *replayProfileStatus_{nullptr};
	QLabel *remoteConnectionStatus_{nullptr};
	QLabel *remoteSessionStatus_{nullptr};
	QLabel *remoteHeartbeatStatus_{nullptr};
	QPushButton *remoteAuthenticateButton_{nullptr};
	QLabel *remoteAuthenticationMessage_{nullptr};
	bool remoteAuthenticationPending_{false};
	bool profileImportChangedLanguage_{false};
	bool compactLayout_{false};
	int sectionColumns_{0};
};

} // namespace clipcoach::ui
