/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_PASS_BUILDER_H
#define PESTO_PASS_BUILDER_H

#include <memory>
#include <optional>

#include <nlohmann/json.hpp>

#include <pesto/PassManager.hpp>

using json = nlohmann::json;

namespace pesto {

namespace builder {

class PassManagerBuilder;
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
	 * @brief Creates a new pass from json context
	 * @param builder the pass manager builder
	 * @param j the json object containing the pass configuration
	 * @return A pointer to the newly created pass
	 */
	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j) = 0;
};
/**
 * A pass manager builder
 * This object is responsible for building a pass manager from a json object
 * Pass builders must be registered so that the pass manager builder can
 * delegate the building of each pass to the corresponding pass builder
 */
class PassManagerBuilder {
  public:
	PassManagerBuilder();
	virtual ~PassManagerBuilder();

	/**
	 * @brief check if a pass builder is registered with the given uuid
	 * @param uuid the uuid of the pass builder
	 * @return true if the pass builder is registered, false otherwise
	 */
	bool hasPassBuilder(std::string uuid);

	/**
	 * @brief Creates a new pass from json context
	 * The json object MUST have at least a "uuid" field that will be used to
	 * identify the pass builder
	 * @param j the json object containing the pass configuration
	 *
	 * @return A pointer to the newly created pass
	 * @throw std::runtime_error if the pass uuid does not match any registered
	 * pass builder
	 */
	std::unique_ptr<pesto::pass::IPass> buildPass(const json &j);

	/**
	 * @brief register a pass builder
	 * @param pb a pointer to the pass builder
	 */
	void registerPassBuilder(std::string uuid, std::unique_ptr<IPassBuilder> pb);

	/**
	 * @brief Creates a new pass manager from json context
	 * @return A pointer to the newly created pass manager
	 */
	std::unique_ptr<PassManager> build(std::string file);

	static std::unique_ptr<PassManagerBuilder> createDefault();

	/**
	 * @brief Get the pass builder corresponding to the given uuid
	 * @param uuid the uuid of the pass builder
	 * @return A pointer to the pass builder
	 */
	std::unique_ptr<IPassBuilder> &getPassBuilder(std::string uuid);

  private:
	std::unordered_map<std::string, std::unique_ptr<IPassBuilder>> builders;
};

} // namespace builder

} // namespace pesto

#endif // PESTO_PASS_BUILDER_H