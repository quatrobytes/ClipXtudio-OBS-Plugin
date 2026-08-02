#include <clipcoach/licensing/license-manager.hpp>
#include "test-support.hpp"

#include <map>

namespace {

using namespace clipcoach::licensing;
using namespace clipcoach::security;

class MemorySecureStorage final : public SecureStorage {
public:
	bool store(std::string_view key, const SecretBytes &secret, std::string *) override
	{
		values[std::string(key)] = secret;
		return available;
	}

	SecretReadResult load(std::string_view key) override
	{
		if (!available)
			return {{}, SecureStorageError::Unavailable, "unavailable"};
		const auto found = values.find(std::string(key));
		if (found == values.end())
			return {{}, SecureStorageError::NotFound, "not found"};
		return {found->second, SecureStorageError::None, {}};
	}

	bool remove(std::string_view key, std::string *) override
	{
		values.erase(std::string(key));
		return available;
	}

	bool available{true};
	std::map<std::string, SecretBytes> values;
};

class FixedFingerprint final : public MachineFingerprintProvider {
public:
	std::optional<DeviceIdentity> identity(std::string *) override
	{
		return DeviceIdentity{"971a1d9e-d0f0-4e28-9483-1fc3b84c0c64", std::string(64, 'a')};
	}
};

class FakeVerifier final : public TokenVerifier {
public:
	TokenVerificationResult verify(std::string_view token) const override
	{
		const auto found = claims.find(std::string(token));
		if (found == claims.end())
			return {{}, TokenVerificationError::InvalidSignature, "invalid signature"};
		return {found->second, TokenVerificationError::None, {}};
	}

	std::map<std::string, SignedLicenseClaims> claims;
};

class FakeApi final : public LicenseApi {
public:
	void activate(ActivationRequest request, Completion completion) override
	{
		lastActivation = std::move(request);
		completion(nextActivation);
	}

	void refresh(RefreshRequest request, Completion completion) override
	{
		++refreshCalls;
		lastRefresh = std::move(request);
		if (deferRefresh) {
			pendingRefresh = std::move(completion);
			return;
		}
		completion(nextRefresh);
	}

	void completeRefresh()
	{
		auto completion = std::move(pendingRefresh);
		if (completion)
			completion(nextRefresh);
	}

	LicenseApiResult nextActivation;
	LicenseApiResult nextRefresh;
	ActivationRequest lastActivation;
	RefreshRequest lastRefresh;
	Completion pendingRefresh;
	bool deferRefresh{false};
	int refreshCalls{0};
};

SignedLicenseClaims claimsAt(LicenseTimePoint now, std::chrono::seconds ttl)
{
	SignedLicenseClaims claims;
	claims.issuer = "clipcoach-studio";
	claims.audience = "clipcoach-native-plugin";
	claims.activationId = "activation-id";
	claims.tokenId = "token-id";
	claims.plan = "pro";
	claims.subscriptionStatus = "active";
	claims.machineFingerprintHash = std::string(64, 'a');
	claims.installId = "971a1d9e-d0f0-4e28-9483-1fc3b84c0c64";
	claims.issuedAt = now - std::chrono::minutes(1);
	claims.expiresAt = now + ttl;
	return claims;
}

LicenseServerResponse serverResponse(std::string token)
{
	return {std::move(token), "opaque-refresh-token", "active", std::nullopt, 12U, 100U};
}

} // namespace

int main()
{
	using namespace clipcoach::licensing;
	const auto baseTime = LicenseTimePoint(std::chrono::seconds(2'000'000'000));
	auto now = baseTime;

	MemorySecureStorage storage;
	FixedFingerprint fingerprint;
	FakeVerifier verifier;
	FakeApi api;
	verifier.claims["valid"] = claimsAt(now, std::chrono::hours(24));
	api.nextActivation.response = serverResponse("valid");
	LicenseManager manager(storage, api, verifier, fingerprint, [&now] { return now; });

	clipcoach::test::expect(manager.initialize(), "missing cache must initialize as Free");
	clipcoach::test::expect(manager.snapshot().state == LicenseState::Free, "initial plan must be Free");
#if defined(CLIPX_ENABLE_QA_LICENSE)
	manager.activate("CLIPX-QA-PRO-2026-LOCAL");
	clipcoach::test::expect(manager.snapshot().state == LicenseState::ProActive,
				"the explicitly enabled local QA key must unlock Pro");
	clipcoach::test::expect(api.lastActivation.licenseKey.empty(),
				"the local QA key must never be sent to the production API");
	LicenseManager restoredQaManager(storage, api, verifier, fingerprint, [&now] { return now; });
	clipcoach::test::expect(restoredQaManager.initialize() &&
					restoredQaManager.snapshot().state == LicenseState::ProActive,
				"the local QA entitlement must survive an OBS restart in secure storage");
	clipcoach::test::expect(restoredQaManager.authorizationToken().empty(),
				"a local QA entitlement must not invent cloud authorization");
	manager.activate("CCS1-AAAAA-BBBBB-CCCCC-DDDDD");
	clipcoach::test::expect(manager.authorizationToken() == "valid",
				"a signed backend activation must replace local-only authorization");
	LicenseManager restoredSignedOverQa(storage, api, verifier, fingerprint, [&now] { return now; });
	clipcoach::test::expect(restoredSignedOverQa.initialize() &&
					restoredSignedOverQa.snapshot().state == LicenseState::ProActive &&
					restoredSignedOverQa.authorizationToken() == "valid",
				"signed backend credentials must take priority over a cached QA entitlement");
	manager.forceFree({}, {});
#endif
	manager.activate("CCS1-AAAAA-BBBBB-CCCCC-DDDDD");
	clipcoach::test::expect(manager.snapshot().state == LicenseState::ProActive,
				"valid signed activation must enable Pro");
	clipcoach::test::expect(api.lastActivation.machineFingerprintHash == std::string(64, 'a') &&
					api.lastActivation.licenseKey.rfind("CCS1-", 0) == 0,
				"activation must send key and hashed fingerprint");
	clipcoach::test::expect(manager.isFeatureAllowed(clipcoach::Feature::VoiceTrigger),
				"Pro feature gate must open for active license");
	const std::string activatedKey = "CCS1-AAAAA-BBBBB-CCCCC-DDDDD";
	bool plainKeyPersisted = false;
	for (const auto &[name, value] : storage.values) {
		(void)name;
		const std::string stored(value.begin(), value.end());
		plainKeyPersisted = plainKeyPersisted || stored.find(activatedKey) != std::string::npos;
	}
	clipcoach::test::expect(!plainKeyPersisted, "the plaintext activation key must never be persisted");
	LicenseManager restoredManager(storage, api, verifier, fingerprint, [&now] { return now; });
	clipcoach::test::expect(restoredManager.initialize() &&
					restoredManager.snapshot().state == LicenseState::ProActive,
				"a signed Pro activation must survive an OBS update or restart");
	clipcoach::test::expect(restoredManager.snapshot().activationId == "activation-id",
				"the restored entitlement must keep its device activation");
	clipcoach::test::expect(restoredManager.authorizationToken() == "valid",
				"the restored signed entitlement must authorize backend AI requests");

	api.deferRefresh = true;
	api.nextRefresh.response = serverResponse("valid");
	bool startupRefreshCompleted = false;
	bool captionRefreshCompleted = false;
	manager.refresh([&startupRefreshCompleted](const LicenseSnapshot &) {
		startupRefreshCompleted = true;
	});
	manager.refresh([&captionRefreshCompleted](const LicenseSnapshot &) {
		captionRefreshCompleted = true;
	});
	clipcoach::test::expect(api.refreshCalls == 1,
				"concurrent startup and caption synchronization must share one API request");
	clipcoach::test::expect(!startupRefreshCompleted && !captionRefreshCompleted,
				"refresh completions must wait for the shared server result");
	api.completeRefresh();
	clipcoach::test::expect(startupRefreshCompleted && captionRefreshCompleted,
				"all synchronization waiters must resume after the shared refresh completes");
	api.deferRefresh = false;

	now += std::chrono::hours(98);
	LicenseManager staleManager(storage, api, verifier, fingerprint, [&now] { return now; });
	clipcoach::test::expect(staleManager.initialize() && staleManager.snapshot().state == LicenseState::Free,
				"an access token beyond offline grace must not grant offline Pro");
	clipcoach::test::expect(staleManager.shouldRefresh(),
				"an expired access token must retain its refresh credential");
	verifier.claims["renewed"] = claimsAt(now, std::chrono::hours(24));
	api.nextRefresh.response = serverResponse("renewed");
	staleManager.periodicRefresh();
	clipcoach::test::expect(staleManager.snapshot().state == LicenseState::ProActive,
				"startup refresh must restore Pro without asking for the one-time key");

	now = baseTime;
	verifier.claims["wrong-device"] = claimsAt(now, std::chrono::hours(24));
	verifier.claims["wrong-device"].machineFingerprintHash = std::string(64, 'b');
	now += std::chrono::hours(25);
	api.nextRefresh.response = serverResponse("wrong-device");
	manager.refresh();
	clipcoach::test::expect(manager.snapshot().state == LicenseState::ProGrace &&
					manager.snapshot().lastErrorCode == "TOKEN_DEVICE_MISMATCH",
				"invalid refresh response must re-evaluate the last trusted token");

	api.nextRefresh = {{}, {LicenseApiErrorKind::Network, "NETWORK_ERROR", "offline", true}};
	clipcoach::test::expect(manager.isFeatureAllowed(clipcoach::Feature::VerticalCanvas),
				"offline grace must keep gated features available");

	now += std::chrono::hours(73);
	manager.refresh();
	clipcoach::test::expect(manager.snapshot().state == LicenseState::Free,
				"expired offline grace must return to Free");
	clipcoach::test::expect(!manager.isFeatureAllowed(clipcoach::Feature::VoiceTrigger),
				"Free state must close Pro feature gate");

	now = baseTime;
	verifier.claims["wrong-device"] = claimsAt(now, std::chrono::hours(24));
	verifier.claims["wrong-device"].machineFingerprintHash = std::string(64, 'b');
	api.nextActivation.response = serverResponse("wrong-device");
	manager.activate("CCS1-FFFFF-EEEEE-DDDDD-CCCCC");
	const auto wrongDeviceSnapshot = manager.snapshot();
	clipcoach::test::expect(
		manager.snapshot().state == LicenseState::Free &&
			wrongDeviceSnapshot.lastErrorCode == "TOKEN_DEVICE_MISMATCH",
		"manipulated device claims must not enable Pro; code=" + wrongDeviceSnapshot.lastErrorCode +
			" state=" + std::to_string(static_cast<int>(wrongDeviceSnapshot.state)) +
			" message=" + wrongDeviceSnapshot.lastErrorMessage);

	api.nextActivation = {{},
			      {LicenseApiErrorKind::KeyAlreadyUsed, "LICENSE_KEY_ALREADY_USED", "already used", false}};
	manager.activate("CCS1-11111-22222-33333-44444");
	clipcoach::test::expect(manager.snapshot().lastErrorCode == "LICENSE_KEY_ALREADY_USED",
				"API errors must be controlled and visible without crashing");

	MemorySecureStorage ownerStorage;
	FakeVerifier ownerVerifier;
	FakeApi ownerApi;
	auto ownerClaims = claimsAt(now, std::chrono::hours(24));
	ownerClaims.perpetual = true;
	ownerClaims.licenseId = "owner-license-id";
	ownerClaims.tokenVersion = 1;
	ownerVerifier.claims["owner-perpetual"] = ownerClaims;
	ownerApi.nextActivation.response = serverResponse("owner-perpetual");
	LicenseManager ownerManager(ownerStorage, ownerApi, ownerVerifier, fingerprint, [&now] { return now; });
	clipcoach::test::expect(ownerManager.initialize(), "owner manager must initialize");
	ownerManager.activate("CCS1-OWNER-AAAAA-BBBBB-CCCCC");
	clipcoach::test::expect(ownerManager.snapshot().state == LicenseState::ProActive &&
					ownerManager.snapshot().perpetual,
				"a valid perpetual owner license must enable unlimited Pro");
	ownerApi.nextRefresh = {
		{},
		{LicenseApiErrorKind::InvalidKey, "REFRESH_TOKEN_INVALID", "The refresh token is invalid.", false},
	};
	ownerManager.refresh();
	clipcoach::test::expect(ownerApi.lastRefresh.licenseToken == "owner-perpetual",
				"refresh must send the current signed license proof for secure recovery");
	clipcoach::test::expect(ownerManager.snapshot().state == LicenseState::ProActive &&
					ownerManager.snapshot().lastErrorCode.empty() &&
					ownerManager.snapshot().lastErrorMessage.empty(),
				"a stale refresh credential must not expose a contradictory error while "
				"the signed perpetual entitlement remains valid");

	return 0;
}
