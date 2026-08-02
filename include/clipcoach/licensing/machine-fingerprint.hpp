#pragma once

#include <clipcoach/licensing/license-types.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace clipcoach::licensing {

class MachineFingerprintProvider {
public:
	virtual ~MachineFingerprintProvider() = default;
	[[nodiscard]] virtual std::optional<DeviceIdentity> identity(std::string *error = nullptr) = 0;
};

[[nodiscard]] std::string generateInstallId();
[[nodiscard]] bool isValidInstallId(std::string_view value) noexcept;
[[nodiscard]] std::string hashMachineFingerprint(std::string_view installId, std::string_view stableSystemIdentifier);
[[nodiscard]] std::string platformStableSystemIdentifier();

class FileMachineFingerprintProvider final : public MachineFingerprintProvider {
public:
	explicit FileMachineFingerprintProvider(std::filesystem::path installIdPath);

	[[nodiscard]] std::optional<DeviceIdentity> identity(std::string *error = nullptr) override;

private:
	std::filesystem::path installIdPath_;
};

} // namespace clipcoach::licensing
