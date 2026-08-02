#include <clipcoach/security/rs256-token-verifier.hpp>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/evp.h>
#include <openssl/pem.h>

#include <chrono>
#include <memory>

namespace clipcoach::security {
namespace {

using BioPointer = std::unique_ptr<BIO, decltype(&BIO_free)>;
using KeyPointer = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using DigestPointer = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;

QByteArray decodeBase64Url(std::string_view value)
{
	return QByteArray::fromBase64(QByteArray(value.data(), static_cast<qsizetype>(value.size())),
				      QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
}

licensing::TokenVerificationResult failure(licensing::TokenVerificationError error, std::string message)
{
	return {{}, error, std::move(message)};
}

std::optional<licensing::LicenseTimePoint> parseEpoch(const QJsonObject &object, const QString &name, bool required)
{
	const auto value = object.value(name);
	if (!value.isDouble())
		return required ? std::optional<licensing::LicenseTimePoint>{} : std::nullopt;
	const auto seconds = static_cast<std::int64_t>(value.toDouble());
	if (seconds <= 0)
		return std::nullopt;
	return licensing::LicenseTimePoint(std::chrono::seconds(seconds));
}

} // namespace

Rs256TokenVerifier::Rs256TokenVerifier(std::string publicKeyPem, std::string issuer, std::string audience,
				       std::string expectedKeyId)
	: publicKeyPem_(std::move(publicKeyPem)),
	  issuer_(std::move(issuer)),
	  audience_(std::move(audience)),
	  expectedKeyId_(std::move(expectedKeyId))
{
}

licensing::TokenVerificationResult Rs256TokenVerifier::verify(std::string_view signedToken) const
{
	const auto first = signedToken.find('.');
	const auto second = first == std::string_view::npos ? std::string_view::npos : signedToken.find('.', first + 1);
	if (first == std::string_view::npos || second == std::string_view::npos ||
	    signedToken.find('.', second + 1) != std::string_view::npos)
		return failure(licensing::TokenVerificationError::Malformed, "License token is malformed");

	const auto headerBytes = decodeBase64Url(signedToken.substr(0, first));
	const auto payloadBytes = decodeBase64Url(signedToken.substr(first + 1, second - first - 1));
	const auto signature = decodeBase64Url(signedToken.substr(second + 1));
	QJsonParseError parseError;
	const auto headerDocument = QJsonDocument::fromJson(headerBytes, &parseError);
	if (headerBytes.isEmpty() || payloadBytes.isEmpty() || signature.isEmpty() ||
	    parseError.error != QJsonParseError::NoError || !headerDocument.isObject())
		return failure(licensing::TokenVerificationError::Malformed, "License token encoding is invalid");
	const auto header = headerDocument.object();
	if (header.value(QStringLiteral("alg")).toString() != QStringLiteral("RS256"))
		return failure(licensing::TokenVerificationError::UnsupportedAlgorithm,
			       "License token algorithm is not RS256");
	if (!expectedKeyId_.empty() && header.value(QStringLiteral("kid")).toString().toStdString() != expectedKeyId_)
		return failure(licensing::TokenVerificationError::UnknownKey, "License token signing key is unknown");

	BioPointer bio(BIO_new_mem_buf(publicKeyPem_.data(), static_cast<int>(publicKeyPem_.size())), &BIO_free);
	KeyPointer key(bio ? PEM_read_bio_PUBKEY(bio.get(), nullptr, nullptr, nullptr) : nullptr, &EVP_PKEY_free);
	DigestPointer digest(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	const auto signingInput = signedToken.substr(0, second);
	if (!key || !digest || EVP_DigestVerifyInit(digest.get(), nullptr, EVP_sha256(), nullptr, key.get()) != 1 ||
	    EVP_DigestVerifyUpdate(digest.get(), signingInput.data(), signingInput.size()) != 1 ||
	    EVP_DigestVerifyFinal(digest.get(), reinterpret_cast<const unsigned char *>(signature.constData()),
				  static_cast<std::size_t>(signature.size())) != 1)
		return failure(licensing::TokenVerificationError::InvalidSignature,
			       "License token signature is invalid");

	const auto payloadDocument = QJsonDocument::fromJson(payloadBytes, &parseError);
	if (parseError.error != QJsonParseError::NoError || !payloadDocument.isObject())
		return failure(licensing::TokenVerificationError::Malformed, "License token payload is invalid");
	const auto payload = payloadDocument.object();
	const auto issuedAt = parseEpoch(payload, QStringLiteral("iat"), true);
	const auto expiresAt = parseEpoch(payload, QStringLiteral("exp"), true);
	if (!issuedAt || !expiresAt || payload.value(QStringLiteral("iss")).toString().toStdString() != issuer_ ||
	    payload.value(QStringLiteral("aud")).toString().toStdString() != audience_)
		return failure(licensing::TokenVerificationError::InvalidClaims, "License token claims are invalid");

	licensing::SignedLicenseClaims claims;
	claims.issuer = issuer_;
	claims.audience = audience_;
	claims.activationId = payload.value(QStringLiteral("sub")).toString().toStdString();
	claims.licenseId = payload.value(QStringLiteral("license_id")).toString().toStdString();
	claims.tokenId = payload.value(QStringLiteral("jti")).toString().toStdString();
	claims.tokenVersion = static_cast<std::uint32_t>(payload.value(QStringLiteral("token_version")).toInt());
	claims.plan = payload.value(QStringLiteral("plan")).toString().toStdString();
	claims.subscriptionStatus = payload.value(QStringLiteral("subscription_status")).toString().toStdString();
	claims.perpetual = payload.value(QStringLiteral("license_kind")).toString() == QStringLiteral("founder");
	claims.machineFingerprintHash =
		payload.value(QStringLiteral("machine_fingerprint_hash")).toString().toStdString();
	claims.installId = payload.value(QStringLiteral("install_id")).toString().toStdString();
	claims.issuedAt = *issuedAt;
	claims.expiresAt = *expiresAt;
	claims.graceEndsAt = parseEpoch(payload, QStringLiteral("grace_until"), false);
	for (const auto &entitlement : payload.value(QStringLiteral("entitlements")).toArray()) {
		if (entitlement.isString())
			claims.entitlements.push_back(entitlement.toString().toStdString());
	}
	if (claims.activationId.empty() || claims.licenseId.empty() || claims.tokenId.empty() ||
	    claims.tokenVersion == 0 ||
	    payload.value(QStringLiteral("device_activation_id")).toString().toStdString() != claims.activationId ||
	    claims.subscriptionStatus.empty() || claims.machineFingerprintHash.size() != 64 ||
	    claims.installId.empty() || claims.expiresAt <= claims.issuedAt)
		return failure(licensing::TokenVerificationError::InvalidClaims,
			       "License token required claims are missing");
	return {std::move(claims), licensing::TokenVerificationError::None, {}};
}

bool Rs256TokenVerifier::configured() const noexcept
{
	return !publicKeyPem_.empty() && !issuer_.empty() && !audience_.empty();
}

} // namespace clipcoach::security
