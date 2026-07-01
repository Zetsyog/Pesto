#pragma once

#include <ostream>

#include <isl/set.h>

namespace pesto::tpz {

class TpzMaxCut {
  public:
	TpzMaxCut();
	~TpzMaxCut();

	void setDomain(__isl_take isl_set *domain);
	void setTileSize(int t);

	void emitC(std::ostream &os);

  private:
	/* The original domain */
	isl_set *domain_;
	unsigned int tile_size_;

	__isl_give isl_pw_multi_aff *compute_min_x1(__isl_take isl_set *domain);
};

} // namespace pesto::tpz