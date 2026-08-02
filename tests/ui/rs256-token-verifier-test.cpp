#include <clipcoach/security/rs256-token-verifier.hpp>
#include "../unit/test-support.hpp"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <memory>
#include <string>

namespace {

using KeyPointer = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using ContextPointer = std::unique_ptr<EVP_PKEY_CTX, decltype(&EVP_PKEY_CTX_free)>;
using DigestPointer = std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)>;
using BioPointer = std::unique_ptr<BIO, decltype(&BIO_free)>;

QByteArray base64Url(const QByteArray &value)
{
	return value.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

KeyPointer createKey()
{
	ContextPointer context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), &EVP_PKEY_CTX_free);
	EVP_PKEY *raw = nullptr;
	if (!context || EVP_PKEY_keygen_init(context.get()) != 1 ||
	    EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 2048) != 1 || EVP_PKEY_keygen(context.get(), &raw) != 1)
		return {nullptr, &EVP_PKEY_free};
	return {raw, &EVP_PKEY_free};
}

std::string publicPem(EVP_PKEY *key)
{
	BioPointer bio(BIO_new(BIO_s_mem()), &BIO_free);
	if (!bio || PEM_write_bio_PUBKEY(bio.get(), key) != 1)
		return {};
	char *data = nullptr;
	const auto length = BIO_get_mem_data(bio.get(), &data);
	return std::string(data, static_cast<std::size_t>(length));
}

std::string sign(EVP_PKEY *key, const QJsonObject &payload)
{
	const auto header = base64Url(QJsonDocument(QJsonObject{
							    {QStringLiteral("alg"), QStringLiteral("RS256")},
							    {QStringLiteral("typ"), QStringLiteral("JWT")},
							    {QStringLiteral("kid"), QStringLiteral("test-key")},
						    })
					      .toJson(QJsonDocument::Compact));
	const auto body = base64Url(QJsonDocument(payload).toJson(QJsonDocument::Compact));
	const auto input = header + '.' + body;
	DigestPointer context(EVP_MD_CTX_new(), &EVP_MD_CTX_free);
	std::size_t size = 0;
	if (!context || EVP_DigestSignInit(context.get(), nullptr, EVP_sha256(), nullptr, key) != 1 ||
	    EVP_DigestSignUpdate(context.get(), input.data(), static_cast<std::size_t>(input.size())) != 1 ||
	    EVP_DigestSignFinal(context.get(), nullptr, &size) != 1)
		return {};
	QByteArray signature(static_cast<qsizetype>(size), '\0');
	if (EVP_DigestSignFinal(context.get(), reinterpret_cast<unsigned char *>(signature.data()), &size) != 1)
		return {};
	signature.resize(static_cast<qsizetype>(size));
	return (input + '.' + base64Url(signature)).toStdString();
}

} // namespace

int main()
{
	auto key = createKey();
	clipcoach::test::expect(key != nullptr, "test RSA key must be generated");
	clipcoach::security::Rs256TokenVerifier verifier(publicPem(key.get()), "clipcoach-studio",
							 "clipcoach-native-plugin", "test-key");
	const auto token =
		sign(key.get(), {{QStringLiteral("iss"), QStringLiteral("clipcoach-studio")},
				 {QStringLiteral("aud"), QStringLiteral("clipcoach-native-plugin")},
				 {QStringLiteral("sub"), QStringLiteral("activation")},
				 {QStringLiteral("device_activation_id"), QStringLiteral("activation")},
				 {QStringLiteral("license_id"), QStringLiteral("42")},
				 {QStringLiteral("jti"), QStringLiteral("token")},
				 {QStringLiteral("token_version"), 1},
				 {QStringLiteral("plan"), QStringLiteral("pro")},
				 {QStringLiteral("subscription_status"), QStringLiteral("active")},
				 {QStringLiteral("machine_fingerprint_hash"), QString(64, QLatin1Char('a'))},
				 {QStringLiteral("install_id"), QStringLiteral("971a1d9e-d0f0-4e28-9483-1fc3b84c0c64")},
				 {QStringLiteral("iat"), 2'000'000'000},
				 {QStringLiteral("exp"), 2'000'003'600}});
	const auto verified = verifier.verify(token);
	clipcoach::test::expect(verified.succeeded() && verified.claims->activationId == "activation",
				"valid RS256 token must expose trusted claims");
	clipcoach::test::expect(verified.claims->licenseId == "42" && verified.claims->tokenVersion == 1,
				"license id and revocation version must be verified claims");

	const auto legacyToken =
		sign(key.get(), {{QStringLiteral("iss"), QStringLiteral("clipcoach-studio")},
				 {QStringLiteral("aud"), QStringLiteral("clipcoach-native-plugin")},
				 {QStringLiteral("sub"), QStringLiteral("activation")},
				 {QStringLiteral("jti"), QStringLiteral("legacy-token")},
				 {QStringLiteral("plan"), QStringLiteral("pro")},
				 {QStringLiteral("subscription_status"), QStringLiteral("active")},
				 {QStringLiteral("machine_fingerprint_hash"), QString(64, QLatin1Char('a'))},
				 {QStringLiteral("install_id"), QStringLiteral("971a1d9e-d0f0-4e28-9483-1fc3b84c0c64")},
				 {QStringLiteral("iat"), 2'000'000'000},
				 {QStringLiteral("exp"), 2'000'003'600}});
	clipcoach::test::expect(verifier.verify(legacyToken).error ==
					clipcoach::licensing::TokenVerificationError::InvalidClaims,
				"tokens without license binding and token version must fail closed");

	auto tampered = token;
	tampered[tampered.size() / 2] = tampered[tampered.size() / 2] == 'a' ? 'b' : 'a';
	clipcoach::test::expect(verifier.verify(tampered).error ==
					clipcoach::licensing::TokenVerificationError::InvalidSignature,
				"tampered response must fail signature verification");

	return 0;
}
