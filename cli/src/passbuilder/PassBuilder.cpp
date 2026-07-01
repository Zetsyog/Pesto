/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/PassBuilder.hpp>

using namespace pesto::builder;

IPassBuilder::IPassBuilder() {
	logger = log::get_logger("pass-builder");
}
IPassBuilder::~IPassBuilder() {}