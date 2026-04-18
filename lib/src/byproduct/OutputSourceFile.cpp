#include <pesto/bundle/byproduct/OutputSourceFile.hpp>

using pesto::bundle::byproduct::OutputSourceFile;

OutputSourceFile::OutputSourceFile(std::filesystem::path path) : path_(std::move(path)) {}

std::filesystem::path OutputSourceFile::path() const {
	return path_;
}