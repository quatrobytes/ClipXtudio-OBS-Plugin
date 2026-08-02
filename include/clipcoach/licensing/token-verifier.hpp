#pragma once

#include <clipcoach/licensing/license-types.hpp>

#include <string_view>

namespace clipcoach::licensing {

class TokenVerifier {
public:
	virtual ~TokenVerifier() = default;
	[[nodiscard]] virtual TokenVerificationResult verify(std::string_view signedToken) const = 0;
};

} // namespace clipcoach::licensing
