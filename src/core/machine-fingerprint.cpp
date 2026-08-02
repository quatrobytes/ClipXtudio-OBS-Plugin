#include <clipcoach/licensing/machine-fingerprint.hpp>

#include <openssl/evp.h>
#include <openssl/rand.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace clipcoach::licensing {
namespace {

std::string trim(std::string value)
{
	while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
		value.pop_back();
	std::size_t first = 0;
	while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first])))
		++first;
	return value.substr(first);
}

std::string bytesToHex(const unsigned char *bytes, std::size_t size)
{
	std::ostringstream output;
	output << std::hex << std::setfill('0');
	for (std::size_t index = 0; index < size; ++index)
		output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
	return output.str();
}

#ifdef _WIN32
std::string wideToUtf8(const std::wstring &value)
{
	if (value.empty())
		return {};
	const auto required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
						  static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
	if (required <= 0)
		return {};
	std::string result(static_cast<std::size_t>(required), '\0');
	if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
				result.data(), required, nullptr, nullptr) != required)
		return {};
	return result;
}
#endif

} // namespace

std::string generateInstallId()
{
	std::array<unsigned char, 16> bytes{};
	if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
		return {};
	bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0fU) | 0x40U);
	bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3fU) | 0x80U);
	const auto hex = bytesToHex(bytes.data(), bytes.size());
	return hex.substr(0, 8) + "-" + hex.substr(8, 4) + "-" + hex.substr(12, 4) + "-" + hex.substr(16, 4) + "-" +
	       hex.substr(20, 12);
}

bool isValidInstallId(std::string_view value) noexcept
{
	if (value.size() != 36)
		return false;
	for (std::size_t index = 0; index < value.size(); ++index) {
		if (index == 8 || index == 13 || index == 18 || index == 23) {
			if (value[index] != '-')
				return false;
		} else if (!std::isxdigit(static_cast<unsigned char>(value[index]))) {
			return false;
		}
	}
	return value[14] == '4' && (value[19] == '8' || value[19] == '9' || value[19] == 'a' || value[19] == 'b' ||
				    value[19] == 'A' || value[19] == 'B');
}

std::string hashMachineFingerprint(std::string_view installId, std::string_view stableSystemIdentifier)
{
	const std::string material = std::string("clipcoach-fingerprint-v1\0", 25) + std::string(installId) + '\0' +
				     std::string(stableSystemIdentifier);
	std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
	unsigned int digestSize = 0;
	auto *context = EVP_MD_CTX_new();
	if (context == nullptr)
		return {};
	const bool succeeded = EVP_DigestInit_ex(context, EVP_sha256(), nullptr) == 1 &&
			       EVP_DigestUpdate(context, material.data(), material.size()) == 1 &&
			       EVP_DigestFinal_ex(context, digest.data(), &digestSize) == 1;
	EVP_MD_CTX_free(context);
	return succeeded ? bytesToHex(digest.data(), digestSize) : std::string{};
}

std::string platformStableSystemIdentifier()
{
#ifdef _WIN32
	std::array<wchar_t, 256> value{};
	DWORD valueSize = static_cast<DWORD>(value.size() * sizeof(wchar_t));
	const auto status = RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", L"MachineGuid",
					 RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, value.data(), &valueSize);
	if (status != ERROR_SUCCESS || valueSize < sizeof(wchar_t))
		return {};
	const auto characterCount = static_cast<std::size_t>(valueSize / sizeof(wchar_t));
	return trim(wideToUtf8(std::wstring(value.data(), characterCount > 0 ? characterCount - 1 : 0)));
#elif defined(__linux__)
	std::ifstream input("/etc/machine-id", std::ios::binary);
	std::string value;
	std::getline(input, value);
	return trim(std::move(value));
#else
	return {};
#endif
}

FileMachineFingerprintProvider::FileMachineFingerprintProvider(std::filesystem::path installIdPath)
	: installIdPath_(std::move(installIdPath))
{
}

std::optional<DeviceIdentity> FileMachineFingerprintProvider::identity(std::string *error)
{
	std::string installId;
	{
		std::ifstream input(installIdPath_, std::ios::binary);
		if (input)
			std::getline(input, installId);
		installId = trim(std::move(installId));
	}

	if (!installId.empty() && !isValidInstallId(installId)) {
		if (error)
			*error = "Stored install_id is invalid";
		return std::nullopt;
	}
	if (installId.empty()) {
		installId = generateInstallId();
		if (installId.empty()) {
			if (error)
				*error = "Cryptographic install_id generation failed";
			return std::nullopt;
		}
		std::error_code filesystemError;
		const auto parent = installIdPath_.parent_path();
		if (!parent.empty())
			std::filesystem::create_directories(parent, filesystemError);
		if (filesystemError) {
			if (error)
				*error = "Could not create install_id directory";
			return std::nullopt;
		}
		const auto temporary = installIdPath_.string() + ".tmp-" + generateInstallId();
		{
			std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
			if (!output || !(output << installId << '\n')) {
				if (error)
					*error = "Could not persist install_id";
				std::error_code removeError;
				std::filesystem::remove(temporary, removeError);
				return std::nullopt;
			}
		}
		std::filesystem::rename(temporary, installIdPath_, filesystemError);
		if (filesystemError) {
			std::filesystem::remove(temporary, filesystemError);
			std::ifstream concurrent(installIdPath_, std::ios::binary);
			std::getline(concurrent, installId);
			installId = trim(std::move(installId));
			if (!isValidInstallId(installId)) {
				if (error)
					*error = "Could not commit install_id";
				return std::nullopt;
			}
		}
#ifndef _WIN32
		std::filesystem::permissions(installIdPath_,
					     std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
					     std::filesystem::perm_options::replace, filesystemError);
#endif
	}

	auto stableIdentifier = platformStableSystemIdentifier();
	const auto fingerprint = hashMachineFingerprint(installId, stableIdentifier);
	std::fill(stableIdentifier.begin(), stableIdentifier.end(), '\0');
	if (fingerprint.size() != 64) {
		if (error)
			*error = "Machine fingerprint hashing failed";
		return std::nullopt;
	}
	return DeviceIdentity{std::move(installId), fingerprint};
}

} // namespace clipcoach::licensing
