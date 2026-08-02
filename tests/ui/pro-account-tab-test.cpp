#include <clipcoach/licensing/license-manager.hpp>
#include <clipcoach/ui/pro-account-tab.hpp>
#include "../unit/test-support.hpp"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <map>
#include <optional>

namespace {

using namespace clipcoach::licensing;
using namespace clipcoach::security;

class MemoryStorage final : public SecureStorage {
public:
	bool store(std::string_view key, const SecretBytes &value, std::string *) override
	{
		values[std::string(key)] = value;
		return true;
	}
	SecretReadResult load(std::string_view key) override
	{
		const auto found = values.find(std::string(key));
		return found == values.end() ? SecretReadResult{{}, SecureStorageError::NotFound, {}}
					     : SecretReadResult{found->second, SecureStorageError::None, {}};
	}
	bool remove(std::string_view key, std::string *) override
	{
		values.erase(std::string(key));
		return true;
	}
	std::map<std::string, SecretBytes> values;
};

class Identity final : public MachineFingerprintProvider {
public:
	std::optional<DeviceIdentity> identity(std::string *) override
	{
		return DeviceIdentity{"971a1d9e-d0f0-4e28-9483-1fc3b84c0c64", std::string(64, 'a')};
	}
};

class Verifier final : public TokenVerifier {
public:
	TokenVerificationResult verify(std::string_view) const override
	{
		return {claims, TokenVerificationError::None, {}};
	}
	SignedLicenseClaims claims;
};

class Api final : public LicenseApi {
public:
	void activate(ActivationRequest request, Completion completion) override
	{
		receivedKey = std::move(request.licenseKey);
		if (activationError) {
			completion({{}, *activationError});
			return;
		}
		completion({LicenseServerResponse{"signed", "refresh", "active", {}, 4U, 50U}, {}});
	}
	void refresh(RefreshRequest, Completion completion) override
	{
		completion({{}, {LicenseApiErrorKind::Network, "NETWORK_ERROR", "offline", true}});
	}
	std::string receivedKey;
	std::optional<LicenseApiError> activationError;
};

} // namespace

int main(int argc, char **argv)
{
	using namespace clipcoach::licensing;
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	QApplication application(argc, argv);
	const auto now = LicenseTimePoint(std::chrono::seconds(2'000'000'000));
	MemoryStorage storage;
	Identity identity;
	Verifier verifier;
	verifier.claims.plan = "pro";
	verifier.claims.subscriptionStatus = "active";
	verifier.claims.activationId = "activation";
	verifier.claims.tokenId = "token";
	verifier.claims.machineFingerprintHash = std::string(64, 'a');
	verifier.claims.installId = "971a1d9e-d0f0-4e28-9483-1fc3b84c0c64";
	verifier.claims.issuedAt = now - std::chrono::minutes(1);
	verifier.claims.expiresAt = now + std::chrono::hours(24);
	verifier.claims.perpetual = true;
	Api api;
	LicenseManager manager(storage, api, verifier, identity, [now] { return now; });
	clipcoach::test::expect(manager.initialize(), "license UI fixture must initialize");
	clipcoach::ui::ProAccountTab tab([](const char *key) { return QString::fromUtf8(key); }, &manager,
					 QUrl(QStringLiteral("https://example.com/account")));
	tab.show();
	application.processEvents();

	auto *key = tab.findChild<QLineEdit *>(QStringLiteral("licenseKeyInput"));
	auto *activate = tab.findChild<QPushButton *>(QStringLiteral("activateLicenseButton"));
	auto *change = tab.findChild<QPushButton *>(QStringLiteral("changeLicenseButton"));
	auto *plan = tab.findChild<QLabel *>(QStringLiteral("proPlanStatus"));
	clipcoach::test::expect(key != nullptr && activate != nullptr && change != nullptr && plan != nullptr,
				"Pro screen must expose key field, activation, replacement and plan status");
	key->setText(QStringLiteral("CCS1-AAAAA-BBBBB-CCCCC-DDDDD"));
	activate->click();
	application.processEvents();
	clipcoach::test::expect(api.receivedKey == "CCS1-AAAAA-BBBBB-CCCCC-DDDDD",
				"Activate button must call LicenseManager with the key");
	clipcoach::test::expect(manager.snapshot().proEnabled() && !activate->isVisible(),
				"successful activation must show Pro and hide key entry");
	clipcoach::test::expect(change->isVisible(), "an active Pro account must expose the change-license action");
	change->click();
	application.processEvents();
	clipcoach::test::expect(key->isVisible() && activate->isVisible(),
				"change license must reveal the validated backend activation form");
	key->setText(QStringLiteral("not-a-license"));
	activate->click();
	application.processEvents();
	clipcoach::test::expect(api.receivedKey == "CCS1-AAAAA-BBBBB-CCCCC-DDDDD",
				"malformed replacement keys must be rejected before reaching the backend");
	key->setText(QStringLiteral("CCS1-11111-22222-33333-44444"));
	activate->click();
	application.processEvents();
	clipcoach::test::expect(api.receivedKey == "CCS1-11111-22222-33333-44444" && manager.snapshot().proEnabled() &&
					!activate->isVisible(),
				"a valid replacement key must be verified by LicenseManager and close the form");
	auto *expiration = tab.findChild<QLabel *>(QStringLiteral("licenseExpirationValue"));
	auto *usage = tab.findChild<QLabel *>(QStringLiteral("monthlyUsageValue"));
	clipcoach::test::expect(expiration != nullptr && expiration->text() == QStringLiteral("Settings.Pro.DateNever"),
				"founder license must display that it never expires");
	clipcoach::test::expect(usage != nullptr && usage->text() == QStringLiteral("Settings.Pro.UsageUnlimited"),
				"founder license must ignore stale monthly counters and display unlimited usage");

	const std::string expiredMessage = "Your Pro subscription expired. ClipCoach has returned to Free.";
	manager.forceFree("SUBSCRIPTION_INACTIVE", expiredMessage);
	application.processEvents();
	auto *message = tab.findChild<QLabel *>(QStringLiteral("licenseStatusMessage"));
	clipcoach::test::expect(message != nullptr && message->isVisible() &&
					message->text().toStdString() == expiredMessage,
				"expired subscription must show the backend downgrade message");
	clipcoach::test::expect(!manager.snapshot().proEnabled() && activate->isVisible(),
				"expired subscription must return the UI and core gates to Free");

	api.activationError = LicenseApiError{
		LicenseApiErrorKind::Server,
		"LICENSE_SIGNING_UNAVAILABLE",
		"License signing keys are not configured.",
		true,
	};
	key->setText(QStringLiteral("CCS1-AAAAA-BBBBB-CCCCC-DDDDD"));
	activate->click();
	application.processEvents();
	clipcoach::test::expect(message->text() == QStringLiteral("Settings.Pro.LicenseActivationUnavailable"),
				"signing configuration failures must show a safe localized availability message");

	return 0;
}
