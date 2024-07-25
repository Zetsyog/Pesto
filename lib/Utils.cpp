#include "pesto/Utils.hpp"

void MetadataContainer::set(const std::string &key, const std::any &value) {
	metadata_[key] = value;
}

bool MetadataContainer::has(const std::string &key) const {
	return metadata_.find(key) != metadata_.end();
}