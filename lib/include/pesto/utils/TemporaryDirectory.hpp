/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace pesto::utils {

// Own a private directory, including cleanup when a pipeline throws.
class TemporaryDirectory {
  public:
	explicit TemporaryDirectory(const std::filesystem::path &parent = std::filesystem::temp_directory_path()) {
		const auto pattern = (parent / ".pesto-XXXXXX").string();
		std::vector<char> name(pattern.begin(), pattern.end());
		name.push_back('\0');
		if (!::mkdtemp(name.data())) {
			throw std::runtime_error("Could not create temporary directory in '" + parent.string() + "'");
		}
		path_ = name.data();
	}

	TemporaryDirectory(const TemporaryDirectory &) = delete;
	TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

	~TemporaryDirectory() {
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}

	const std::filesystem::path &path() const {
		return path_;
	}

  private:
	std::filesystem::path path_;
};

} // namespace pesto::utils
