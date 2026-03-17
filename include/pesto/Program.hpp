/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_POLY_PROGRAM_H
#define PESTO_POLY_PROGRAM_H

#include <filesystem>
#include <memory>
#include <vector>

#include <osl/osl.h>

#include <pesto/OslScop.hpp>

namespace pesto {

/**
 * A class representing a polyhedral program
 * A polyhedral program is a collection of OslScop objects
 * OslScop objects can be added and / or removed from the program
 * The order of the OslScop objects does matter
 */
class Program {
  public:
	class Builder;

	virtual ~Program();

	void addScopNest(OslScop *nest);
	void clearScopNests();
	std::vector<OslScop *> getScopNests();

	std::filesystem::path getInputFile();
	std::filesystem::path getOutputFile();

  private:
	std::filesystem::path inputFile;
	std::filesystem::path outputFile;
	std::vector<OslScop *> scops;
};

/**
 * This class is used to build a pesto::Program object from an OpenScop
 * representation
 */
class Program::Builder {
  public:
	Builder();

	Builder &addScop(osl_scop_p scop);
	Builder &setInputFile(std::filesystem::path path);
	Builder &setOutputFile(std::filesystem::path path);

	std::unique_ptr<Program> build();

  private:
	std::unique_ptr<Program> prog;
	int nextScopId;
};

} // namespace pesto

#endif // PESTO_POLY_PROGRAM_H