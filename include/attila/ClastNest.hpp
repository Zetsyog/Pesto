#ifndef ATTILA_CLAST_NEST_H
#define ATTILA_CLAST_NEST_H

#include <cloog/cloog.h>

class ClastNest {
  public:
	ClastNest();
	virtual ~ClastNest();

	void setClastRoot(struct clast_stmt *root);
	struct clast_stmt *getClastRoot();

	CloogState *getCloogState();
	CloogOptions *getCloogOptions();

  private:
	CloogState *cloogState;
	CloogOptions *cloogOptions;
	struct clast_stmt *root;
};

#endif // ATTILA_CLAST_NEST_H