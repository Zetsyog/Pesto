#include <algorithm>
#include <memory>

#include <isl/aff.h>
#include <isl/ctx.h>
#include <isl/set.h>

#include <cloog/isl/cloog.h>

#include <trahrhe/trahrhe.h>

#include <pesto/Pesto.hpp>
#include <pesto/clast/clast_support.h>
#include <pesto/isl_support.h>

pesto::ParamTilingUtility::ParamTilingUtility()
	: insertedStmts(0)
	, tilesizeTarget(32)
	, error(false)
	, boundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST)
	, boundCorrectionStrategy(ptile::codegen::BoundCorrectionStrategy::NO_CORRECTION)
	, volumeCheckStrategy(ptile::codegen::VolumeCheckStrategy::NO_CHECK) {
	this->ctx = isl_ctx_alloc();
}

pesto::ParamTilingUtility::~ParamTilingUtility() { isl_ctx_free(this->ctx); }

// TODO: return status
void pesto::ParamTilingUtility::apply(CloogProgram *cloogProgram, CloogOptions *cloogOptions, struct clast_root *root) {
	if (root == nullptr) {
		this->error = true;
		this->errorMessage = "Clast root is null";
		return;
	}
	if (this->error) {
		return;
	}

	this->names.clear();
	this->statements.clear();
	this->varList.clear();
	this->insertedStmts = 0;
	this->error = false;
	this->errorMessage = "";

	// Then we must create group of statement that can be tiled together
	// i.e tiling bands
	std::cerr << "[param-tiling] Computing stmt groups..." << std::endl;
	auto stmtGroups = this->computeStmtGroups(root);

	std::cerr << "[param-tiling] Found " << stmtGroups.size() << " statements groups" << std::endl;

	if (stmtGroups.size() == 0) {
		// TODO: this error handling seems not appropriate...
		this->error = true;
		this->errorMessage = "No statement groups found";
		return;
	}

	std::cerr << "[param-tiling] Applying tiling..." << std::endl;

	// then for each statement group we apply the tiling
	for (auto &group : stmtGroups) {
		this->tileGroup(cloogProgram, cloogOptions, root, group);
	}

	for (auto &group : stmtGroups) {
		this->trahrheDomains.push_back(group.trahrheDomain);
	}
}

std::vector<pesto::StmtGroup> pesto::ParamTilingUtility::computeStmtGroups(struct clast_root *root) {
	int stmt_count = 0;
	// this is a list of pointer of user statements in the clast
	// they will be freed with the clast root so no need for memory management
	// here
	struct clast_stmt **stmt = clast_get_user_stmt_list((struct clast_stmt *)root, &stmt_count);
	std::vector<StmtGroup> stmtGroups;

	std::cerr << "[param-tiling] Found " << stmt_count << " statements in clast" << std::endl;
	if (stmt_count == 0) {
		return stmtGroups;
	}

	// vector of all our statements
	// we will remove them from this vector as we process them
	std::vector<struct clast_stmt *> stmt_to_process;
	for (int i = 0; i < stmt_count; i++) {
		stmt_to_process.push_back(stmt[i]);
	}

	/*
		The algorithm is as follows:
		1. Take the first statement and create a group with it
		2. Mark the statement as processed
		3. For each statement that has not been processed yet
			1. If the statement has the same parent clast_for as the group
				1. Add the statement to the group
				2. Mark the statement as processed

	*/

	// while we have statements to process
	while (stmt_to_process.size() != 0) {
		// list of statements that have the same parent for
		// we pick a statement
		struct clast_stmt *stmt = stmt_to_process.back();
		// we mark it as processed
		stmt_to_process.pop_back();

		// get the parent for of the group
		struct clast_for *parent_for = clast_get_parent_clast_for((struct clast_stmt *)root, stmt);
		// Check is this statement is inside a for loop
		if (parent_for == NULL) {
			// if not we skip it
			continue;
		}
		// the statement is inside a for loop
		// we create a group that will contain this statement and all the other statements inside the same for
		StmtGroup group;
		group.stmts.push_back(stmt);
		group.parent_for = parent_for;
		group.id = stmtGroups.size();

		// for each statement that has not been processed yet
		for (std::vector<struct clast_stmt *>::iterator it = stmt_to_process.begin(); it != stmt_to_process.end();) {
			struct clast_for *tmp = clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)*it);
			if (parent_for == tmp) {
				group.stmts.push_back(*it);
				it = stmt_to_process.erase(it);
			} else {
				++it;
			}
		}

		isl_set *group_domain = isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));
		isl_bool empty = isl_set_is_empty(group_domain);
		int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);
		isl_set_free(group_domain);

		if (empty == isl_bool_true || tiling_dim_count == 0) {
			continue;
		}

		// append group to the list of groups
		stmtGroups.push_back(group);
	}
	free(stmt);

	this->insertedStmts = stmt_count;

	return stmtGroups;
}

void pesto::ParamTilingUtility::tileGroup(CloogProgram *cloogProgram, CloogOptions *cloogOptions,
										  struct clast_root *root, StmtGroup &group) {
	isl_set *group_domain = isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));

	int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);

	// if there are no tiled dimensions we can skip this group
	if (tiling_dim_count == 0) {
		std::cerr << "No tiled dimensions found in group" << std::endl;
		return;
	}

	// First we need to associate for each tiled dimension its inner and outter
	// for
	this->associateTilingFor(root, group);

	for (auto &data : group.tilingData) {
		std::cerr << "Found tiling for pair : " << data.outter_for->iterator
				  << " is outter of: " << data.inner_for->iterator << std::endl;
	}

	// Then we need to sort the tiling_for ascendingly of the depth of the
	// outter for

	// Then for each outter for (from outter most to inner most)
	for (auto &data : group.tilingData) {
		this->insertStatements(cloogOptions, root, group, data);
		this->replaceOutterForBounds(group, data);
		this->replaceInnerForBounds(cloogProgram, group, data);
	}

	this->computeTrahrheDomain(cloogProgram, group);

	this->generateStatements(cloogProgram, group);
	this->declareGroupVariables(group);

	// then we need to handle the parallel loop and update openmp pragma
	this->updateOmpPragma(group);

	isl_set_free(group_domain);
}

std::string pesto::ParamTilingUtility::getErrorMessage() { return this->errorMessage; }

bool pesto::ParamTilingUtility::hasError() { return this->error; }

void pesto::ParamTilingUtility::associateTilingFor(struct clast_root *root, StmtGroup &group) {
	struct clast_for *current_for = group.parent_for;

	while (current_for != NULL) {
		// if the current for is an inner tiled for
		if (clast_for_is_inner_tile_loop(current_for, this->tilesizeTarget)) {
			// we get the corresponding outter for
			struct clast_for *outter_for =
				clast_get_outter_tiling_for((struct clast_stmt *)root, current_for, this->tilesizeTarget);
			if (outter_for == NULL) {
				std::cerr << "[param-tiling] Warning: found a inner tiled for "
							 "with no outter tiling for"
						  << std::endl;
				current_for = clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)current_for);
				continue;
			}
			TilingData data;

			data.outter_for = outter_for;
			data.inner_for = current_for;
			group.tilingData.push_back(data);
		}
		current_for = clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)current_for);
	}

	// sort the tiling data by depth of the outter for
	std::sort(group.tilingData.begin(), group.tilingData.end(), [](const TilingData &a, const TilingData &b) {
		return isl_set_n_dim(isl_set_from_cloog_domain(a.outter_for->domain)) <
			   isl_set_n_dim(isl_set_from_cloog_domain(b.outter_for->domain));
	});

	// update the tiling depth
	for (std::size_t i = 0; i < group.tilingData.size(); i++) {
		group.tilingData[i].tiling_depth = i;
	}
}

void pesto::ParamTilingUtility::pushTilingType(ParamTilingType type) { this->tilingTypes.push_back(type); }

pesto::ParamTilingType pesto::ParamTilingUtility::getTilingType(std::size_t depth) {
	// default is algebraic
	if (this->tilingTypes.size() == 0) {
		return ParamTilingType::ALGEBRAIC;
	}
	if (depth < this->tilingTypes.size()) {
		return this->tilingTypes[depth];
	}
	// if the depth is greater than the number of tiling types
	// we return the last one
	return this->tilingTypes[this->tilingTypes.size() - 1];
}

int pesto::ParamTilingUtility::islFirstTilingDimIdx(StmtGroup &group) {
	int firstTilingDim = 1000;
	for (auto &data : group.tilingData) {
		int dim = isl_set_n_dim(isl_set_from_cloog_domain(data.outter_for->domain));
		firstTilingDim = dim < firstTilingDim ? dim : firstTilingDim;
	}
	return firstTilingDim - 1;
}

void pesto::ParamTilingUtility::insertStatements(CloogOptions *cloogOptions, struct clast_root *root, StmtGroup &group,
												 TilingData &data) {
	struct clast_for *outter_for = data.outter_for;
	int tiling_depth = data.tiling_depth;

	group.tilingData[tiling_depth].head_stmt_id = ++(this->insertedStmts);
	group.tilingData[tiling_depth].body_stmt_id = ++(this->insertedStmts);

	struct clast_user_stmt *header_stmt =
		pesto_clast_create_stmt(cloogOptions, NULL, group.tilingData[tiling_depth].head_stmt_id);
	clast_insert_before((struct clast_stmt *)root, (struct clast_stmt *)outter_for, (struct clast_stmt *)header_stmt);

	struct clast_user_stmt *body_stmt =
		pesto_clast_create_stmt(cloogOptions, NULL, group.tilingData[tiling_depth].body_stmt_id);
	clast_insert_indepth((struct clast_stmt *)outter_for, (struct clast_stmt *)body_stmt);
}

void pesto::ParamTilingUtility::replaceInnerForBounds(CloogProgram *cloogProgram, StmtGroup &group, TilingData &data) {
	struct clast_for *inner_for = data.inner_for;
	int tiling_depth = data.tiling_depth;

	free_clast_expr(inner_for->LB);
	free_clast_expr(inner_for->UB);
	inner_for->LB = NULL;
	inner_for->UB = NULL;

	isl_set *domain = isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	domain = this->dropTilingConstraints(domain, group);

	isl_size ndim = isl_set_dim(domain, isl_dim_set);

	inner_for->LB = clast_bound_from_isl_set(cloogProgram->names, domain, 0, ndim - 1);
	inner_for->UB = clast_bound_from_isl_set(cloogProgram->names, domain, 1, ndim - 1);

	isl_set_free(domain);

	auto &tilingData = group.tilingData[tiling_depth];

	ptile::NameHelper nameHelper = this->getNameHelper(group, tilingData);

	auto lb_ptr = std::make_shared<std::string>(nameHelper.innerForLBName());
	struct clast_name *param_lb = new_clast_name(lb_ptr->c_str());
	this->names.push_back(lb_ptr);

	auto ub_ptr = std::make_shared<std::string>(nameHelper.innerForUBName());
	struct clast_name *param_ub = new_clast_name(ub_ptr->c_str());
	this->names.push_back(ub_ptr);

	if (inner_for->LB != NULL) {
		struct clast_reduction *red = NULL;
		red = new_clast_reduction(clast_red_max, 2);
		red->elts[0] = inner_for->LB;
		red->elts[1] = (struct clast_expr *)param_lb;
		inner_for->LB = (struct clast_expr *)red;
	} else {
		inner_for->LB = (struct clast_expr *)param_lb;
	}

	if (inner_for->UB != NULL) {
		struct clast_reduction *red = NULL;
		red = new_clast_reduction(clast_red_min, 2);
		red->elts[0] = inner_for->UB;
		red->elts[1] = (struct clast_expr *)param_ub;
		inner_for->UB = (struct clast_expr *)red;
	} else {
		inner_for->UB = (struct clast_expr *)param_ub;
	}
}

void pesto::ParamTilingUtility::replaceOutterForBounds(StmtGroup &group, TilingData &data) {
	struct clast_for *outter_for = data.outter_for;

	free_clast_expr(outter_for->LB);
	free_clast_expr(outter_for->UB);
	outter_for->LB = NULL;
	outter_for->UB = NULL;

	cloog_int_t zero;
	cloog_int_init(zero);
	outter_for->LB = (struct clast_expr *)new_clast_term(zero, NULL);

	ptile::NameHelper nameHelper = this->getNameHelper(group, data);

	auto ub_ptr = std::make_shared<std::string>(nameHelper.outterForUBName());
	outter_for->UB = (struct clast_expr *)new_clast_name(ub_ptr->c_str());
	this->names.push_back(ub_ptr);
}

void pesto::ParamTilingUtility::generateStatements(CloogProgram *cloogProgram, StmtGroup &group) {
	using StmtType = ptile::codegen::StmtType;

	for (auto &data : group.tilingData) {

		ptile::codegen::ParamTilingCodegen codegen = this->getCodegen(cloogProgram, group, data);
		std::cerr << "[param-tiling] Generating statements... (" << data.head_stmt_id << ", " << data.body_stmt_id
				  << ")" << std::endl;
		for (std::size_t i = 0; i < 2; ++i) {
			std::ostringstream oss;
			codegen.genStmt(oss, i == 0 ? StmtType::HEAD : StmtType::BODY);
			this->statements.push_back(
				std::make_pair(i == 0 ? data.head_stmt_id : data.body_stmt_id, std::string(oss.str())));
		}
	}
}

isl_set *pesto::ParamTilingUtility::dropTilingConstraints(__isl_take isl_set *set, StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (auto riter = group.tilingData.rbegin(); riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

		int ndim = isl_set_dim(isl_set_from_cloog_domain(data.outter_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set, ndim - 1, 1);
	}

	return res;
}

// TODO: remove because not used
isl_set *pesto::ParamTilingUtility::removeTilingDimensions(__isl_take isl_set *set, StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (auto riter = group.tilingData.rbegin(); riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

		int ndim = isl_set_dim(isl_set_from_cloog_domain(data.outter_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set, ndim - 1, 1);
		res = isl_set_remove_dims(res, isl_dim_set, ndim - 1, 1);
	}

	return res;
}

std::vector<std::shared_ptr<std::string>> pesto::ParamTilingUtility::getNames() { return this->names; }

std::vector<std::pair<int, std::string>> pesto::ParamTilingUtility::getStatements() { return this->statements; }

void pesto::ParamTilingUtility::setIdTrahrhePrefix(int id) { this->id = id; }

std::vector<std::string> pesto::ParamTilingUtility::getVarList() { return this->varList; }

void pesto::ParamTilingUtility::declareVariable(std::string var) {
	// we do not want to declare the same variable twice
	if (std::find(this->varList.begin(), this->varList.end(), var) != this->varList.end()) {
		return;
	}
	this->varList.push_back(var);
}

void pesto::ParamTilingUtility::declareGroupVariables(StmtGroup &group) {
	for (auto &data : group.tilingData) {
		auto nameHelper = this->getNameHelper(group, data);

		this->declareVariable(nameHelper.pcMaxName());
		this->declareVariable(nameHelper.targetVolName());
		this->declareVariable(nameHelper.outterForUBName());
		this->declareVariable(nameHelper.innerForLBName());
		this->declareVariable(nameHelper.innerForUBName());
	}
}

void pesto::ParamTilingUtility::updateOmpPragma(StmtGroup &group) {
	// first find the parallel for
	struct clast_for *parallel_for = NULL;
	int tiling_depth = -1;
	for (auto &data : group.tilingData) {
		struct clast_for *outter_for = data.outter_for;
		if (outter_for->parallel & CLAST_PARALLEL_OMP) {
			parallel_for = outter_for;
			tiling_depth = data.tiling_depth;
			break;
		}
	}

	// we could be doing sequential tiling
	if (parallel_for == NULL) {
		return;
	}

	// then we need to update the pragma so it can handle custom directives
	parallel_for->parallel = CLAST_PARALLEL_OMP | CLAST_PARALLEL_USER;
	std::string userDirectiveStr = "omp parallel for firstprivate(";

	auto nameHelper = this->getNameHelper(group, group.tilingData[0]);

	// omp firstprivate directive
	userDirectiveStr += nameHelper.outterForUBName() + ", ";
	userDirectiveStr += nameHelper.pcMaxName() + ", ";
	userDirectiveStr += nameHelper.targetVolName() + ") \\\n";

	parallel_for->user_directive = strdup(userDirectiveStr.c_str());

	// private variables
	std::string privateVarsStr = std::string(parallel_for->private_vars);

	// TODO: tile scheduling

	// now we can free old private vars
	free(parallel_for->private_vars);
	parallel_for->private_vars = NULL;

	if (privateVarsStr != "") {
		privateVarsStr += ", ";
	}
	nameHelper = this->getNameHelper(group, group.tilingData[tiling_depth]);

	privateVarsStr += nameHelper.innerForLBName();
	privateVarsStr += ", " + nameHelper.innerForUBName();

	for (std::size_t i = tiling_depth + 1; i < group.tilingData.size(); i += 1) {
		nameHelper = this->getNameHelper(group, group.tilingData[i]);
		privateVarsStr += ", " + nameHelper.outterForUBName();
		privateVarsStr += ", " + nameHelper.pcMaxName();
		privateVarsStr += ", " + nameHelper.targetVolName();
		privateVarsStr += ", " + nameHelper.innerForLBName();
		privateVarsStr += ", " + nameHelper.innerForUBName();
	}
	parallel_for->private_vars = strdup(privateVarsStr.c_str());
}

void pesto::ParamTilingUtility::computeTrahrheDomain(CloogProgram *cloogProgram, StmtGroup &group) {

	isl_set *domain = this->getInnerDomain(cloogProgram, group, group.tilingData.back());
	// rename the dimension
	for (std::size_t i = 0; i < group.tilingData.size(); i += 1) {
		auto &data = group.tilingData[i];
		ptile::NameHelper trahrheNameHelper = this->getTrahrheNameHelper(group, data);
		domain = isl_set_set_dim_name(domain, isl_dim_set, i, trahrheNameHelper.trahrheIteratorName().c_str());
	}

	int ntiled_dim = 0;
	for (std::size_t i = 0; i < group.tilingData.size(); i += 1) {
		if (this->getTilingType(i) != ParamTilingType::RECTANGULAR) {
			ntiled_dim += 1;
		}
	}

	isl_printer *printer = isl_printer_to_str(ctx);

	printer = isl_printer_print_set(printer, domain);

	char *str = isl_printer_get_str(printer);

	group.trahrheDomain.first = ntiled_dim;
	group.trahrheDomain.second = std::string(str);

	free(str);
	isl_printer_free(printer);

	isl_set_free(domain);
}

std::vector<std::pair<int, std::string>> pesto::ParamTilingUtility::getTrahrheDomains() { return trahrheDomains; }

__isl_give isl_set *pesto::ParamTilingUtility::extractIslInnerTiledDomain(StmtGroup &group, __isl_take isl_set *domain,
																		  std::size_t ndim) {
	assert(ndim <= group.tilingData.size());

	// we are going to build a map
	// that will transform the current dimensions to keep only the tiled
	// dimensions in the order of the tiling iterators
	isl_space *space = isl_set_get_space(domain);
	isl_map *trans = isl_map_universe(space);

	// we move all dimensions to input dimensions
	trans = isl_map_move_dims(trans, isl_dim_in, 0, isl_dim_set, 0, isl_map_dim(trans, isl_dim_set));
	// we have as much output dimensions as tiled dimensions
	trans = isl_map_add_dims(trans, isl_dim_out, ndim);

	// for each tiled dimensionss
	for (std::size_t i = 0; i < ndim; i += 1) {
		auto &data = group.tilingData[i];

		// we add a constraint [input_dim] = [output_dim]
		// where [input_dim] is the dimension in the original set
		// and [output_dim] is the corresponding dimension in the new space
		isl_constraint *cst = isl_constraint_alloc_equality(isl_local_space_from_space(isl_map_get_space(trans)));

		int in_idx = isl_map_find_dim_by_name(trans, isl_dim_in, data.inner_for->iterator);
		cst = isl_constraint_set_coefficient_si(cst, isl_dim_in, in_idx, -1);
		cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, i, 1);
		trans = isl_map_add_constraint(trans, cst);
	}

	// apply the map to original domain
	isl_set *out = isl_set_apply(domain, trans);

	return out;
}

void pesto::ParamTilingUtility::setBoundComputingMethod(pesto::ptile::codegen::BoundComputeMethod method) {
	this->boundComputeMethod = method;
}

void pesto::ParamTilingUtility::setTileSizeTarget(int size) { this->tilesizeTarget = size; }

__isl_give isl_set *pesto::ParamTilingUtility::moveNonTiledDimensionsToParameters(__isl_take isl_set *set,
																				  StmtGroup &group) {

	isl_set *domain = isl_set_copy(set);
	isl_set_free(set);

	isl_size ndim = isl_set_dim(set, isl_dim_set);
	// TODO: move untiled dimensions as parameters
	for (isl_size i = 0; i < ndim; i++) {
		// ignore scalar dim
		if (isl_set_is_dim_scalar(domain, i)) {
			continue;
		}
		bool is_tiled = false;
		// search if this dimension is tiled
		for (std::size_t j = 0; j < group.tilingData.size(); j += 1) {
			auto &data = group.tilingData[j];
			int in_idx = isl_set_find_dim_by_name(domain, isl_dim_set, data.inner_for->iterator);
			int out_idx = isl_set_find_dim_by_name(domain, isl_dim_set, data.outter_for->iterator);
			if (i == in_idx || i == out_idx) {
				is_tiled = true;
				break;
			}
		}
		if (is_tiled) {
			continue;
		}
		std::cerr << "dim " << isl_set_get_dim_name(domain, isl_dim_set, i) << "(" << i << ") will be moved to param "
				  << std::endl;
		isl_size npar = isl_set_dim(domain, isl_dim_param);
		domain = isl_set_move_dims(domain, isl_dim_param, npar, isl_dim_set, i, 1);
		i -= 1;
		ndim = isl_set_dim(domain, isl_dim_set);
	}
	return domain;
}

__isl_give isl_set *pesto::ParamTilingUtility::getInnerDomain(CloogProgram *cloogProgram, StmtGroup &group,
															  TilingData &data) {
	isl_set *inner_domain = isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));
	// define param names
	for (int i = 0; i < cloogProgram->names->nb_parameters; i += 1) {
		inner_domain = isl_set_set_dim_name(inner_domain, isl_dim_param, i, cloogProgram->names->parameters[i]);
	}

	for (isl_size i = 0; i < isl_set_n_dim(inner_domain); i += 1) {
		inner_domain = isl_set_set_dim_name(inner_domain, isl_dim_set, i, cloogProgram->names->scattering[i]);
	}
	inner_domain = this->moveNonTiledDimensionsToParameters(inner_domain, group);
	inner_domain = this->extractIslInnerTiledDomain(group, inner_domain, data.tiling_depth + 1);

	return inner_domain;
}

pesto::ptile::NameHelper pesto::ParamTilingUtility::getNameHelper(StmtGroup &group, TilingData &data) {
	ptile::NameHelper nameHelper(data.tiling_depth, data.outter_for->iterator, data.inner_for->iterator);
	nameHelper.setPrefix("g" + std::to_string(group.id));
	return nameHelper;
}

pesto::ptile::NameHelper pesto::ParamTilingUtility::getTrahrheNameHelper(StmtGroup &group, TilingData &data) {
	ptile::NameHelper helper = this->getNameHelper(group, data);
	helper.setPrefix("n" + std::to_string(this->id) + helper.getSeparator() + "g" + std::to_string(group.id));
	return helper;
}

void pesto::ParamTilingUtility::setBoundCorrectionStrategy(pesto::ptile::codegen::BoundCorrectionStrategy strategy) {
	this->boundCorrectionStrategy = strategy;
}
void pesto::ParamTilingUtility::setVolumeCheckStrategy(pesto::ptile::codegen::VolumeCheckStrategy strategy) {
	this->volumeCheckStrategy = strategy;
}

pesto::ptile::codegen::ParamTilingCodegen pesto::ParamTilingUtility::getCodegen(CloogProgram *cloogProgram,
																				StmtGroup &group, TilingData &data) {
	ptile::codegen::ParamTilingCodegen codegen(group, *data.outter_for, *data.inner_for, data.tiling_depth, this->id,
											   this->getInnerDomain(cloogProgram, group, data));
	codegen.setBoundComputeMethod(this->boundComputeMethod);
	codegen.setBoundCorrectionStrategy(this->boundCorrectionStrategy);
	codegen.setVolumeCheckStrategy(this->volumeCheckStrategy);
	codegen.setTilingType(this->getTilingType(data.tiling_depth));
	return codegen;
}