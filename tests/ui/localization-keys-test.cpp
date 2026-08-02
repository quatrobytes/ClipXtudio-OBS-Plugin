#include "../unit/test-support.hpp"

#include <clipcoach/ui/ui-strings.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::string readFile(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	std::ostringstream contents;
	contents << input.rdbuf();
	return contents.str();
}

} // namespace

int main()
{
	const auto sourceRoot = std::filesystem::path(CLIPCOACH_SOURCE_DIR);
	const auto english = readFile(sourceRoot / "data" / "locale" / "en-US.ini");
	const auto spanish = readFile(sourceRoot / "data" / "locale" / "es-ES.ini");

	for (const auto *key : clipcoach::ui::strings::kRequiredTranslationKeys) {
		const auto declaration = std::string(key) + '=';
		clipcoach::test::expect(english.find(declaration) != std::string::npos,
					std::string("missing English localization key: ") + key);
		clipcoach::test::expect(spanish.find(declaration) != std::string::npos,
					std::string("missing Spanish localization key: ") + key);
	}

	return clipcoach::test::pass("localization-keys-test");
}
