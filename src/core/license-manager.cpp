#include <clipcoach/licensing/license-manager.hpp>
#include <clipcoach/licensing/feature-gate.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

namespace clipcoach::licensing {
namespace {

constexpr std::array<std::uint8_t, 5> kCredentialMagic{'C', 'C', 'S', 'L', 1};
#if defined(CLIPX_ENABLE_QA_LICENSE)
constexpr std::string_view kQaLicenseKey = "CLIPX-QA-PRO-2026-LOCAL";
constexpr std::string_view kQaStorageKey = "qa-pro-entitlement-v1";
constexpr auto kQaEntitlementDuration = std::chrono::hours(24 * 30);
#endif
constexpr std::size_t kMaximumCredentialBytes = 16 * 1024;
constexpr std::size_t kMaximumFieldBytes = 8 * 1024;

void secureClear(std::string &value)
{
	std::fill(value.begin(), value.end(), '\0');
	value.clear();
}

void appendUint32(security::SecretBytes &output, std::uint32_t value)
{
	for (int shift = 0; shift < 32; shift += 8)
		output.push_back(static_cast<std::uint8_t>((value >> static_cast<unsigned int>(shift)) & 0xffU));
}

void appendInt64(security::SecretBytes &output, std::int64_t value)
{
	const auto encoded = static_cast<std::uint64_t>(value);
	for (int shift = 0; shift < 64; shift += 8)
		output.push_back(static_cast<std::uint8_t>((encoded >> static_cast<unsigned int>(shift)) & 0xffU));
}

void appendString(security::SecretBytes &output, const std::string &value)
{
	appendUint32(output, static_cast<std::uint32_t>(value.size()));
	output.insert(output.end(), value.begin(), value.end());
}

bool readUint32(const security::SecretBytes &input, std::size_t &offset, std::uint32_t &value)
{
	if (offset + 4 > input.size())
		return false;
	value = 0;
	for (int shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(input[offset++]) << static_cast<unsigned int>(shift);
	return true;
}

bool readInt64(const security::SecretBytes &input, std::size_t &offset, std::int64_t &value)
{
	if (offset + 8 > input.size())
		return false;
	std::uint64_t encoded = 0;
	for (int shift = 0; shift < 64; shift += 8)
		encoded |= static_cast<std::uint64_t>(input[offset++]) << static_cast<unsigned int>(shift);
	value = static_cast<std::int64_t>(encoded);
	return true;
}

bool readString(const security::SecretBytes &input, std::size_t &offset, std::string &value)
{
	std::uint32_t size = 0;
	if (!readUint32(input, offset, size) || size > kMaximumFieldBytes || offset + size > input.size())
		return false;
	value.assign(reinterpret_cast<const char *>(input.data() + offset), size);
	offset += size;
	return true;
}

std::int64_t epochSeconds(const std::optional<LicenseTimePoint> &value)
{
	if (!value)
		return -1;
	return std::chrono::duration_cast<std::chrono::seconds>(value->time_since_epoch()).count();
}

std::optional<LicenseTimePoint> optionalTime(std::int64_t value)
{
	if (value < 0)
		return std::nullopt;
	return LicenseTimePoint(std::chrono::seconds(value));
}

std::int64_t optionalNumber(const std::optional<std::uint32_t> &value)
{
	return value ? static_cast<std::int64_t>(*value) : -1;
}

std::optional<std::uint32_t> optionalUint32(std::int64_t value)
{
	if (value < 0 || value > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()))
		return std::nullopt;
	return static_cast<std::uint32_t>(value);
}

} // namespace

LicenseManager::LicenseManager(security::SecureStorage &secureStorage, LicenseApi &api,
			       const TokenVerifier &tokenVerifier, MachineFingerprintProvider &fingerprintProvider,
			       Clock clock, std::chrono::seconds offlineGrace, std::chrono::seconds clockDrift)
	: secureStorage_(secureStorage),
	  api_(api),
	  tokenVerifier_(tokenVerifier),
	  fingerprintProvider_(fingerprintProvider),
	  clock_(std::move(clock)),
	  offlineGrace_(std::max(std::chrono::seconds::zero(), offlineGrace)),
	  clockDrift_(std::max(std::chrono::seconds::zero(), clockDrift))
{
}

LicenseManager::~LicenseManager()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (credentials_) {
		secureClear(credentials_->licenseToken);
		secureClear(credentials_->refreshToken);
	}
}

bool LicenseManager::initialize(std::string *error)
{
	std::string identityError;
	auto identity = fingerprintProvider_.identity(&identityError);
	if (!identity) {
		std::lock_guard<std::mutex> lock(mutex_);
		setErrorLocked("DEVICE_IDENTITY_UNAVAILABLE", identityError);
		if (error)
			*error = identityError;
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(mutex_);
		identity_ = std::move(identity);
		snapshot_.installId = identity_->installId;
	}

#if defined(CLIPX_ENABLE_QA_LICENSE)
	const auto restoreQaEntitlement = [this]() {
		auto storedQa = secureStorage_.load(kQaStorageKey);
		if (!storedQa.succeeded() || !storedQa.value)
			return true;

		const std::string cache(storedQa.value->begin(), storedQa.value->end());
		std::fill(storedQa.value->begin(), storedQa.value->end(), 0);
		const auto separator = cache.find('|');
		try {
			const auto expiresSeconds = std::stoll(cache.substr(0, separator));
			const auto cachedInstallId = separator == std::string::npos ? std::string{}
										    : cache.substr(separator + 1);
			const auto storedExpiresAt = LicenseTimePoint(std::chrono::seconds(expiresSeconds));
			if (identity_ && cachedInstallId == identity_->installId) {
				const auto expiresAt = std::max(storedExpiresAt, clock_() + kQaEntitlementDuration);
				const auto renewedSeconds =
					std::chrono::duration_cast<std::chrono::seconds>(expiresAt.time_since_epoch())
						.count();
				const auto renewedCache = std::to_string(renewedSeconds) + "|" + cachedInstallId;
				const security::SecretBytes renewedSecret(renewedCache.begin(), renewedCache.end());
				std::string storageError;
				(void)secureStorage_.store(kQaStorageKey, renewedSecret, &storageError);
				LicenseSnapshot initialized;
				{
					std::lock_guard<std::mutex> lock(mutex_);
					snapshot_.state = LicenseState::ProActive;
					snapshot_.activationId = "clipx-local-qa";
					snapshot_.tokenExpiresAt = expiresAt;
					snapshot_.graceEndsAt = expiresAt;
					snapshot_.renewsAt = expiresAt;
					snapshot_.monthlyUsage = 0;
					snapshot_.monthlyLimit = 1000;
					initialized = snapshot_;
				}
				notifyObservers(initialized);
				return true;
			}
		} catch (...) {
		}
		(void)secureStorage_.remove(kQaStorageKey);
		return true;
	};
#endif

	auto stored = secureStorage_.load(kCredentialStorageKey);
	if (stored.error == security::SecureStorageError::NotFound) {
#if defined(CLIPX_ENABLE_QA_LICENSE)
		return restoreQaEntitlement();
#else
		return true;
#endif
	}
	if (!stored.succeeded() || !stored.value) {
		std::lock_guard<std::mutex> lock(mutex_);
		snapshot_.secureStorageAvailable = false;
		setErrorLocked("SECURE_STORAGE_UNAVAILABLE", stored.message);
		if (error)
			*error = stored.message;
		return false;
	}

	auto credentials = deserialize(*stored.value);
	std::fill(stored.value->begin(), stored.value->end(), 0);
	if (!credentials) {
		(void)secureStorage_.remove(kCredentialStorageKey);
		std::lock_guard<std::mutex> lock(mutex_);
		setErrorLocked("LICENSE_CACHE_INVALID", "Stored license credentials are invalid");
		if (error)
			*error = snapshot_.lastErrorMessage;
		return false;
	}

	const auto verified = tokenVerifier_.verify(credentials->licenseToken);
	if (!verified.succeeded()) {
		secureClear(credentials->licenseToken);
		secureClear(credentials->refreshToken);
		(void)secureStorage_.remove(kCredentialStorageKey);
		std::lock_guard<std::mutex> lock(mutex_);
		setErrorLocked("LICENSE_TOKEN_INVALID", verified.message);
		if (error)
			*error = snapshot_.lastErrorMessage;
		return false;
	}

	LicenseSnapshot initialized;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!identity_ || verified.claims->machineFingerprintHash != identity_->fingerprintHash ||
		    verified.claims->installId != identity_->installId) {
			secureClear(credentials->licenseToken);
			secureClear(credentials->refreshToken);
			setErrorLocked("TOKEN_DEVICE_MISMATCH", "Stored license belongs to another device");
			if (error)
				*error = snapshot_.lastErrorMessage;
			(void)secureStorage_.remove(kCredentialStorageKey);
			return false;
		}
		const auto state = evaluateClaims(*verified.claims, clock_());
		if (state == LicenseState::Free) {
			// The access token can expire while OBS is closed. Keep the separately
			// revocable refresh credential so startup can renew online without
			// asking the user to enter the one-time license key again.
			credentials_ = std::move(credentials);
			snapshot_.state = LicenseState::Free;
			snapshot_.activationId = verified.claims->activationId;
			snapshot_.tokenExpiresAt = verified.claims->expiresAt;
			snapshot_.perpetual = verified.claims->perpetual;
			snapshot_.renewsAt = credentials_->renewsAt;
			snapshot_.monthlyUsage = credentials_->monthlyUsage;
			snapshot_.monthlyLimit = credentials_->monthlyLimit;
			setErrorLocked("LICENSE_REFRESH_REQUIRED", "Stored license needs an online refresh");
			initialized = snapshot_;
		} else {
			credentials_ = std::move(credentials);
			snapshot_.state = state;
			snapshot_.activationId = verified.claims->activationId;
			snapshot_.tokenExpiresAt = verified.claims->expiresAt;
			snapshot_.perpetual = verified.claims->perpetual;
			snapshot_.graceEndsAt =
				verified.claims->graceEndsAt
					? verified.claims->graceEndsAt
					: std::optional<LicenseTimePoint>(verified.claims->expiresAt + offlineGrace_);
			snapshot_.renewsAt = credentials_->renewsAt;
			snapshot_.monthlyUsage = credentials_->monthlyUsage;
			snapshot_.monthlyLimit = credentials_->monthlyLimit;
			snapshot_.lastErrorCode.clear();
			snapshot_.lastErrorMessage.clear();
			initialized = snapshot_;
		}
	}
	notifyObservers(initialized);
	return true;
}

void LicenseManager::activate(std::string licenseKey, Completion completion)
{
#if defined(CLIPX_ENABLE_QA_LICENSE)
	if (licenseKey == kQaLicenseKey) {
		LicenseSnapshot value;
		LicenseTimePoint expiresAt;
		{
			std::lock_guard<std::mutex> lock(mutex_);
			const auto now = clock_();
			expiresAt = now + kQaEntitlementDuration;
			snapshot_.state = LicenseState::ProActive;
			snapshot_.activationId = "clipx-local-qa";
			snapshot_.tokenExpiresAt = expiresAt;
			snapshot_.graceEndsAt = snapshot_.tokenExpiresAt;
			snapshot_.renewsAt = snapshot_.tokenExpiresAt;
			snapshot_.monthlyUsage = 0;
			snapshot_.monthlyLimit = 1000;
			snapshot_.refreshInProgress = false;
			snapshot_.lastErrorCode.clear();
			snapshot_.lastErrorMessage.clear();
			value = snapshot_;
		}
		const auto expiresSeconds =
			std::chrono::duration_cast<std::chrono::seconds>(expiresAt.time_since_epoch()).count();
		const auto cache = std::to_string(expiresSeconds) + "|" + value.installId;
		const security::SecretBytes secret(cache.begin(), cache.end());
		std::string storageError;
		if (!secureStorage_.store(kQaStorageKey, secret, &storageError)) {
			std::lock_guard<std::mutex> lock(mutex_);
			setErrorLocked("QA_LICENSE_CACHE_FAILED", storageError);
			value = snapshot_;
		}
		notifyObservers(value);
		if (completion)
			completion(value);
		return;
	}
#endif
	ActivationRequest request;
	LicenseSnapshot pending;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!identity_) {
			setErrorLocked("DEVICE_IDENTITY_UNAVAILABLE", "Device identity is unavailable");
			pending = snapshot_;
		} else if (!snapshot_.secureStorageAvailable) {
			setErrorLocked("SECURE_STORAGE_UNAVAILABLE", "Secure storage is required for Pro");
			pending = snapshot_;
		} else {
			snapshot_.refreshInProgress = true;
			snapshot_.lastErrorCode.clear();
			snapshot_.lastErrorMessage.clear();
			request = {std::move(licenseKey), identity_->fingerprintHash, identity_->installId,
				   createRequestNonce()};
			pending = snapshot_;
		}
	}
	secureClear(licenseKey);
	notifyObservers(pending);
	if (request.licenseKey.empty()) {
		if (completion)
			completion(pending);
		return;
	}

	api_.activate(std::move(request), [this, completion = std::move(completion)](LicenseApiResult result) mutable {
		handleServerResult(std::move(result), true, std::move(completion));
	});
}

void LicenseManager::refresh(Completion completion)
{
	RefreshRequest request;
	LicenseSnapshot pending;
	bool waitingForCurrentRefresh = false;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!identity_ || !credentials_ || credentials_->refreshToken.empty()) {
			setErrorLocked("REFRESH_TOKEN_MISSING", "No active license can be refreshed");
			pending = snapshot_;
		} else if (snapshot_.refreshInProgress) {
			if (completion)
				refreshWaiters_.push_back(std::move(completion));
			pending = snapshot_;
			waitingForCurrentRefresh = true;
		} else {
			snapshot_.refreshInProgress = true;
			request = {credentials_->refreshToken, credentials_->licenseToken, identity_->fingerprintHash,
				   identity_->installId, createRequestNonce()};
			pending = snapshot_;
		}
	}
	notifyObservers(pending);
	if (waitingForCurrentRefresh)
		return;
	if (request.refreshToken.empty()) {
		if (completion)
			completion(pending);
		return;
	}
	api_.refresh(std::move(request), [this, completion = std::move(completion)](LicenseApiResult result) mutable {
		handleServerResult(std::move(result), false, std::move(completion));
	});
}

void LicenseManager::periodicRefresh()
{
	if (shouldRefresh())
		refresh();
}

void LicenseManager::forceFree(std::string code, std::string message)
{
	LicenseSnapshot value;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		clearCredentialsLocked();
		setErrorLocked(std::move(code), std::move(message));
		value = snapshot_;
	}
	(void)secureStorage_.remove(kCredentialStorageKey);
#if defined(CLIPX_ENABLE_QA_LICENSE)
	(void)secureStorage_.remove(kQaStorageKey);
#endif
	notifyObservers(value);
}

LicenseSnapshot LicenseManager::snapshot() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return snapshot_;
}

bool LicenseManager::isFeatureAllowed(Feature feature) const
{
	return licensing::isFeatureAllowed(feature, snapshot());
}

bool LicenseManager::shouldRefresh() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!credentials_ || credentials_->refreshToken.empty() || snapshot_.refreshInProgress)
		return false;
	return true;
}

std::string LicenseManager::authorizationToken() const
{
	std::scoped_lock lock(mutex_);
	return snapshot_.proEnabled() && credentials_ ? credentials_->licenseToken : std::string{};
}

LicenseManager::ObserverId LicenseManager::addObserver(Observer observer)
{
	ObserverId id;
	LicenseSnapshot current;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		id = nextObserverId_++;
		observers_.emplace(id, observer);
		current = snapshot_;
	}
	if (observer)
		observer(current);
	return id;
}

void LicenseManager::removeObserver(ObserverId id)
{
	std::lock_guard<std::mutex> lock(mutex_);
	observers_.erase(id);
}

void LicenseManager::handleServerResult(LicenseApiResult result, bool activation, Completion completion)
{
	if (result.succeeded()) {
		std::string validationError;
		if (!acceptAndStore(*result.response, &validationError)) {
			if (!activation) {
				const bool deviceMismatch = validationError.find("another device") != std::string::npos;
				handleServerResult(
					{{},
					 {LicenseApiErrorKind::InvalidResponse,
					  deviceMismatch ? "TOKEN_DEVICE_MISMATCH" : "LICENSE_RESPONSE_INVALID",
					  std::move(validationError), false}},
					false, std::move(completion));
				return;
			}
			LicenseSnapshot value;
			{
				std::lock_guard<std::mutex> lock(mutex_);
				snapshot_.refreshInProgress = false;
				const bool deviceMismatch = validationError.find("another device") != std::string::npos;
				setErrorLocked(deviceMismatch ? "TOKEN_DEVICE_MISMATCH" : "LICENSE_RESPONSE_INVALID",
					       std::move(validationError));
				if (activation && !credentials_)
					snapshot_.state = LicenseState::Free;
				value = snapshot_;
			}
			finishServerResult(value, activation, std::move(completion));
			return;
		}
		const auto value = snapshot();
		finishServerResult(value, activation, std::move(completion));
		return;
	}

	bool removeStored = false;
	LicenseSnapshot value;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		snapshot_.refreshInProgress = false;
		setErrorLocked(result.error.code.empty() ? "LICENSE_API_ERROR" : result.error.code,
			       result.error.message);
		if (result.error.kind == LicenseApiErrorKind::SubscriptionInactive) {
			clearCredentialsLocked();
			removeStored = true;
		} else if (!activation && credentials_) {
			const auto verified = tokenVerifier_.verify(credentials_->licenseToken);
			if (!verified.succeeded() || !identity_ ||
			    verified.claims->machineFingerprintHash != identity_->fingerprintHash ||
			    verified.claims->installId != identity_->installId) {
				clearCredentialsLocked();
				removeStored = true;
			} else {
				snapshot_.state = evaluateClaims(*verified.claims, clock_());
				snapshot_.tokenExpiresAt = verified.claims->expiresAt;
				snapshot_.perpetual = verified.claims->perpetual;
				snapshot_.graceEndsAt = verified.claims->graceEndsAt
								? verified.claims->graceEndsAt
								: std::optional<LicenseTimePoint>(
									  verified.claims->expiresAt + offlineGrace_);
				if (snapshot_.state == LicenseState::Free) {
					clearCredentialsLocked();
					removeStored = true;
				} else if (snapshot_.perpetual && snapshot_.proEnabled() &&
					   result.error.code == "REFRESH_TOKEN_INVALID") {
					// The signed perpetual entitlement remains valid while the server repairs
					// a stale refresh credential. Do not expose an internal token error as if
					// the owner's Pro license had failed.
					snapshot_.lastErrorCode.clear();
					snapshot_.lastErrorMessage.clear();
				}
			}
		}
		value = snapshot_;
	}
	if (removeStored)
		(void)secureStorage_.remove(kCredentialStorageKey);
	finishServerResult(value, activation, std::move(completion));
}

void LicenseManager::finishServerResult(const LicenseSnapshot &value, bool activation, Completion completion)
{
	std::vector<Completion> waiters;
	if (!activation) {
		std::lock_guard<std::mutex> lock(mutex_);
		waiters.swap(refreshWaiters_);
	}

	notifyObservers(value);
	if (completion)
		completion(value);
	for (auto &waiter : waiters) {
		if (waiter)
			waiter(value);
	}
}

bool LicenseManager::acceptAndStore(const LicenseServerResponse &response, std::string *error)
{
	if (response.licenseToken.empty() || response.refreshToken.empty() ||
	    (response.status != "active" && response.status != "grace")) {
		if (error)
			*error = "License response is incomplete";
		return false;
	}
	const auto verified = tokenVerifier_.verify(response.licenseToken);
	if (!verified.succeeded()) {
		if (error)
			*error = verified.message.empty() ? "License token signature is invalid" : verified.message;
		return false;
	}

	Credentials candidate{response.licenseToken, response.refreshToken, response.renewsAt, response.monthlyUsage,
			      response.monthlyLimit};
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!identity_ || verified.claims->machineFingerprintHash != identity_->fingerprintHash ||
		    verified.claims->installId != identity_->installId) {
			secureClear(candidate.licenseToken);
			secureClear(candidate.refreshToken);
			if (error)
				*error = "License token is bound to another device";
			return false;
		}
		if (verified.claims->plan != "pro" || (verified.claims->subscriptionStatus != "active" &&
						       verified.claims->subscriptionStatus != "grace")) {
			secureClear(candidate.licenseToken);
			secureClear(candidate.refreshToken);
			if (error)
				*error = "Signed license claims do not grant Pro";
			return false;
		}
		const auto state = evaluateClaims(*verified.claims, clock_());
		if (state == LicenseState::Free) {
			secureClear(candidate.licenseToken);
			secureClear(candidate.refreshToken);
			if (error)
				*error = "Signed license token is expired";
			return false;
		}

		auto bytes = serialize(candidate);
		std::string storageError;
		const auto stored = secureStorage_.store(kCredentialStorageKey, bytes, &storageError);
		std::fill(bytes.begin(), bytes.end(), 0);
		if (!stored) {
			secureClear(candidate.licenseToken);
			secureClear(candidate.refreshToken);
			snapshot_.secureStorageAvailable = false;
			if (error)
				*error = storageError.empty() ? "Secure storage failed" : storageError;
			return false;
		}

		if (credentials_) {
			secureClear(credentials_->licenseToken);
			secureClear(credentials_->refreshToken);
		}
		credentials_ = std::move(candidate);
		snapshot_.state = state;
		snapshot_.activationId = verified.claims->activationId;
		snapshot_.installId = identity_->installId;
		snapshot_.tokenExpiresAt = verified.claims->expiresAt;
		snapshot_.perpetual = verified.claims->perpetual;
		snapshot_.graceEndsAt =
			verified.claims->graceEndsAt
				? verified.claims->graceEndsAt
				: std::optional<LicenseTimePoint>(verified.claims->expiresAt + offlineGrace_);
		snapshot_.renewsAt = response.renewsAt;
		snapshot_.monthlyUsage = response.monthlyUsage;
		snapshot_.monthlyLimit = response.monthlyLimit;
		snapshot_.lastErrorCode.clear();
		snapshot_.lastErrorMessage.clear();
		snapshot_.refreshInProgress = false;
		snapshot_.secureStorageAvailable = true;
	}
	return true;
}

LicenseState LicenseManager::evaluateClaims(const SignedLicenseClaims &claims, LicenseTimePoint now) const
{
	if (claims.plan != "pro" || (claims.subscriptionStatus != "active" && claims.subscriptionStatus != "grace") ||
	    now + clockDrift_ < claims.issuedAt)
		return LicenseState::Free;

	if (claims.subscriptionStatus == "active" && now <= claims.expiresAt + clockDrift_)
		return LicenseState::ProActive;

	auto offlineEnd = claims.expiresAt + offlineGrace_ + clockDrift_;
	if (claims.graceEndsAt)
		offlineEnd = std::min(offlineEnd, *claims.graceEndsAt + clockDrift_);
	return now <= offlineEnd ? LicenseState::ProGrace : LicenseState::Free;
}

void LicenseManager::clearCredentialsLocked()
{
	if (credentials_) {
		secureClear(credentials_->licenseToken);
		secureClear(credentials_->refreshToken);
		credentials_.reset();
	}
	snapshot_.state = LicenseState::Free;
	snapshot_.activationId.clear();
	snapshot_.tokenExpiresAt.reset();
	snapshot_.graceEndsAt.reset();
	snapshot_.renewsAt.reset();
	snapshot_.monthlyUsage.reset();
	snapshot_.monthlyLimit.reset();
	snapshot_.perpetual = false;
	snapshot_.refreshInProgress = false;
}

void LicenseManager::setErrorLocked(std::string code, std::string message)
{
	snapshot_.lastErrorCode = std::move(code);
	snapshot_.lastErrorMessage = std::move(message);
}

void LicenseManager::notifyObservers(const LicenseSnapshot &value)
{
	std::vector<Observer> observers;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		observers.reserve(observers_.size());
		for (const auto &[id, observer] : observers_) {
			(void)id;
			observers.push_back(observer);
		}
	}
	for (const auto &observer : observers) {
		if (observer)
			observer(value);
	}
}

security::SecretBytes LicenseManager::serialize(const Credentials &credentials)
{
	security::SecretBytes output(kCredentialMagic.begin(), kCredentialMagic.end());
	output.reserve(credentials.licenseToken.size() + credentials.refreshToken.size() + 64);
	appendString(output, credentials.licenseToken);
	appendString(output, credentials.refreshToken);
	appendInt64(output, epochSeconds(credentials.renewsAt));
	appendInt64(output, optionalNumber(credentials.monthlyUsage));
	appendInt64(output, optionalNumber(credentials.monthlyLimit));
	return output;
}

std::optional<LicenseManager::Credentials> LicenseManager::deserialize(const security::SecretBytes &bytes)
{
	if (bytes.size() < kCredentialMagic.size() || bytes.size() > kMaximumCredentialBytes ||
	    !std::equal(kCredentialMagic.begin(), kCredentialMagic.end(), bytes.begin()))
		return std::nullopt;
	std::size_t offset = kCredentialMagic.size();
	Credentials credentials;
	std::int64_t renewsAt = -1;
	std::int64_t usage = -1;
	std::int64_t limit = -1;
	if (!readString(bytes, offset, credentials.licenseToken) ||
	    !readString(bytes, offset, credentials.refreshToken) || !readInt64(bytes, offset, renewsAt) ||
	    !readInt64(bytes, offset, usage) || !readInt64(bytes, offset, limit) || offset != bytes.size() ||
	    credentials.licenseToken.empty() || credentials.refreshToken.empty())
		return std::nullopt;
	credentials.renewsAt = optionalTime(renewsAt);
	credentials.monthlyUsage = optionalUint32(usage);
	credentials.monthlyLimit = optionalUint32(limit);
	return credentials;
}

std::string LicenseManager::createRequestNonce()
{
	return generateInstallId();
}

} // namespace clipcoach::licensing
