/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <string>
#include <vector>

namespace pesto::utils {

namespace process {

struct Result {
	int exit_code = -1;
	bool signaled = false;
	int signal_num = 0;
};

struct RedirectTo {
	enum class Kind { Inherit, File, Null };

	Kind kind = Kind::Inherit;
	std::string path;
	bool append = false;

	static RedirectTo inherit();
	static RedirectTo file(std::string path, bool append = false);
	static RedirectTo null();
};

} // namespace process

class Process {
  public:
	Process &command(std::vector<std::string> args);
	Process &command(const std::string &cmd);
	Process &stdout_to(process::RedirectTo r);
	Process &stderr_to(process::RedirectTo r);
	Process &merge_stderr_into_stdout();

	process::Result run();

  private:
	static void setup_fd(int target_fd, const process::RedirectTo &redir);

	std::vector<std::string> args_;
	process::RedirectTo stdout_;
	process::RedirectTo stderr_;
	bool merge_stderr_ = false;
};

namespace process {

process::Result run(std::vector<std::string> args, process::RedirectTo stdout_redir = process::RedirectTo::inherit(),
					process::RedirectTo stderr_redir = process::RedirectTo::inherit());

}

} // namespace pesto::utils