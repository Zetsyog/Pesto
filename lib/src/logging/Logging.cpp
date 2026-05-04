/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <pesto/Logging.hpp>

namespace pesto::log {
static std::vector<spdlog::sink_ptr> g_sinks;

// Helper to initialise the sinks exactly once.
static void init_sinks() {
	// Console sink – coloured, thread‑safe
	auto console = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
	console->set_pattern("[%T] [%^%l%$] [%n] %v");

	g_sinks = {console};
}

const std::vector<spdlog::sink_ptr> &sinks() {
	// Lazy initialisation – safe in C++11+ (static local init is thread‑safe)
	static const bool initialized = [] {
		init_sinks();
		return true;
	}();
	(void)initialized; // silence unused‑variable warning
	return g_sinks;
}

std::shared_ptr<spdlog::logger> get_logger(const std::string &name) {
	auto existing_logger = spdlog::get(name);
	if (existing_logger != nullptr) {
		existing_logger->set_level(spdlog::get_level());

		return existing_logger;
	}

	auto logger = std::make_shared<spdlog::logger>(name, sinks().begin(), sinks().end());

	// Default configuration – can be overridden per‑component later
	logger->flush_on(spdlog::level::err);
	logger->set_level(spdlog::get_level());

	spdlog::register_logger(logger);
	return logger;
}

void set_level(spdlog::level::level_enum lvl) {
	for (auto &sink : g_sinks) {
		sink->set_level(lvl);
	}
}

} // namespace pesto::log