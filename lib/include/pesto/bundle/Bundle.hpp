/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#pragma once

#include <filesystem>
#include <memory>
#include <vector>

#include <pesto/Utils.hpp>
#include <pesto/bundle/ByProduct.hpp>
#include <pesto/ir/IR.hpp>

namespace pesto {

class Bundle {
	using IR = ir::IR;

  public:
	utils::MetadataContainer metadata; // Arbitrary key-value metadata (type-erased, safe access via templated helpers)

	Bundle() = default;
	explicit Bundle(std::unique_ptr<IR> ir) : ir_(std::move(ir)) {}

	// No copy
	Bundle(const Bundle &) = delete;
	Bundle &operator=(const Bundle &) = delete;

	// Move only
	Bundle(Bundle &&) = default;
	Bundle &operator=(Bundle &&) = default;

	// IR access
	bool empty() const {
		return !ir_;
	}
	const std::string &ir_id() const {
		static const std::string empty_id = "empty";
		return ir_ ? ir_->id() : empty_id;
	}
	const IR *get_ir() const {
		return ir_.get();
	}
	IR *get_ir() {
		return ir_.get();
	}

	// Replace IR
	void set_ir(std::unique_ptr<IR> ir) {
		ir_ = std::move(ir);
	}

	// Metadata: key → type-erased value (safe access via templated helpers)
	// void set_meta(const std::string &key, MetaValue value) { metadata_[key] = std::move(value); }

	// template <typename T> void set_meta_typed(const std::string &key, T value) {
	// 	metadata_.emplace(key, MetaValue::make<T>(std::move(value)));
	// }

	// template <typename T> std::optional<T> get_meta_typed(const std::string &key) const {
	// 	auto it = metadata_.find(key);
	// 	if (it == metadata_.end())
	// 		return std::nullopt;
	// 	return it->second.try_get<T>();
	// }

	// Byproducts: files only (extend as needed)
	void add_byproduct(std::unique_ptr<IByProduct> byproduct) {
		byproducts_.push_back(std::move(byproduct));
	}
	const std::vector<std::unique_ptr<IByProduct>> &byproducts() const {
		return byproducts_;
	}

  private:
	std::unique_ptr<IR> ir_;
	// Metadata metadata_; // see below
	std::vector<std::unique_ptr<IByProduct>> byproducts_;
};

} // namespace pesto
