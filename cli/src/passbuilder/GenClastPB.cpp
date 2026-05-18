/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/clast/GenClastPass.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>

using namespace pesto::builder;

GenClastPB::GenClastPB() {
	logger = log::get_logger("pb/gen-clast");
}

GenClastPB::~GenClastPB() {}

std::unique_ptr<pesto::pass::IPass> GenClastPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;
	(void)j;

	auto pass = std::make_unique<pass::GenClastPass>();

	// TODO: cleaner parsing
	for (auto &[key, val] : j.items()) {
		// ignore uuid
		if (key == "uuid") {
		} else if (key == "optLastLevel") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->cloogL = val.get<int>();
		} else if (key == "optFirstLevel") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->cloogF = val.get<int>();
		} else if (key == "stopLevel") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->cloogStop = val.get<int>();
		} else if (key == "simpleHulls") {
			if (!val.is_boolean()) {
				logger->error("{} must be a boolean.", key.c_str());
				throw std::invalid_argument(key + " must be a boolean");
			}
			pass->cloogSh = val.get<bool>() ? 1 : 0;
		} else if (key == "firstUnrollDimension") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->cloogFirstUnroll = val.get<int>();
		} else if (key == "backtrack") {
			if (!val.is_boolean()) {
				logger->error("{} must be a boolean.", key.c_str());
				throw std::invalid_argument(key + " must be a boolean");
			}
			pass->cloogBacktrack = val.get<bool>() ? 1 : 0;
		} else if (key == "codegenContext") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->codegenContext = val.get<int>();
		} else if (key == "spreadEqualities") {
			if (!val.is_boolean()) {
				logger->error("{} must be a boolean.", key.c_str());
				throw std::invalid_argument(key + " must be a boolean");
			}
			pass->cloogEsp = val.get<bool>() ? 1 : 0;
		} else {
			logger->error("Invalid option: {}", key.c_str());
			throw std::invalid_argument("Invalid option: " + key);
		}
	}

	return pass;
}
