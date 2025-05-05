#include <algorithm>
#include <memory>

#include <isl/aff.h>
#include <isl/ctx.h>
#include <isl/set.h>

#include <cloog/isl/cloog.h>

#include <trahrhe/trahrhe.h>

#include <pesto/ParamTiling.hpp>
#include <pesto/Pesto.hpp>
#include <pesto/clast/clast_support.h>
#include <pesto/isl_support.h>

#define STR_BUF_SIZE 512

pesto::ParamTilingUtility::ParamTilingUtility()
	: scop(nullptr)
	, root(nullptr)
	, insertedStmts(0)
	, tilesizeTarget(32)
	, error(false)
	, boundComputeMethod(ptile::codegen::BoundComputeMethod::FILL_LAST) {
	this->ctx = isl_ctx_alloc();
}

pesto::ParamTilingUtility::~ParamTilingUtility() { isl_ctx_free(this->ctx); }

// TODO: return status
void pesto::ParamTilingUtility::apply() {
	this->sanityCheck();
	if (this->error) {
		return;
	}
	// First we check if param tiling is possible
	if (!canBeTiled()) {
		this->errorMessage = "Scop cannot be tiled";
		this->error = true;
		return;
	}

	this->insertedStmts = osl_statement_number(scop->statement);

	// Then we must create group of statement that can be tiled together
	// i.e tiling bands
	std::cerr << "[param-tiling] Computing stmt groups..." << std::endl;
	this->computeStmtGroups();

	std::cerr << "[param-tiling] Found " << this->stmtGroups.size() << " statements groups" << std::endl;

	if (this->stmtGroups.size() == 0) {
		// TODO: this error handling seems not appropriate...
		this->error = true;
		this->errorMessage = "No statement groups found";
		return;
	}

	std::cerr << "[param-tiling] Applying tiling..." << std::endl;

	// then for each statement group we apply the tiling
	for (auto &group : this->stmtGroups) {
		this->tileGroup(*group);
	}
}

void pesto::ParamTilingUtility::computeStmtGroups() {
	int stmt_count = 0;
	// this is a list of pointer of user statements in the clast
	// they will be freed with the clast root so no need for memory management
	// here
	struct clast_stmt **stmt = clast_get_user_stmt_list((struct clast_stmt *)this->root, &stmt_count);

	std::cerr << "[param-tiling] Found " << stmt_count << " statements in clast" << std::endl;

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
		std::unique_ptr<struct StmtGroup> group = std::make_unique<struct StmtGroup>();

		// take the first statement that has not been processed
		group->stmts.push_back(stmt_to_process.back());
		// we mark it as processed
		stmt_to_process.pop_back();

		// get the parent for of the group
		struct clast_for *parent_for =
			clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)group->stmts.begin()[0]);
		group->parent_for = parent_for;
		group->id = this->stmtGroups.size();

		// for each statement that has not been processed yet
		for (std::vector<struct clast_stmt *>::iterator it = stmt_to_process.begin(); it != stmt_to_process.end();) {
			struct clast_for *tmp = clast_get_parent_clast_for((struct clast_stmt *)root, (struct clast_stmt *)*it);
			if (parent_for == tmp) {
				group->stmts.push_back(*it);
				it = stmt_to_process.erase(it);
			} else {
				++it;
			}
		}

		isl_set *group_domain = isl_set_copy(isl_set_from_cloog_domain(group->parent_for->domain));
		isl_bool empty = isl_set_is_empty(group_domain);
		int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);
		isl_set_free(group_domain);

		if (empty == isl_bool_true || tiling_dim_count == 0) {
			continue;
		}

		// append group to the list of groups
		this->stmtGroups.push_back(std::move(group));
	}
	free(stmt);
}

void pesto::ParamTilingUtility::tileGroup(StmtGroup &group) {
	isl_set *group_domain = isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));

	int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);

	// if there are no tiled dimensions we can skip this group
	if (tiling_dim_count == 0) {
		std::cerr << "No tiled dimensions found in group" << std::endl;
		return;
	}

	// First we need to associate for each tiled dimension its inner and outter
	// for
	this->associateTilingFor(group);

	for (auto &data : group.tilingData) {
		std::cerr << "Found tiling for pair : " << data->outter_for->iterator
				  << " is outter of: " << data->inner_for->iterator << std::endl;
	}

	// Then we need to sort the tiling_for ascendingly of the depth of the
	// outter for

	// Then for each outter for (from outter most to inner most)
	for (auto &data : group.tilingData) {
		struct clast_for *outter_for = data->outter_for;
		struct clast_for *inner_for = data->inner_for;
		int tiling_depth = data->tiling_depth;

		isl_set *inner_domain = this->getInnerDomain(group, data);

		// this->computeCodegenData(group, outter_for, inner_for, tiling_depth);
		auto codegen = std::make_unique<ptile::codegen::ParamTilingCodegen>(group, *outter_for, *inner_for,
																			tiling_depth, this->id, inner_domain);
		isl_set_free(inner_domain);
		codegen->setTilingType(this->getTilingType(tiling_depth));
		codegen->setBoundComputeMethod(this->boundComputeMethod);
		data->codegen = std::move(codegen);

		this->insertStatements(group, outter_for, inner_for, tiling_depth);
		this->replaceOutterForBounds(group, outter_for, inner_for, tiling_depth);
		this->replaceInnerForBounds(group, outter_for, inner_for, tiling_depth);
	}

	this->computeTrahrheDomain(group);

	this->generateStatements(group);
	this->declareGroupVariables(group);

	// then we need to handle the parallel loop and update openmp pragma
	this->updateOmpPragma(group);

	isl_set_free(group_domain);
}

bool pesto::ParamTilingUtility::canBeTiled() {
	// TODO: better alg
	// check if scops has iterator dimensions
	osl_statement_p stmt = this->scop->statement;
	while (stmt != NULL) {
		if (stmt->domain->nb_output_dims == 0) {
			return false;
		}
		if (stmt->domain->nb_rows == 0) {
			return false;
		}
		stmt = stmt->next;
	}
	return true;
}

void pesto::ParamTilingUtility::sanityCheck() {
	if (this->scop == nullptr) {
		this->error = true;
		this->errorMessage = "Scop not set";
		return;
	}
	if (this->root == nullptr) {
		this->error = true;
		this->errorMessage = "Clast root not set";
		return;
	}
	if (this->scop->statement == nullptr) {
		this->error = true;
		this->errorMessage = "Scop has no statement";
		return;
	}
}

std::string pesto::ParamTilingUtility::getErrorMessage() { return this->errorMessage; }

bool pesto::ParamTilingUtility::hasError() { return this->error; }

void pesto::ParamTilingUtility::setClastRoot(struct clast_root *root) { this->root = root; }

void pesto::ParamTilingUtility::associateTilingFor(StmtGroup &group) {
	struct clast_for *current_for = group.parent_for;

	while (current_for != NULL) {
		// if the current for is an inner tiled for
		if (clast_for_is_inner_tile_loop(current_for, this->tilesizeTarget)) {
			// we get the corresponding outter for
			struct clast_for *outter_for =
				clast_get_outter_tiling_for((struct clast_stmt *)this->root, current_for, this->tilesizeTarget);
			if (outter_for == NULL) {
				std::cerr << "[param-tiling] Warning: found a inner tiled for "
							 "with no outter tiling for"
						  << std::endl;
				current_for =
					clast_get_parent_clast_for((struct clast_stmt *)this->root, (struct clast_stmt *)current_for);
				continue;
			}
			auto data = std::make_unique<struct TilingData>();
			data->outter_for = outter_for;
			data->inner_for = current_for;
			group.tilingData.push_back(std::move(data));
		}
		current_for = clast_get_parent_clast_for((struct clast_stmt *)this->root, (struct clast_stmt *)current_for);
	}

	// sort the tiling data by depth of the outter for
	std::sort(group.tilingData.begin(), group.tilingData.end(),
			  [](const std::unique_ptr<struct TilingData> &a, const std::unique_ptr<struct TilingData> &b) {
				  return isl_set_n_dim(isl_set_from_cloog_domain(a->outter_for->domain)) <
						 isl_set_n_dim(isl_set_from_cloog_domain(b->outter_for->domain));
			  });

	// update the tiling depth
	for (std::size_t i = 0; i < group.tilingData.size(); i++) {
		group.tilingData[i]->tiling_depth = i;
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

int pesto::ParamTilingUtility::islFirstTilingDimIdx(struct StmtGroup &group) {
	int firstTilingDim = 1000;
	for (auto &data : group.tilingData) {
		int dim = isl_set_n_dim(isl_set_from_cloog_domain(data->outter_for->domain));
		firstTilingDim = dim < firstTilingDim ? dim : firstTilingDim;
	}
	return firstTilingDim - 1;
}

void pesto::ParamTilingUtility::setCloogProgram(CloogProgram *program) { this->cloogProgram = program; }

void pesto::ParamTilingUtility::setCloogOptions(CloogOptions *options) { this->cloogOptions = options; }

void pesto::ParamTilingUtility::insertStatements(struct StmtGroup &group, struct clast_for *outter_for,
												 struct clast_for *inner_for, int tiling_depth) {
	(void)inner_for;

	group.tilingData[tiling_depth]->head_stmt_id = ++(this->insertedStmts);
	group.tilingData[tiling_depth]->body_stmt_id = ++(this->insertedStmts);

	struct clast_user_stmt *header_stmt =
		pesto_clast_create_stmt(this->cloogOptions, NULL, group.tilingData[tiling_depth]->head_stmt_id);
	clast_insert_before((struct clast_stmt *)this->root, (struct clast_stmt *)outter_for,
						(struct clast_stmt *)header_stmt);

	struct clast_user_stmt *body_stmt =
		pesto_clast_create_stmt(this->cloogOptions, NULL, group.tilingData[tiling_depth]->body_stmt_id);
	clast_insert_indepth((struct clast_stmt *)outter_for, (struct clast_stmt *)body_stmt);
}

void pesto::ParamTilingUtility::replaceInnerForBounds(struct StmtGroup &group, struct clast_for *outter_for,
													  struct clast_for *inner_for, int tiling_depth) {
	// suppress warning
	(void)outter_for;

	free_clast_expr(inner_for->LB);
	free_clast_expr(inner_for->UB);
	inner_for->LB = NULL;
	inner_for->UB = NULL;

	isl_set *domain = isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	domain = this->dropTilingConstraints(domain, group);

	isl_size ndim = isl_set_dim(domain, isl_dim_set);

	inner_for->LB = clast_bound_from_isl_set(this->cloogProgram->names, domain, 0, ndim - 1);
	inner_for->UB = clast_bound_from_isl_set(this->cloogProgram->names, domain, 1, ndim - 1);

	isl_set_free(domain);

	auto &tilingData = group.tilingData[tiling_depth];

	auto lb_ptr = std::make_shared<std::string>(std::string(tilingData->codegen->getInnerForLBName()));
	struct clast_name *param_lb = new_clast_name(lb_ptr->c_str());
	this->names.push_back(lb_ptr);

	auto ub_ptr = std::make_shared<std::string>(std::string(tilingData->codegen->getInnerForUBName()));
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

void pesto::ParamTilingUtility::replaceOutterForBounds(struct StmtGroup &group, struct clast_for *outter_for,
													   struct clast_for *inner_for, int tiling_depth) {
	// suppress warning
	(void)inner_for;

	free_clast_expr(outter_for->LB);
	free_clast_expr(outter_for->UB);
	outter_for->LB = NULL;
	outter_for->UB = NULL;

	cloog_int_t zero;
	cloog_int_init(zero);
	outter_for->LB = (struct clast_expr *)new_clast_term(zero, NULL);

	auto ub_ptr =
		std::make_shared<std::string>(std::string(group.tilingData[tiling_depth]->codegen->getOutterForUBName()));
	outter_for->UB = (struct clast_expr *)new_clast_name(ub_ptr->c_str());
	this->names.push_back(ub_ptr);
}

void pesto::ParamTilingUtility::generateStatements(struct StmtGroup &group) {

	for (auto &data : group.tilingData) {
		std::cerr << "[param-tiling] Generating statements... (" << data->head_stmt_id << ", " << data->body_stmt_id
				  << ")" << std::endl;
		for (std::size_t i = 0; i < 2; ++i) {
			std::ostringstream oss;
			data->codegen->genStmt(oss, i == 0 ? ptile::codegen::HEAD : ptile::codegen::BODY);
			this->statements.push_back(
				std::make_pair(i == 0 ? data->head_stmt_id : data->body_stmt_id, std::string(oss.str())));
		}
	}
}

isl_set *pesto::ParamTilingUtility::dropTilingConstraints(__isl_take isl_set *set, struct StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (std::vector<std::unique_ptr<struct TilingData>>::reverse_iterator riter = group.tilingData.rbegin();
		 riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

		int ndim = isl_set_dim(isl_set_from_cloog_domain(data->outter_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set, ndim - 1, 1);
	}

	return res;
}

// TODO: remove because not used
isl_set *pesto::ParamTilingUtility::removeTilingDimensions(__isl_take isl_set *set, struct StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (std::vector<std::unique_ptr<struct TilingData>>::reverse_iterator riter = group.tilingData.rbegin();
		 riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

		int ndim = isl_set_dim(isl_set_from_cloog_domain(data->outter_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set, ndim - 1, 1);
		res = isl_set_remove_dims(res, isl_dim_set, ndim - 1, 1);
	}

	return res;
}

void pesto::ParamTilingUtility::setScop(osl_scop_p scop) { this->scop = scop; }

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

void pesto::ParamTilingUtility::declareGroupVariables(struct StmtGroup &group) {
	for (auto &data : group.tilingData) {

		this->declareVariable(std::string(data->codegen->getPCMaxName()));

		this->declareVariable(std::string(data->codegen->getTargetVolName()));

		this->declareVariable(std::string(data->codegen->getOutterForUBName()));

		this->declareVariable(std::string(data->codegen->getInnerForLBName()));

		this->declareVariable(std::string(data->codegen->getInnerForUBName()));
	}
}

void pesto::ParamTilingUtility::updateOmpPragma(struct StmtGroup &group) {
	// first find the parallel for
	struct clast_for *parallel_for = NULL;
	int tiling_depth = -1;
	for (auto &data : group.tilingData) {
		struct clast_for *outter_for = data->outter_for;
		if (outter_for->parallel & CLAST_PARALLEL_OMP) {
			parallel_for = outter_for;
			tiling_depth = data->tiling_depth;
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

	// omp firstprivate directive
	userDirectiveStr += group.tilingData[0]->codegen->getOutterForUBName() + ", ";
	userDirectiveStr += group.tilingData[0]->codegen->getPCMaxName() + ", ";
	userDirectiveStr += group.tilingData[0]->codegen->getTargetVolName() + ") \\\n";

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
	privateVarsStr += group.tilingData[tiling_depth]->codegen->getInnerForLBName();
	privateVarsStr += ", " + group.tilingData[tiling_depth]->codegen->getInnerForUBName();

	for (std::size_t i = tiling_depth + 1; i < group.tilingData.size(); i += 1) {
		privateVarsStr += ", " + group.tilingData[i]->codegen->getOutterForUBName();
		privateVarsStr += ", " + group.tilingData[i]->codegen->getPCMaxName();
		privateVarsStr += ", " + group.tilingData[i]->codegen->getTargetVolName();
		privateVarsStr += ", " + group.tilingData[i]->codegen->getInnerForLBName();
		privateVarsStr += ", " + group.tilingData[i]->codegen->getInnerForUBName();
	}
	parallel_for->private_vars = strdup(privateVarsStr.c_str());
}

void pesto::ParamTilingUtility::computeTrahrheDomain(struct StmtGroup &group) {

	isl_set *domain = isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));

	// define param names
	for (int i = 0; i < this->cloogProgram->names->nb_parameters; i += 1) {
		domain = isl_set_set_dim_name(domain, isl_dim_param, i, this->cloogProgram->names->parameters[i]);
	}

	for (int i = 0; i < isl_set_n_dim(domain); i += 1) {
		domain = isl_set_set_dim_name(domain, isl_dim_set, i, this->cloogProgram->names->scattering[i]);
	}

	domain = this->moveNonTiledDimensionsToParameters(domain, group);
	;

	// apply the map to original domain
	isl_set *out = this->extractIslInnerTiledDomain(group, isl_set_copy(domain), group.tilingData.size());

	// rename the dimension
	for (std::size_t i = 0; i < group.tilingData.size(); i += 1) {
		auto &data = group.tilingData[i];
		out = isl_set_set_dim_name(out, isl_dim_set, i, data->codegen->getTrahrheIteratorName().c_str());
	}

	int ntiled_dim = 0;
	for (std::size_t i = 0; i < group.tilingData.size(); i += 1) {
		if (this->getTilingType(i) != ParamTilingType::RECTANGULAR) {
			ntiled_dim += 1;
		}
	}

	isl_printer *printer = isl_printer_to_str(ctx);

	printer = isl_printer_print_set(printer, out);

	char *str = isl_printer_get_str(printer);

	group.trahrheDomain.first = ntiled_dim;
	group.trahrheDomain.second = std::string(str);

	free(str);
	isl_printer_free(printer);

	isl_set_free(out);
	isl_set_free(domain);
}

std::vector<std::pair<int, std::string>> pesto::ParamTilingUtility::getTrahrheDomains() {
	std::vector<std::pair<int, std::string>> res;
	for (auto &group : this->stmtGroups) {
		res.push_back(group->trahrheDomain);
	}
	return res;
}

__isl_give isl_set *pesto::ParamTilingUtility::extractIslInnerTiledDomain(struct StmtGroup &group,
																		  __isl_take isl_set *domain,
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

		int in_idx = isl_map_find_dim_by_name(trans, isl_dim_in, data->inner_for->iterator);
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
																				  struct StmtGroup &group) {

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
			int in_idx = isl_set_find_dim_by_name(domain, isl_dim_set, data->inner_for->iterator);
			int out_idx = isl_set_find_dim_by_name(domain, isl_dim_set, data->outter_for->iterator);
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

__isl_give isl_set *pesto::ParamTilingUtility::getInnerDomain(struct StmtGroup &group,
															  std::unique_ptr<struct TilingData> &data) {
	isl_set *inner_domain = isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));
	// define param names
	for (int i = 0; i < this->cloogProgram->names->nb_parameters; i += 1) {
		inner_domain = isl_set_set_dim_name(inner_domain, isl_dim_param, i, this->cloogProgram->names->parameters[i]);
	}

	for (isl_size i = 0; i < isl_set_n_dim(inner_domain); i += 1) {
		inner_domain = isl_set_set_dim_name(inner_domain, isl_dim_set, i, this->cloogProgram->names->scattering[i]);
	}
	inner_domain = this->moveNonTiledDimensionsToParameters(inner_domain, group);
	inner_domain = this->extractIslInnerTiledDomain(group, inner_domain, data->tiling_depth + 1);

	return inner_domain;
}