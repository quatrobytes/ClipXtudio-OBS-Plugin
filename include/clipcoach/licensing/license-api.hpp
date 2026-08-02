#pragma once

#include <clipcoach/licensing/license-types.hpp>

#include <functional>

namespace clipcoach::licensing {

class LicenseApi {
public:
	using Completion = std::function<void(LicenseApiResult)>;

	virtual ~LicenseApi() = default;
	virtual void activate(ActivationRequest request, Completion completion) = 0;
	virtual void refresh(RefreshRequest request, Completion completion) = 0;
};

} // namespace clipcoach::licensing
