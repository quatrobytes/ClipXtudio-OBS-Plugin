#pragma once

#include <clipcoach/licensing/license-manager.hpp>

#include <QUrl>
#include <QWidget>

#include <functional>

class QLabel;
class QLineEdit;
class QPushButton;

namespace clipcoach::ui {

class ProAccountTab final : public QWidget {
public:
	using TranslationFunction = std::function<QString(const char *)>;

	ProAccountTab(TranslationFunction translator, licensing::LicenseManager *licenseManager, QUrl membershipUrl,
		      QWidget *parent = nullptr);
	~ProAccountTab() override;

private:
	[[nodiscard]] QString text(const char *key) const;
	void activate();
	void toggleLicenseChange();
	void update(const licensing::LicenseSnapshot &snapshot);
	[[nodiscard]] QString formatDate(const std::optional<licensing::LicenseTimePoint> &value) const;

	TranslationFunction translator_;
	licensing::LicenseManager *licenseManager_{nullptr};
	licensing::LicenseManager::ObserverId observerId_{0};
	QUrl membershipUrl_;
	QLabel *planValue_{nullptr};
	QLabel *expirationValue_{nullptr};
	QLabel *usageValue_{nullptr};
	QLabel *deviceValue_{nullptr};
	QLabel *message_{nullptr};
	QLineEdit *licenseKey_{nullptr};
	QPushButton *activateButton_{nullptr};
	QPushButton *changeLicenseButton_{nullptr};
	QPushButton *manageButton_{nullptr};
	bool changingLicense_{false};
};

} // namespace clipcoach::ui
