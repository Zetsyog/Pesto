/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/CLI.hpp>

int main(int argc, char **argv) {
	pesto::CLI cli;

	int exitCode = cli.start(argc, argv);

	return exitCode;
}
