#include <clipcoach/ui/pro-account-tab.hpp>
#include <clipcoach/ui/components/primary-button.hpp>
#include <clipcoach/ui/components/secondary-button.hpp>
#include <clipcoach/ui/ui-strings.hpp>

#include <QDateTime>
#include <QDesktopServices>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QPointer>
#include <QRegularExpression>
#include <QStyle>
#include <QTimeZone>
#include <QVBoxLayout>

namespace clipcoach::ui {
namespace {

bool allowedMembershipUrl(const QUrl &url)
{
	if (!url.isValid() || url.host().isEmpty())
		return false;
	if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
		return true;
#ifdef CLIPX_ALLOW_INSECURE_LOCAL_API
	const auto host = url.host().toLower();
	return url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0 &&
	       (host == QStringLiteral("localhost") || host == QStringLiteral("127.0.0.1") ||
		host == QStringLiteral("::1"));
#else
	return false;
#endif
}

} // namespace

ProAccountTab::ProAccountTab(TranslationFunction translator, licensing::LicenseManager *licenseManager,
			     QUrl membershipUrl, QWidget *parent)
	: QWidget(parent),
	  translator_(std::move(translator)),
	  licenseManager_(licenseManager),
	  membershipUrl_(std::move(membershipUrl))
{
	setObjectName(QStringLiteral("proAccountPanel"));
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);

	auto *form = new QFormLayout();
	planValue_ = new QLabel(this);
	planValue_->setObjectName(QStringLiteral("proPlanStatus"));
	planValue_->setProperty("statusRole", QStringLiteral("plan"));
	expirationValue_ = new QLabel(this);
	expirationValue_->setObjectName(QStringLiteral("licenseExpirationValue"));
	usageValue_ = new QLabel(this);
	usageValue_->setObjectName(QStringLiteral("monthlyUsageValue"));
	deviceValue_ = new QLabel(this);
	deviceValue_->setObjectName(QStringLiteral("activatedDeviceValue"));
	form->addRow(text(strings::kSettingsPlanStatus), planValue_);
	form->addRow(text(strings::kSettingsLicenseExpiration), expirationValue_);
	form->addRow(text(strings::kSettingsMonthlyUsage), usageValue_);
	form->addRow(text(strings::kSettingsActivatedDevice), deviceValue_);
	layout->addLayout(form);

	licenseKey_ = new QLineEdit(this);
	licenseKey_->setObjectName(QStringLiteral("licenseKeyInput"));
	licenseKey_->setEchoMode(QLineEdit::Password);
	licenseKey_->setMaxLength(64);
	licenseKey_->setClearButtonEnabled(true);
	licenseKey_->setPlaceholderText(text(strings::kSettingsLicenseKeyPlaceholder));
	layout->addWidget(licenseKey_);

	activateButton_ = new PrimaryButton(text(strings::kSettingsActivateLicense), this);
	activateButton_->setObjectName(QStringLiteral("activateLicenseButton"));
	layout->addWidget(activateButton_);
	changeLicenseButton_ = new SecondaryButton(text(strings::kSettingsChangeLicense), this);
	changeLicenseButton_->setObjectName(QStringLiteral("changeLicenseButton"));
	layout->addWidget(changeLicenseButton_);
	manageButton_ = new SecondaryButton(text(strings::kSettingsManageMembership), this);
	manageButton_->setObjectName(QStringLiteral("manageMembershipButton"));
	manageButton_->setEnabled(allowedMembershipUrl(membershipUrl_));
	layout->addWidget(manageButton_);

	message_ = new QLabel(this);
	message_->setObjectName(QStringLiteral("licenseStatusMessage"));
	message_->setWordWrap(true);
	message_->hide();
	layout->addWidget(message_);

	connect(activateButton_, &QPushButton::clicked, this, [this] { activate(); });
	connect(changeLicenseButton_, &QPushButton::clicked, this, [this] { toggleLicenseChange(); });
	connect(licenseKey_, &QLineEdit::returnPressed, this, [this] { activate(); });
	connect(manageButton_, &QPushButton::clicked, this, [this] {
		if (allowedMembershipUrl(membershipUrl_))
			QDesktopServices::openUrl(membershipUrl_);
	});

	if (licenseManager_ != nullptr) {
		observerId_ = licenseManager_->addObserver(
			[this](const licensing::LicenseSnapshot &snapshot) { update(snapshot); });
	} else {
		update({});
	}
}

ProAccountTab::~ProAccountTab()
{
	if (licenseManager_ != nullptr && observerId_ != 0)
		licenseManager_->removeObserver(observerId_);
}

QString ProAccountTab::text(const char *key) const
{
	return translator_ ? translator_(key) : QString::fromUtf8(key);
}

void ProAccountTab::activate()
{
	if (licenseManager_ == nullptr)
		return;
	const auto key = licenseKey_->text().trimmed();
	if (key.isEmpty()) {
		message_->setText(text(strings::kSettingsLicenseKeyRequired));
		message_->setProperty("notificationTone", QStringLiteral("error"));
		message_->show();
		return;
	}
	static const QRegularExpression licensePattern(
		QStringLiteral("^CCS1-(?:[A-Fa-f0-9]{5}-){3}[A-Fa-f0-9]{5}$"));
	if (!licensePattern.match(key).hasMatch()) {
		message_->setText(text(strings::kSettingsLicenseKeyInvalid));
		message_->setProperty("notificationTone", QStringLiteral("error"));
		message_->show();
		return;
	}
	activateButton_->setEnabled(false);
	activateButton_->setText(text(strings::kSettingsActivating));
	QPointer<ProAccountTab> self(this);
	licenseManager_->activate(key.toUpper().toStdString(), [self](const licensing::LicenseSnapshot &snapshot) {
		if (self.isNull())
			return;
		if (snapshot.lastErrorCode.empty() && snapshot.state == licensing::LicenseState::ProActive)
			self->changingLicense_ = false;
		self->update(snapshot);
	});
	licenseKey_->clear();
}

void ProAccountTab::toggleLicenseChange()
{
	changingLicense_ = !changingLicense_;
	licenseKey_->clear();
	if (licenseManager_ != nullptr)
		update(licenseManager_->snapshot());
	if (changingLicense_)
		licenseKey_->setFocus();
}

void ProAccountTab::update(const licensing::LicenseSnapshot &snapshot)
{
	if (snapshot.refreshInProgress) {
		planValue_->setText(text(strings::kPlanValidating));
	} else {
	switch (snapshot.state) {
	case licensing::LicenseState::ProActive:
		planValue_->setText(text(strings::kPlanProActive));
		break;
	case licensing::LicenseState::ProGrace:
		planValue_->setText(text(strings::kPlanProGrace));
		break;
	case licensing::LicenseState::Free:
		planValue_->setText(text(strings::kPlanFree));
		break;
	}
	}
	expirationValue_->setText(snapshot.perpetual
					  ? text(strings::kSettingsDateNever)
					  : (snapshot.renewsAt ? formatDate(snapshot.renewsAt)
							      : formatDate(snapshot.tokenExpiresAt)));
	if (snapshot.perpetual) {
		usageValue_->setText(text(strings::kSettingsUsageUnlimited));
	} else if (snapshot.monthlyUsage && snapshot.monthlyLimit) {
		usageValue_->setText(
			text(strings::kSettingsUsageFormat).arg(*snapshot.monthlyUsage).arg(*snapshot.monthlyLimit));
	} else {
		usageValue_->setText(text(strings::kSettingsUsageUnavailable));
	}
	deviceValue_->setText(
		snapshot.installId.empty()
			? text(strings::kSettingsDeviceUnavailable)
			: text(strings::kSettingsThisDevice)
				  .arg(QString::fromStdString(snapshot.installId.substr(
					  snapshot.installId.size() > 8 ? snapshot.installId.size() - 8 : 0))));

	activateButton_->setText(text(strings::kSettingsActivateLicense));
	activateButton_->setEnabled(!snapshot.refreshInProgress && snapshot.secureStorageAvailable);
	licenseKey_->setEnabled(activateButton_->isEnabled());
	const bool showActivation = !snapshot.proEnabled() || changingLicense_;
	licenseKey_->setVisible(showActivation);
	activateButton_->setVisible(showActivation);
	changeLicenseButton_->setVisible(snapshot.proEnabled());
	changeLicenseButton_->setEnabled(!snapshot.refreshInProgress && snapshot.secureStorageAvailable);
	changeLicenseButton_->setText(text(changingLicense_ ? strings::kSettingsCancelLicenseChange
							 : strings::kSettingsChangeLicense));

	if (!snapshot.lastErrorMessage.empty()) {
		const auto errorCode = QString::fromStdString(snapshot.lastErrorCode);
		message_->setText(
			errorCode == QStringLiteral("LICENSE_SIGNING_UNAVAILABLE") ||
					errorCode == QStringLiteral("SERVER_MISCONFIGURED")
				? text(strings::kSettingsLicenseActivationUnavailable)
				: QString::fromStdString(snapshot.lastErrorMessage));
		message_->setProperty("notificationTone", QStringLiteral("error"));
		message_->style()->unpolish(message_);
		message_->style()->polish(message_);
		message_->show();
	} else if (snapshot.state == licensing::LicenseState::ProGrace) {
		message_->setText(text(strings::kSettingsOfflineGrace));
		message_->setProperty("notificationTone", QStringLiteral("warning"));
		message_->show();
	} else {
		message_->hide();
	}
}

QString ProAccountTab::formatDate(const std::optional<licensing::LicenseTimePoint> &value) const
{
	if (!value)
		return text(strings::kSettingsDateUnavailable);
	const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(value->time_since_epoch()).count();
	return QLocale().toString(QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::UTC).toLocalTime(),
				  QLocale::ShortFormat);
}

} // namespace clipcoach::ui
