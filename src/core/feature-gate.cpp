#include <clipcoach/licensing/feature-gate.hpp>

namespace clipcoach::licensing {

bool isFeatureAllowed(Feature feature, const LicenseSnapshot &license) noexcept
{
	return PlanPolicy::allows(license.proEnabled() ? Plan::Pro : Plan::Free, feature);
}

} // namespace clipcoach::licensing
