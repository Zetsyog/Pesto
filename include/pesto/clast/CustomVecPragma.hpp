#ifndef PESTO_PASS_CLAST_CUSTOM_VEC_PRAGMA_H
#define PESTO_PASS_CLAST_CUSTOM_VEC_PRAGMA_H

#include <memory>
#include <string>
#include <vector>

#include <cloog/cloog.h>
#include <pesto/Pass.hpp>

namespace pesto {

namespace pass {

/**
 * A pass that will alter the clast
 * It will search for clast_for that are marked for vectorization
 * and will add the appropriate pragmas
 */
class CustomVecPragma : public IPass {
  private:
	std::string pragma;

  public:
	CustomVecPragma();
	virtual ~CustomVecPragma();

	virtual void applyScop(OslScop &nest) override;

	/**
	 * @brief set the pragma to add to the clast
	 */
	void setPragma(std::string pragma);
};

} // namespace pass

} // namespace pesto

#endif // PESTO_PASS_CLAST_CUSTOM_VEC_PRAGMA_H