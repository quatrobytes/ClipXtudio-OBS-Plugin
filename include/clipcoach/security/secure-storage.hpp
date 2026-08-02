#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace clipcoach::security {

using SecretBytes = std::vector<std::uint8_t>;

enum class SecureStorageError {
	None,
	Unavailable,
	AccessDenied,
	NotFound,
	PlatformFailure,
	InvalidData,
};

struct SecretReadResult {
	std::optional<SecretBytes> value;
	SecureStorageError error{SecureStorageError::None};
	std::string message;

	[[nodiscard]] bool succeeded() const noexcept
	{
		return error == SecureStorageError::None;
	}
};

class SecureStorage {
public:
	virtual ~SecureStorage() = default;
	[[nodiscard]] virtual bool
	store(std::string_view key, const SecretBytes &secret,
	      std::string *error = nullptr) = 0;
	[[nodiscard]] virtual SecretReadResult
	load(std::string_view key) = 0;
	[[nodiscard]] virtual bool
	remove(std::string_view key, std::string *error = nullptr) = 0;
};

[[nodiscard]] std::unique_ptr<SecureStorage>
createPlatformSecureStorage(std::string serviceName);

} // namespace clipcoach::security
