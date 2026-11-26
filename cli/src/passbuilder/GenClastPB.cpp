/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/pass/clast/GenClast.hpp>
#include <pesto/passbuilder/GenClastPB.hpp>
#include <pesto/passbuilder/ParamConstraintJson.hpp>

using namespace pesto::builder;

GenClastPB::GenClastPB() {
	logger = log::get_logger("pb/gen-clast");
}

GenClastPB::~GenClastPB() {}

std::unique_ptr<pesto::pass::IPass> GenClastPB::build(PassManagerBuilder &builder, const json &j) {
	(void)builder;

	auto pass = std::make_unique<pass::GenClastPass>();
	this->configure(*pass, j);
	return pass;
}

void GenClastPB::configure(pass::GenClastPass &passRef, const json &j) {
	auto *pass = &passRef;

	// TODO: cleaner parsing
	for (auto &[key, val] : j.items()) {
		// ignore uuid
		if (key == "uuid") {
			continue;
		}
		if (key == "optLastLevel") {
			if (val.is_number_integer()) {
				pass->cloogL = val.get<int>();
				continue;
			}
			if (val.is_array()) {
				val.get_to(pass->stmtLastOptLevel);
				continue;
			}
			if (val.is_object()) {
				for (auto &[stmt, level] : val.items()) {
					if (!level.is_number_integer()) {
						logger->error("optLastLevel values must be integers.");
						throw std::invalid_argument("optLastLevel values must be integers");
					}
					int parsedStmtId;
					try {
						parsedStmtId = std::stoi(stmt);
					} catch (const std::exception &e) {
						logger->error("optLastLevel keys must be statement ids (integers).");
						throw std::invalid_argument("optLastLevel keys must be statement ids (integers)");
					}
					if (parsedStmtId <= 0) {
						logger->error("optLastLevel statement ids must be positive.");
						throw std::invalid_argument("optLastLevel statement ids must be positive");
					}
					unsigned long stmtId = (unsigned long)parsedStmtId;
					if (stmtId >= pass->stmtLastOptLevel.size()) {
						pass->stmtLastOptLevel.resize(stmtId, -1);
					}
					pass->stmtLastOptLevel[stmtId - 1] = level.get<int>();
				}
				continue;
			}
			logger->error("{} must be an integer, an object or an array.", key.c_str());
			throw std::invalid_argument(key + "  must be an integer, an object or an array");
		}
		if (key == "optFirstLevel") {
			if (val.is_number_integer()) {
				pass->cloogF = val.get<int>();
				continue;
			}
			if (val.is_array()) {
				val.get_to(pass->stmtFirstOptLevel);
				continue;
			}
			if (val.is_object()) {
				for (auto &[stmt, level] : val.items()) {
					if (!level.is_number_integer()) {
						logger->error("optFirstLevel values must be integers.");
						throw std::invalid_argument("optFirstLevel values must be integers");
					}
					int parsedStmtId;
					try {
						parsedStmtId = std::stoi(stmt);
					} catch (const std::exception &e) {
						logger->error("optFirstLevel keys must be statement ids (integers).");
						throw std::invalid_argument("optFirstLevel keys must be statement ids (integers)");
					}
					if (parsedStmtId <= 0) {
						logger->error("optFirstLevel statement ids must be positive.");
						throw std::invalid_argument("optFirstLevel statement ids must be positive");
					}
					unsigned long stmtId = (unsigned long)parsedStmtId;
					if (stmtId >= pass->stmtFirstOptLevel.size()) {
						pass->stmtFirstOptLevel.resize(stmtId, -1);
					}
					pass->stmtFirstOptLevel[stmtId - 1] = level.get<int>();
				}
				continue;
			}
			logger->error("{} must be an integer, an object or an array.", key.c_str());
			throw std::invalid_argument(key + "  must be an integer, an object or an array");
		}
		if (key == "stopLevel") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->cloogStop = val.get<int>();
			continue;
		}
		if (key == "simpleHulls") {
			if (!val.is_boolean()) {
				logger->error("{} must be a boolean.", key.c_str());
				throw std::invalid_argument(key + " must be a boolean");
			}
			pass->cloogSh = val.get<bool>() ? 1 : 0;
			continue;
		}
		if (key == "firstUnrollDimension") {
			if (!val.is_number_integer()) {
				logger->error("{} must be an integer.", key.c_str());
				throw std::invalid_argument(key + " must be an integer");
			}
			pass->cloogFirstUnroll = val.get<int>();
			continue;
		}
		if (key == "backtrack") {
			if (!val.is_boolean()) {
				logger->error("{} must be a boolean.", key.c_str());
				throw std::invalid_argument(key + " must be a boolean");
			}
			pass->cloogBacktrack = val.get<bool>() ? 1 : 0;
			continue;
		}
		if (key == "codegenContext") {
			pass->codegenContext = parseParamConstraint(val, key, *logger);
			continue;
		}
		if (key == "spreadEqualities") {
			if (!val.is_boolean()) {
				logger->error("{} must be a boolean.", key.c_str());
				throw std::invalid_argument(key + " must be a boolean");
			}
			pass->cloogEsp = val.get<bool>() ? 1 : 0;
			continue;
		}

		logger->error("Invalid option: {}", key.c_str());
		throw std::invalid_argument("Invalid option: " + key);
	}
}
