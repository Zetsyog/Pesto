/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_LOGGING_H
#define PESTO_LOGGING_H

#include <memory>
#include <vector>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace pesto::log {

// Public accessor – returns a reference to the *global* sink container.
const std::vector<spdlog::sink_ptr> &sinks();

// Factory: give a name, get a logger that shares the global sinks.
std::shared_ptr<spdlog::logger> get_logger(const std::string &name);

// Optional helpers to change global configuration at runtime.
void set_level(spdlog::level::level_enum lvl);
void set_pattern(const std::string &pattern);
} // namespace pesto::log

#endif // PESTO_LOGGING_H
