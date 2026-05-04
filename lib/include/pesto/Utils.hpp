/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <any>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

namespace pesto {

namespace utils {

class MetadataContainer {
  public:
	// Returns a reference to the stored instance of type T.
	// Default-constructs it if not already present.
	template <typename T> T &get() {
		const std::type_index key{typeid(T)};
		auto it = store_.find(key);
		if (it == store_.end()) {
			auto [inserted_it, _] = store_.emplace(key, T{});
			return std::any_cast<T &>(inserted_it->second);
		}
		return std::any_cast<T &>(it->second);
	}

	// Returns a const reference; throws if type is not present.
	template <typename T> const T &get() const {
		const std::type_index key{typeid(T)};
		auto it = store_.find(key);
		if (it == store_.end()) {
			throw std::out_of_range{std::string{"MetadataContainer: type not found: "} + typeid(T).name()};
		}
		return std::any_cast<const T &>(it->second);
	}

	// Explicitly store a value (replaces any existing entry of that type).
	template <typename T> void set(T value) {
		store_.insert_or_assign(std::type_index{typeid(T)}, std::move(value));
	}

	// Returns true if an instance of T is stored.
	template <typename T> bool has() const {
		return store_.count(std::type_index{typeid(T)}) > 0;
	}

	// Removes the entry for type T. Returns true if something was actually removed.
	template <typename T> bool erase() {
		return store_.erase(std::type_index{typeid(T)}) > 0;
	}

	void clear() {
		store_.clear();
	}
	std::size_t size() const {
		return store_.size();
	}

  private:
	std::unordered_map<std::type_index, std::any> store_;
};

} // namespace utils

} // namespace pesto
