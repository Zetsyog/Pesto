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
	template <typename T, typename... Args> void set(Args &&...args) {
		std::type_index key = typeid(T);

		auto it = metadata_.find(key);
		if (it != metadata_.end()) {
			throw std::runtime_error("Metadata already set");
		}
		std::shared_ptr<T> value =
			std::make_shared<T>(std::forward<Args>(args)...);
		metadata_[key] = std::make_shared<std::any>(value);
	}

	/**
	 * Get a metadata value for a given key
	 */
	template <typename T> std::weak_ptr<T> get() const {
		std::type_index key = typeid(T);
		auto it = metadata_.find(key);
		if (it == metadata_.end()) {
			return std::weak_ptr<T>(std::shared_ptr<T>(nullptr));
		}
		return std::weak_ptr<T>(std::any_cast<std::shared_ptr<T>>(*it->second));
	}

	/**
	 * Get a metadata value for a given key, or create it if it does not exist
	 */
	template <typename T, typename... Args>
	std::weak_ptr<T> getOrCreate(Args &&...args) {
		if (!this->get<T>().lock()) {
			this->set<T>(std::forward<Args>(args)...);
		}
		return get<T>();
	}

  private:
	std::unordered_map<std::type_index, std::shared_ptr<std::any>> metadata_;
};

#endif // PESTO_METADATACONTAINER_H
