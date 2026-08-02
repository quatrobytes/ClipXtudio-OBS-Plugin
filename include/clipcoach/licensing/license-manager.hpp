#pragma once

#include <clipcoach/licensing/license-api.hpp>
#include <clipcoach/licensing/machine-fingerprint.hpp>
#include <clipcoach/licensing/token-verifier.hpp>
#include <clipcoach/security/secure-storage.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace clipcoach::licensing {

class LicenseManager final {
public:
	using Clock = std::function<LicenseTimePoint()>;
	using Completion = std::function<void(const LicenseSnapshot &)>;
	using Observer = std::function<void(const LicenseSnapshot &)>;
	using ObserverId = std::uint64_t;

	LicenseManager(security::SecureStorage &secureStorage, LicenseApi &api, const TokenVerifier &tokenVerifier,
		       MachineFingerprintProvider &fingerprintProvider, Clock clock = std::chrono::system_clock::now,
		       std::chrono::seconds offlineGrace = std::chrono::hours(72),
		       std::chrono::seconds clockDrift = std::chrono::minutes(5));
	~LicenseManager();

	[[nodiscard]] bool initialize(std::string *error = nullptr);
	void activate(std::string licenseKey, Completion completion = {});
	void refresh(Completion completion = {});
	void periodicRefresh();
	void forceFree(std::string code, std::string message);

	[[nodiscard]] LicenseSnapshot snapshot() const;
	[[nodiscard]] bool isFeatureAllowed(Feature feature) const;
	[[nodiscard]] bool shouldRefresh() const;
	[[nodiscard]] std::string authorizationToken() const;

	[[nodiscard]] ObserverId addObserver(Observer observer);
	void removeObserver(ObserverId id);

	static constexpr std::string_view kCredentialStorageKey = "pro-license-credentials-v1";

private:
	struct Credentials {
		std::string licenseToken;
		std::string refreshToken;
		std::optional<LicenseTimePoint> renewsAt;
		std::optional<std::uint32_t> monthlyUsage;
		std::optional<std::uint32_t> monthlyLimit;
	};

	void handleServerResult(LicenseApiResult result, bool activation, Completion completion);
	void finishServerResult(const LicenseSnapshot &value, bool activation, Completion completion);
	[[nodiscard]] bool acceptAndStore(const LicenseServerResponse &response, std::string *error);
	[[nodiscard]] LicenseState evaluateClaims(const SignedLicenseClaims &claims, LicenseTimePoint now) const;
	void clearCredentialsLocked();
	void setErrorLocked(std::string code, std::string message);
	void notifyObservers(const LicenseSnapshot &value);
	[[nodiscard]] static security::SecretBytes serialize(const Credentials &credentials);
	[[nodiscard]] static std::optional<Credentials> deserialize(const security::SecretBytes &bytes);
	[[nodiscard]] static std::string createRequestNonce();

	security::SecureStorage &secureStorage_;
	LicenseApi &api_;
	const TokenVerifier &tokenVerifier_;
	MachineFingerprintProvider &fingerprintProvider_;
	Clock clock_;
	std::chrono::seconds offlineGrace_;
	std::chrono::seconds clockDrift_;

	mutable std::mutex mutex_;
	LicenseSnapshot snapshot_;
	std::optional<Credentials> credentials_;
	std::optional<DeviceIdentity> identity_;
	std::vector<Completion> refreshWaiters_;
	std::map<ObserverId, Observer> observers_;
	ObserverId nextObserverId_{1};
};

} // namespace clipcoach::licensing
