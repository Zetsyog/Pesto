#include "pesto/ClastNest.hpp"

ClastNest::ClastNest() {
	this->cloogState = cloog_state_malloc();
	this->cloogOptions = cloog_options_malloc(this->cloogState);
	this->root = nullptr;
}

ClastNest::~ClastNest() {
	if (this->root != nullptr)
		cloog_clast_free(this->root);
	cloog_options_free(this->cloogOptions);
	cloog_state_free(this->cloogState);
}

void ClastNest::setClastRoot(struct clast_stmt *root) {
	if (this->root != nullptr)
		cloog_clast_free(this->root);
	this->root = root;
}

struct clast_stmt *ClastNest::getClastRoot() { return this->root; }

CloogState *ClastNest::getCloogState() { return this->cloogState; }

CloogOptions *ClastNest::getCloogOptions() { return this->cloogOptions; }
