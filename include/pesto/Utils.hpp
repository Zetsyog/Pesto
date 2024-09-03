#ifndef PESTO_METADATACONTAINER_H
#define PESTO_METADATACONTAINER_H

#include <any>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

/**
 * A container for metadata that can be attached to any object
 * The metadata is stored as a map from type to value
 * The metadata is stored as a shared pointer, so that it can be shared between
 * multiple objects
 */
class MetadataContainer {
  public:
	/**
	 * Set a metadata value for a given key
	 */
	template <typename T, typename... Args> void set(Args &&...args);

	/**
	 * Get a metadata value for a given key
	 */
	template <typename T> std::optional<std::shared_ptr<T>> get() const;

	/**
	 * Get a metadata value for a given key, or create it if it does not exist
	 */
	template <typename T, typename... Args>
	std::shared_ptr<T> getOrCreate(Args &&...args);

  private:
	std::unordered_map<std::type_index, std::any> metadata_;
};

// Template function definitions need to be in the header file
template <typename T>
std::optional<std::shared_ptr<T>> MetadataContainer::get() const {
	std::type_index key = typeid(T);
	auto it = metadata_.find(key);
	if (it == metadata_.end()) {
		return std::nullopt;
	}
	return std::any_cast<std::shared_ptr<T>>(it->second);
}

template <typename T, typename... Args>
void MetadataContainer::set(Args &&...args) {
	std::type_index key = typeid(T);

	auto it = metadata_.find(key);
	if (it != metadata_.end()) {
		throw std::runtime_error("Metadata already set");
	}
	metadata_[key] = std::make_shared<T>(T(std::forward<Args>(args)...));
}

template <typename T, typename... Args>
std::shared_ptr<T> MetadataContainer::getOrCreate(Args &&...args) {
	std::type_index key = typeid(T);
	auto it = metadata_.find(key);
	if (it == metadata_.end()) {
		metadata_[key] = std::make_shared<T>(T(std::forward<Args>(args)...));
	}
	return get<T>().value();
}
#endif // PESTO_METADATACONTAINER_H
