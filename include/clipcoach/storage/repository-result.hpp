#pragma once

#include <string>
#include <utility>

namespace clipcoach::storage {

struct StorageStatus {
	bool success{false};
	std::string error;

	[[nodiscard]] static StorageStatus ok() { return {true, {}}; }
	[[nodiscard]] static StorageStatus fail(std::string message)
	{
		return {false, std::move(message)};
	}
};

template<typename T> struct RepositoryResult {
	bool success{false};
	T value{};
	std::string error;

	[[nodiscard]] static RepositoryResult ok(T result)
	{
		return {true, std::move(result), {}};
	}
	[[nodiscard]] static RepositoryResult fail(std::string message)
	{
		return {false, {}, std::move(message)};
	}
};

} // namespace clipcoach::storage
