#pragma once

#include <clipcoach/licensing/token-verifier.hpp>

#include <string>

namespace clipcoach::security {

class Rs256TokenVerifier final : public licensing::TokenVerifier {
public:
	Rs256TokenVerifier(std::string publicKeyPem, std::string issuer, std::string audience,
			   std::string expectedKeyId = {});

	[[nodiscard]] licensing::TokenVerificationResult verify(std::string_view signedToken) const override;
	[[nodiscard]] bool configured() const noexcept;

private:
	std::string publicKeyPem_;
	std::string issuer_;
	std::string audience_;
	std::string expectedKeyId_;
};

} // namespace clipcoach::security
