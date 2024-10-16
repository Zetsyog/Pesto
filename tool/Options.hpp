#ifndef PESTO_OPTIONS_H
#define PESTO_OPTIONS_H

#include <memory>
#include <optional>
#include <string>

namespace pesto {

class Options {
  public:
	static std::unique_ptr<Options> build(int argc, char **argv);

	Options();

	std::string getGenPrefix();
	std::string getInputPath();
	std::optional<std::string> getConfigPath();
	int getReadScop();

  private:
	std::string genPrefix;
	std::string inputPath;
	std::optional<std::string> configPath;
	int readScop;
};

} // namespace pesto

#endif // PESTO_OPTIONS_H