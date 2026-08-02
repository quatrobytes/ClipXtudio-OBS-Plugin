#pragma once

#include <clipcoach/core/feature-gate-service.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace clipcoach::licensing {

using LicenseTimePoint = std::chrono::system_clock::time_point;

enum class LicenseState {
	Free,
	ProActive,
	ProGrace,
};

enum class LicenseApiErrorKind {
	None,
	Network,
	InvalidKey,
	KeyAlreadyUsed,
	SubscriptionInactive,
	DeviceMismatch,
	RateLimited,
	InvalidResponse,
	Server,
};

struct LicenseApiError {
	LicenseApiErrorKind kind{LicenseApiErrorKind::None};
	std::string code;
	std::string message;
	bool retryable{false};
};

struct DeviceIdentity {
	std::string installId;
	std::string fingerprintHash;
};

struct ActivationRequest {
	std::string licenseKey;
	std::string machineFingerprintHash;
	std::string installId;
	std::string requestNonce;
};

struct RefreshRequest {
	std::string refreshToken;
	std::string licenseToken;
	std::string machineFingerprintHash;
	std::string installId;
	std::string requestNonce;
};

struct LicenseServerResponse {
	std::string licenseToken;
	std::string refreshToken;
	std::string status;
	std::optional<LicenseTimePoint> renewsAt;
	std::optional<std::uint32_t> monthlyUsage;
	std::optional<std::uint32_t> monthlyLimit;
};

struct LicenseApiResult {
	std::optional<LicenseServerResponse> response;
	LicenseApiError error;

	[[nodiscard]] bool succeeded() const noexcept
	{
		return response.has_value() && error.kind == LicenseApiErrorKind::None;
	}
};

struct SignedLicenseClaims {
	std::string issuer;
	std::string audience;
	std::string activationId;
	std::string licenseId;
	std::string tokenId;
	std::uint32_t tokenVersion{0};
	std::string plan;
	std::string subscriptionStatus;
	bool perpetual{false};
	std::string machineFingerprintHash;
	std::string installId;
	LicenseTimePoint issuedAt{};
	LicenseTimePoint expiresAt{};
	std::optional<LicenseTimePoint> graceEndsAt;
	std::vector<std::string> entitlements;
};

enum class TokenVerificationError {
	None,
	Malformed,
	UnsupportedAlgorithm,
	UnknownKey,
	InvalidSignature,
	InvalidClaims,
};

struct TokenVerificationResult {
	std::optional<SignedLicenseClaims> claims;
	TokenVerificationError error{TokenVerificationError::None};
	std::string message;

	[[nodiscard]] bool succeeded() const noexcept
	{
		return claims.has_value() && error == TokenVerificationError::None;
	}
};

struct LicenseSnapshot {
	LicenseState state{LicenseState::Free};
	std::string activationId;
	std::string installId;
	std::optional<LicenseTimePoint> tokenExpiresAt;
	std::optional<LicenseTimePoint> graceEndsAt;
	std::optional<LicenseTimePoint> renewsAt;
	std::optional<std::uint32_t> monthlyUsage;
	std::optional<std::uint32_t> monthlyLimit;
	std::string lastErrorCode;
	std::string lastErrorMessage;
	bool refreshInProgress{false};
	bool secureStorageAvailable{true};
	bool perpetual{false};

	[[nodiscard]] bool proEnabled() const noexcept
	{
		return state == LicenseState::ProActive || state == LicenseState::ProGrace;
	}
};

} // namespace clipcoach::licensing
