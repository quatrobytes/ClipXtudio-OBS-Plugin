#include <clipcoach/licensing/machine-fingerprint.hpp>
#include "test-support.hpp"

#include <filesystem>
#include <fstream>

int main()
{
	using namespace clipcoach::licensing;

	const auto first = generateInstallId();
	const auto second = generateInstallId();
	clipcoach::test::expect(isValidInstallId(first), "generated install_id must be a UUID");
	clipcoach::test::expect(first != second, "install_id generation must not repeat");
	clipcoach::test::expect(hashMachineFingerprint("971a1d9e-d0f0-4e28-9483-1fc3b84c0c64", "stable-system-id") ==
					"8555edb0fa278bdf4b6fce49767b9df304a0a0dc08ca24805b403f00656bd359",
				"fingerprint hashing must be deterministic SHA-256");

	const auto path = std::filesystem::temp_directory_path() / ("clipcoach-install-" + first + ".txt");
	std::error_code removeError;
	std::filesystem::remove(path, removeError);
	FileMachineFingerprintProvider provider(path);
	std::string error;
	const auto identity = provider.identity(&error);
	clipcoach::test::expect(identity.has_value(), "provider must create install identity: " + error);
	const auto reloaded = provider.identity(&error);
	clipcoach::test::expect(reloaded.has_value() && reloaded->installId == identity->installId,
				"install_id must persist across loads");
	clipcoach::test::expect(reloaded->fingerprintHash.size() == 64, "fingerprint must be sent only as SHA-256");
	std::filesystem::remove(path, removeError);

	return 0;
}
