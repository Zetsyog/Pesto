#ifndef PESTO_OPTIONS_H
#define PESTO_OPTIONS_H

#include <memory>
#include <string>

class PestoOptions {
  public:
	static std::unique_ptr<PestoOptions> build(int argc, char **argv);

	PestoOptions();

	std::string getGenPrefix();
	std::string getInputPath();
	int getReadScop();

  private:
	std::string genPrefix;
	std::string inputPath;
	int readScop;
};

#endif // PESTO_OPTIONS_H