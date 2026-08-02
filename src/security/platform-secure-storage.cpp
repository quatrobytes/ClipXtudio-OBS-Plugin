#include <clipcoach/security/secure-storage.hpp>

#ifdef _WIN32
#include <windows.h>
#include <wincred.h>
#endif

#include <utility>

namespace clipcoach::security {
namespace {

class UnavailableSecureStorage final : public SecureStorage {
public:
	bool store(std::string_view, const SecretBytes &,
		   std::string *error) override
	{
		if (error)
			*error = "Platform secure storage is unavailable";
		return false;
	}

	SecretReadResult load(std::string_view) override
	{
		return {{}, SecureStorageError::Unavailable,
			"Platform secure storage is unavailable"};
	}

	bool remove(std::string_view, std::string *error) override
	{
		if (error)
			*error = "Platform secure storage is unavailable";
		return false;
	}
};

#ifdef _WIN32
std::optional<std::wstring> utf8ToWide(std::string_view value)
{
	if (value.empty())
		return std::wstring{};
	const auto length = MultiByteToWideChar(
		CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
		static_cast<int>(value.size()), nullptr, 0);
	if (length <= 0)
		return std::nullopt;
	std::wstring result(static_cast<std::size_t>(length), L'\0');
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
				static_cast<int>(value.size()), result.data(),
				length) != length)
		return std::nullopt;
	return result;
}

class WindowsCredentialSecureStorage final : public SecureStorage {
public:
	explicit WindowsCredentialSecureStorage(std::string serviceName)
		: serviceName_(std::move(serviceName))
	{
	}

	bool store(std::string_view key, const SecretBytes &secret,
		   std::string *error) override
	{
		if (secret.empty() ||
		    secret.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
			if (error)
				*error =
					"Secret exceeds Windows Credential Manager limits";
			return false;
		}
		const auto target = targetName(key);
		if (!target) {
			if (error)
				*error = "Secure storage key is invalid UTF-8";
			return false;
		}
		CREDENTIALW credential{};
		credential.Type = CRED_TYPE_GENERIC;
		credential.TargetName =
			const_cast<wchar_t *>(target->c_str());
		credential.CredentialBlobSize =
			static_cast<DWORD>(secret.size());
		credential.CredentialBlob =
			const_cast<LPBYTE>(secret.data());
		credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
		if (!CredWriteW(&credential, 0)) {
			if (error)
				*error =
					"Windows Credential Manager write failed";
			return false;
		}
		return true;
	}

	SecretReadResult load(std::string_view key) override
	{
		const auto target = targetName(key);
		if (!target)
			return {{}, SecureStorageError::InvalidData,
				"Secure storage key is invalid UTF-8"};
		PCREDENTIALW credential = nullptr;
		if (!CredReadW(target->c_str(), CRED_TYPE_GENERIC, 0,
			       &credential)) {
			return {{},
				GetLastError() == ERROR_NOT_FOUND
					? SecureStorageError::NotFound
					: SecureStorageError::PlatformFailure,
				"Windows Credential Manager read failed"};
		}
		SecretBytes result(
			credential->CredentialBlob,
			credential->CredentialBlob +
				credential->CredentialBlobSize);
		if (credential->CredentialBlob != nullptr &&
		    credential->CredentialBlobSize > 0) {
			SecureZeroMemory(credential->CredentialBlob,
					 credential->CredentialBlobSize);
		}
		CredFree(credential);
		return {std::move(result), SecureStorageError::None, {}};
	}

	bool remove(std::string_view key, std::string *error) override
	{
		const auto target = targetName(key);
		if (!target) {
			if (error)
				*error = "Secure storage key is invalid UTF-8";
			return false;
		}
		if (!CredDeleteW(target->c_str(), CRED_TYPE_GENERIC, 0) &&
		    GetLastError() != ERROR_NOT_FOUND) {
			if (error)
				*error =
					"Windows Credential Manager delete failed";
			return false;
		}
		return true;
	}

private:
	std::optional<std::wstring>
	targetName(std::string_view key) const
	{
		return utf8ToWide(serviceName_ + "/" + std::string(key));
	}

	std::string serviceName_;
};
#endif

} // namespace

std::unique_ptr<SecureStorage>
createPlatformSecureStorage(std::string serviceName)
{
#ifdef _WIN32
	return std::make_unique<WindowsCredentialSecureStorage>(
		std::move(serviceName));
#else
	(void)serviceName;
	return std::make_unique<UnavailableSecureStorage>();
#endif
}

} // namespace clipcoach::security
