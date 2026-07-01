#include <isl/aff.h>
#include <isl/map.h>
#include <isl/set.h>
#include <isl/space.h>

#include <pesto/atile/TpzMaxCut.hpp>

namespace pesto::tpz {

TpzMaxCut::TpzMaxCut() : domain_(nullptr), tile_size_(0) {}

TpzMaxCut::~TpzMaxCut() {
	if (domain_) {
		isl_set_free(domain_);
	}
}

void TpzMaxCut::setDomain(__isl_take isl_set *domain) {
	if (domain_) {
		isl_set_free(domain_);
	}
	domain_ = isl_set_copy(domain);
	isl_set_free(domain);
}

void TpzMaxCut::setTileSize(int t) {
	tile_size_ = t;
}

__isl_give isl_pw_multi_aff *TpzMaxCut::compute_min_x1(__isl_take isl_set *domain) {
	// Implementation of the computation for min_x1 goes here
	isl_space *space = isl_set_get_space(domain);
	isl_size n_out = isl_space_dim(space, isl_dim_out);
	space = isl_space_move_dims(space, isl_dim_in, 0, isl_dim_out, 0, n_out);
	space = isl_space_add_dims(space, isl_dim_out, n_out);

	isl_map *map = isl_map_identity(space);
	map = isl_map_remove_dims(map, isl_dim_out, 1, n_out - 1);

	domain = isl_set_apply(domain, map);

	isl_set_dump(domain);
	return isl_set_lexmin_pw_multi_aff(domain);
}

void TpzMaxCut::emitC(std::ostream &os) {
	isl_pw_multi_aff *min_x1 = compute_min_x1(isl_set_copy(domain_));

	isl_pw_multi_aff_dump(min_x1);

	isl_pw_multi_aff_free(min_x1);
}

} // namespace pesto::tpz
