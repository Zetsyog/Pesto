#ifndef PESTO_OPTIONS_H
#define PESTO_OPTIONS_H

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace pesto {

class Options {
  public:
	static std::unique_ptr<Options> build(int argc, char **argv);

	Options();

	std::filesystem::path getInputPath();
	std::filesystem::path getOutputPath();

	std::optional<std::string> getConfigPath();
	int getReadScop();

  private:
	std::filesystem::path input;
	std::filesystem::path output;
	std::optional<std::string> configPath;
	int readScop;
};

} // namespace pesto

#endif // PESTO_OPTIONS_H