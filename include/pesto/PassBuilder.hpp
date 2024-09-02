#ifndef PESTO_PASS_BUILDER_H
#define PESTO_PASS_BUILDER_H

#include <memory>

#include <nlohmann/json.hpp>

#include <pesto/PassManager.hpp>

using json = nlohmann::json;

namespace pesto {

class IPassBuilder {
  public:
	IPassBuilder();
	virtual ~IPassBuilder();

	virtual std::string getUUID() = 0;
	virtual std::unique_ptr<pesto::IPolyhedralPass> build(const json &j) = 0;
};

class PMBuilder {
  public:
	PMBuilder();
	virtual ~PMBuilder();

	void registerPassBuilder(std::unique_ptr<IPassBuilder> pb);
	std::unique_ptr<PassManager> fromJSON(std::string file);

	static std::unique_ptr<PMBuilder> createDefault();

  private:
	std::unordered_map<std::string, std::unique_ptr<IPassBuilder>> builders;
};

}; // namespace pesto

#endif // PESTO_PASS_BUILDER_H