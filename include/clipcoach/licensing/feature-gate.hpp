#pragma once

#include <clipcoach/licensing/license-types.hpp>

namespace clipcoach::licensing {

[[nodiscard]] bool isFeatureAllowed(Feature feature, const LicenseSnapshot &license) noexcept;

} // namespace clipcoach::licensing
