#ifndef PESTO_PASS_BUILDER_H
#define PESTO_PASS_BUILDER_H

#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include <pesto/PassManager.hpp>

using json = nlohmann::json;

namespace pesto {

class PMBuilder;

/**
 * Interface for a pass builder
 * This object is responsible for building a pass from a json object
 * Each pass builder must have a unique identifier that will be used to identify
 * it in the json configuration file
 */
class IPassBuilder {
  public:
	IPassBuilder();
	virtual ~IPassBuilder();

	/**
	 * @brief unique identifier for this pass
	 * @return the string uuid
	 */
	virtual std::string getUUID() = 0;

	/**
	 * @brief Creates a new pass from json context
	 * @param builder the pass manager builder
	 * @param j the json object containing the pass configuration
	 * @return A pointer to the newly created pass
	 */
	virtual std::unique_ptr<pesto::IPestoPass> build(PMBuilder &builder,
													 const json &j) = 0;
};

/**
 * A pass manager builder
 * This object is responsible for building a pass manager from a json object
 * Pass builders must be registered so that the pass manager builder can
 * delegate the building of each pass to the corresponding pass builder
 */
class PMBuilder {
  public:
	PMBuilder();
	virtual ~PMBuilder();

	/**
	 * @brief return a pass builder corresponding to the given id
	 * Pass builders must be registered before calling this function
	 * using the registerPassBuilder function
	 *
	 * @param uuid the unique identifier of the pass builder
	 * @return a pointer to the pass builder or an empty optional if no pass
	 * builder is found
	 */
	std::optional<IPassBuilder> getPassBuilder(std::string uuid);

	std::unique_ptr<IPestoPass> buildPass(std::string uuid, const json &j);

	/**
	 * @brief register a pass builder
	 * @param pb a pointer to the pass builder
	 */
	void registerPassBuilder(std::unique_ptr<IPassBuilder> pb);

	/**
	 * @brief Creates a new pass manager from json context
	 * @return A pointer to the newly created pass manager
	 */
	std::unique_ptr<PassManager> build(std::string file);

	static std::unique_ptr<PMBuilder> createDefault();

  private:
	std::unordered_map<std::string, std::unique_ptr<IPassBuilder>> builders;
};

}; // namespace pesto

#endif // PESTO_PASS_BUILDER_H