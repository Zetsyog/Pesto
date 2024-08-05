#ifndef PESTO_METADATACONTAINER_H
#define PESTO_METADATACONTAINER_H

#include <any>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeindex>
#include <unordered_map>

class MetadataContainer {
  public:
	// Set a metadata value for a given key
	template <typename T, typename... Args> void set(Args &&...args);

	// Get a metadata value for a given key
	template <typename T> std::optional<std::shared_ptr<T>> get() const;

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
#endif // PESTO_METADATACONTAINER_H
