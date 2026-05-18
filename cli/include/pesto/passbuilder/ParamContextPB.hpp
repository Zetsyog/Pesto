/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/PassBuilder.hpp>

#include <memory>
#include <string>

namespace pesto {

namespace builder {

class ParamContextPB : public IPassBuilder {
  public:
	static const std::string &getID() {
		static const std::string uuid = "param-context";
		return uuid;
	};

	ParamContextPB();
	virtual ~ParamContextPB();

	virtual std::unique_ptr<pesto::pass::IPass> build(PassManagerBuilder &builder, const json &j);
};

} // namespace builder

} // namespace pesto
