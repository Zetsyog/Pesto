#ifndef PESTO_METADATACONTAINER_H
#define PESTO_METADATACONTAINER_H

#include <any>
#include <stdexcept>
#include <string>
#include <unordered_map>

class MetadataContainer {
  public:
	// Set a metadata value for a given key
	void set(const std::string &key, const std::any &value);

	// Get a metadata value for a given key
	template <typename T> T get(const std::string &key) const;

	// Check if a key exists in the metadata
	bool has(const std::string &key) const;

  private:
	std::unordered_map<std::string, std::any> metadata_;
};

// Template function definitions need to be in the header file
template <typename T> T MetadataContainer::get(const std::string &key) const {
	auto it = metadata_.find(key);
	if (it != metadata_.end()) {
		return std::any_cast<T>(it->second);
	} else {
		throw std::runtime_error("Key not found");
	}
}

#endif // PESTO_METADATACONTAINER_H
