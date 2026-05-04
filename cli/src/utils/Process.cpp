
/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#include <cstring>
#include <sstream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <pesto/utils/Process.hpp>

namespace pesto::utils {
process::RedirectTo process::RedirectTo::inherit() {
	return {};
}

process::RedirectTo process::RedirectTo::file(std::string path, bool append) {
	process::RedirectTo r;
	r.kind = Kind::File;
	r.path = std::move(path);
	r.append = append;
	return r;
}

process::RedirectTo process::RedirectTo::null() {
	process::RedirectTo r;
	r.kind = Kind::Null;
	return r;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Process — public interface
// ─────────────────────────────────────────────────────────────────────────────

Process &Process::command(std::vector<std::string> args) {
	args_ = std::move(args);
	return *this;
}

Process &Process::command(const std::string &cmd) {
	args_.clear();
	std::istringstream ss(cmd);
	std::string tok;
	while (ss >> tok)
		args_.push_back(tok);
	return *this;
}

Process &Process::stdout_to(process::RedirectTo r) {
	stdout_ = std::move(r);
	return *this;
}

Process &Process::stderr_to(process::RedirectTo r) {
	stderr_ = std::move(r);
	return *this;
}

Process &Process::merge_stderr_into_stdout() {
	merge_stderr_ = true;
	return *this;
}

process::Result Process::run() {
	if (args_.empty())
		throw std::invalid_argument("Process: no command specified");

	// Build null-terminated argv for execvp
	std::vector<char *> argv;
	argv.reserve(args_.size() + 1);
	for (auto &a : args_)
		argv.push_back(const_cast<char *>(a.c_str()));
	argv.push_back(nullptr);

	pid_t pid = ::fork();
	if (pid < 0)
		throw std::runtime_error(std::string("fork() failed: ") + strerror(errno));

	if (pid == 0) {
		// ── Child ────────────────────────────────────────────────────────────
		setup_fd(STDOUT_FILENO, stdout_);

		if (merge_stderr_) {
			if (::dup2(STDOUT_FILENO, STDERR_FILENO) < 0)
				_exit(127);
		} else {
			setup_fd(STDERR_FILENO, stderr_);
		}

		::execvp(argv[0], argv.data());
		::perror("execvp");
		_exit(127);
	}

	// ── Parent ───────────────────────────────────────────────────────────────
	int status = 0;
	while (::waitpid(pid, &status, 0) < 0) {
		if (errno != EINTR)
			throw std::runtime_error(std::string("waitpid() failed: ") + strerror(errno));
	}

	process::Result res;
	if (WIFEXITED(status)) {
		res.exit_code = WEXITSTATUS(status);
	} else if (WIFSIGNALED(status)) {
		res.signaled = true;
		res.signal_num = WTERMSIG(status);
		res.exit_code = -res.signal_num;
	}
	return res;
}

void Process::setup_fd(int target_fd, const process::RedirectTo &redir) {
	if (redir.kind == process::RedirectTo::Kind::Inherit)
		return;

	const char *path = (redir.kind == process::RedirectTo::Kind::Null) ? "/dev/null" : redir.path.c_str();

	int flags = O_WRONLY | O_CREAT;
	flags |= redir.append ? O_APPEND : O_TRUNC;

	int fd = ::open(path, flags, 0644);
	if (fd < 0) {
		::perror("open");
		_exit(127);
	}

	if (::dup2(fd, target_fd) < 0) {
		::perror("dup2");
		_exit(127);
	}
	::close(fd);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Convenience free function
// ─────────────────────────────────────────────────────────────────────────────

process::Result process::run(std::vector<std::string> args, process::RedirectTo stdout_redir,
							 process::RedirectTo stderr_redir) {
	return Process{}
		.command(std::move(args))
		.stdout_to(std::move(stdout_redir))
		.stderr_to(std::move(stderr_redir))
		.run();
}
} // namespace pesto::utils