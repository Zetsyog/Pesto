#include <algorithm>

#include <pesto/Pesto.hpp>

#include <isl/ctx.h>
#include <isl/set.h>

#include <cloog/isl/cloog.h>

#include "clast/clast_support.h"
#include "isl_support.h"

#define STR_BUF_SIZE 512

pesto::ParamTilingUtility::ParamTilingUtility()
	: scop(nullptr), root(nullptr), insertedStmts(0), error(false) {}

pesto::ParamTilingUtility::~ParamTilingUtility() {
	for (auto &group : this->stmtGroups) {
		for (auto &data : group->tilingData) {
			trahrhe_codegen_free(&data->codegen_data.first);
			trahrhe_codegen_free(&data->codegen_data.second);
		}
	}
}

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
	std::cerr << "[param-tiling] Found " << this->stmtGroups.size()
			  << " statements groups" << std::endl;

	if (this->stmtGroups.size() == 0) {
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
	struct clast_stmt **stmt =
		clast_get_user_stmt_list((struct clast_stmt *)this->root, &stmt_count);

	std::cerr << "[param-tiling] Found " << stmt_count << " statements in clast"
			  << std::endl;

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
		std::unique_ptr<struct StmtGroup> group =
			std::make_unique<struct StmtGroup>();

		// take the first statement that has not been processed
		group->stmts.push_back(stmt_to_process.back());
		// we mark it as processed
		stmt_to_process.pop_back();

		// get the parent for of the group
		struct clast_for *parent_for = clast_get_parent_clast_for(
			(struct clast_stmt *)root,
			(struct clast_stmt *)group->stmts.begin()[0]);
		group->parent_for = parent_for;
		group->id = this->stmtGroups.size();

		// for each statement that has not been processed yet
		for (std::vector<struct clast_stmt *>::iterator it =
				 stmt_to_process.begin();
			 it != stmt_to_process.end();) {
			struct clast_for *tmp = clast_get_parent_clast_for(
				(struct clast_stmt *)root, (struct clast_stmt *)*it);
			if (parent_for == tmp) {
				group->stmts.push_back(*it);
				it = stmt_to_process.erase(it);
			} else {
				++it;
			}
		}

		// append group to the list of groups
		this->stmtGroups.push_back(std::move(group));
	}
}

void pesto::ParamTilingUtility::tileGroup(StmtGroup &group) {
	isl_set *group_domain =
		isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));

	std::cerr << "group domain: " << std::endl;
	isl_set_dump(group_domain);

	int tiling_dim_count = isl_set_count_tiling_dimensions(group_domain);
	std::cerr << "tiled dimensions found: " << tiling_dim_count << std::endl;

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
				  << " is outter of: " << data->inner_for->iterator
				  << std::endl;
	}

	// Then we need to sort the tiling_for ascendingly of the depth of the
	// outter for

	// Then for each outter for (from outter most to inner most)
	for (auto &data : group.tilingData) {
		struct clast_for *outter_for = data->outter_for;
		struct clast_for *inner_for = data->inner_for;
		int tiling_depth = data->tiling_depth;

		// TODO: remove
		std::cerr << "Proccessing " << outter_for->iterator << std::endl;

		this->computeCodegenData(group, outter_for, inner_for, tiling_depth);

		this->insertStatements(group, outter_for, inner_for, tiling_depth);
		this->replaceOutterForBounds(group, outter_for, inner_for,
									 tiling_depth);
		this->replaceInnerForBounds(group, outter_for, inner_for, tiling_depth);

		tiling_depth += 1;
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

std::string pesto::ParamTilingUtility::getErrorMessage() {
	return this->errorMessage;
}

bool pesto::ParamTilingUtility::hasError() { return this->error; }

void pesto::ParamTilingUtility::setClastRoot(struct clast_root *root) {
	this->root = root;
}

void pesto::ParamTilingUtility::associateTilingFor(StmtGroup &group) {
	struct clast_for *current_for = group.parent_for;

	while (current_for != NULL) {
		// if the current for is an inner tiled for
		if (clast_for_is_inner_tile_loop(current_for)) {
			// we get the corresponding outter for
			struct clast_for *outter_for = clast_get_outter_tiling_for(
				(struct clast_stmt *)this->root, current_for);
			if (outter_for == NULL) {
				std::cerr << "[param-tiling] Warning: found a inner tiled for "
							 "with no outter tiling for"
						  << std::endl;
				current_for = clast_get_parent_clast_for(
					(struct clast_stmt *)this->root,
					(struct clast_stmt *)current_for);
				continue;
			}
			auto data = std::make_unique<struct TilingData>();
			data->outter_for = outter_for;
			data->inner_for = current_for;
			group.tilingData.push_back(std::move(data));
		}
		current_for = clast_get_parent_clast_for(
			(struct clast_stmt *)this->root, (struct clast_stmt *)current_for);
	}

	struct clast_root *root = this->root;
	// sort the tiling data by depth of the outter for
	std::sort(group.tilingData.begin(), group.tilingData.end(),
			  [root](const std::unique_ptr<struct TilingData> &a,
					 const std::unique_ptr<struct TilingData> &b) {
				  return isl_set_n_dim(
							 isl_set_from_cloog_domain(a->outter_for->domain)) <
						 isl_set_n_dim(
							 isl_set_from_cloog_domain(b->outter_for->domain));
			  });

	// update the tiling depth
	for (std::size_t i = 0; i < group.tilingData.size(); i++) {
		group.tilingData[i]->tiling_depth = i;
	}
}

void pesto::ParamTilingUtility::pushTilingType(ParamTilingType type) {
	this->tilingTypes.push_back(type);
}

pesto::ParamTilingType
pesto::ParamTilingUtility::getTilingType(std::size_t depth) {
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

void pesto::ParamTilingUtility::computeCodegenData(struct StmtGroup &group,
												   struct clast_for *outter_for,
												   struct clast_for *inner_for,
												   int tiling_depth) {
	ParamTilingType type = this->getTilingType(tiling_depth);
	ptile_type trahrhe_type;
	if (type == ParamTilingType::ALGEBRAIC) {
		trahrhe_type = trahrhe_type_algebraic;
	} else if (type == ParamTilingType::RECTANGULAR) {
		trahrhe_type = trahrhe_type_rectangular;
	} else {
		std::cerr << "[param-tiling] Warning: unknown tiling type" << std::endl;
		trahrhe_type = trahrhe_type_algebraic;
	}
	struct trahrhe_codegen_data header_data = {.stmt_type = trahrhe_stmt_head,
											   .tiling_type = trahrhe_type};
	struct trahrhe_codegen_data body_data = {.stmt_type = trahrhe_stmt_body,
											 .tiling_type = trahrhe_type};

	header_data.group_id = group.id;
	body_data.group_id = group.id;
	header_data.depth = tiling_depth;
	body_data.depth = tiling_depth;
	header_data.stmt_id = ++(this->insertedStmts);
	body_data.stmt_id = ++(this->insertedStmts);

	header_data.inner_it_name = strdup(inner_for->iterator);
	body_data.inner_it_name = strdup(inner_for->iterator);
	header_data.iterator_name = strdup(outter_for->iterator);
	body_data.iterator_name = strdup(outter_for->iterator);

	header_data.npar = this->cloogProgram->names->nb_parameters;
	body_data.npar = this->cloogProgram->names->nb_parameters;
	for (int i = 0; i < tiling_depth; i++) {
		header_data.npar += 2;
		body_data.npar += 2;
	}

	header_data.params = (char **)malloc(header_data.npar * sizeof(char *));
	body_data.params = (char **)malloc(body_data.npar * sizeof(char *));
	for (int i = 0; i < this->cloogProgram->names->nb_parameters; i++) {
		header_data.params[i] =
			strdup(this->cloogProgram->names->parameters[i]);
		body_data.params[i] = strdup(this->cloogProgram->names->parameters[i]);
	}
	for (int i = 0; i < tiling_depth; i++) {
		auto pair = group.tilingData[i]->codegen_data;
		header_data.params[2 * i + this->cloogProgram->names->nb_parameters] =
			strdup(pair.first.lb_name);
		header_data
			.params[2 * i + 1 + this->cloogProgram->names->nb_parameters] =
			strdup(pair.first.ub_name);
		body_data.params[2 * i + this->cloogProgram->names->nb_parameters] =
			strdup(pair.second.lb_name);
		body_data.params[2 * i + 1 + this->cloogProgram->names->nb_parameters] =
			strdup(pair.second.ub_name);
	}

	header_data.nest_id = this->id;
	body_data.nest_id = this->id;

	header_data.lb_name = (char *)malloc(STR_BUF_SIZE * sizeof(char));
	body_data.lb_name = (char *)malloc(STR_BUF_SIZE * sizeof(char));
	TRAHRHE_GEN_INNER_LB(sprintf, header_data.lb_name, &header_data);
	TRAHRHE_GEN_INNER_LB(sprintf, body_data.lb_name, &body_data);

	header_data.ub_name = (char *)malloc(STR_BUF_SIZE * sizeof(char));
	body_data.ub_name = (char *)malloc(STR_BUF_SIZE * sizeof(char));
	TRAHRHE_GEN_INNER_UB(sprintf, header_data.ub_name, &header_data);
	TRAHRHE_GEN_INNER_UB(sprintf, body_data.ub_name, &body_data);

	// TODO: move in separate function
	char *lb_expr = (char *)malloc(STR_BUF_SIZE * sizeof(char));
	char *ub_expr = (char *)malloc(STR_BUF_SIZE * sizeof(char));

	memset(lb_expr, 0, STR_BUF_SIZE);
	memset(ub_expr, 0, STR_BUF_SIZE);

	int firstTilingDim = this->islFirstTilingDimIdx(group);

	CloogNames *ub_names = cloog_names_hard_copy(this->cloogProgram->names);
	for (int i = firstTilingDim; i < ub_names->nb_scattering; i += 1) {
		char *it_name = ub_names->scattering[i];
		ub_names->scattering[i] = (char *)malloc(32 * sizeof(char));
		snprintf(ub_names->scattering[i], 32, "g%d_ub%s", header_data.group_id,
				 it_name);
		free(it_name);
	}

	CloogNames *lb_names = cloog_names_hard_copy(this->cloogProgram->names);
	for (int i = firstTilingDim; i < lb_names->nb_scattering; i += 1) {
		char *it_name = lb_names->scattering[i];
		lb_names->scattering[i] = (char *)malloc(32 * sizeof(char));
		snprintf(lb_names->scattering[i], 32, "g%d_lb%s", header_data.group_id,
				 it_name);
		TRAHRHE_GEN_INNER_LB(sprintf, lb_expr, &header_data);
		free(it_name);
	}
	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	domain = this->dropTilingConstraints(domain, group);

	for (int bound = 0; bound < 2; bound += 1) {
		struct clast_expr *expr = clast_bound_from_isl_set(
			bound == 1 ? ub_names : lb_names, domain, bound);
		FILE *fd = fmemopen(bound == 0 ? lb_expr : ub_expr, 512, "w");
		clast_pprint_expr(this->cloogOptions, fd, expr);
		fclose(fd);

		free_clast_expr(expr);

		header_data.lb_expr = strdup(lb_expr);
		header_data.ub_expr = strdup(ub_expr);
		body_data.lb_expr = strdup(lb_expr);
		body_data.ub_expr = strdup(ub_expr);
	}

	isl_set_free(domain);

	cloog_names_free(ub_names);
	cloog_names_free(lb_names);

	free(lb_expr);
	free(ub_expr);

	group.tilingData[tiling_depth]->codegen_data = {header_data, body_data};
}

int pesto::ParamTilingUtility::islFirstTilingDimIdx(struct StmtGroup &group) {
	int firstTilingDim = 1000;
	for (auto &data : group.tilingData) {
		int dim =
			isl_set_n_dim(isl_set_from_cloog_domain(data->outter_for->domain));
		firstTilingDim = dim < firstTilingDim ? dim : firstTilingDim;
	}
	return firstTilingDim - 1;
}

void pesto::ParamTilingUtility::setCloogProgram(CloogProgram *program) {
	this->cloogProgram = program;
}

void pesto::ParamTilingUtility::setCloogOptions(CloogOptions *options) {
	this->cloogOptions = options;
}

void pesto::ParamTilingUtility::insertStatements(struct StmtGroup &group,
												 struct clast_for *outter_for,
												 struct clast_for *inner_for,
												 int tiling_depth) {
	auto pair = group.tilingData[tiling_depth]->codegen_data;

	struct clast_user_stmt *header_stmt =
		pesto_clast_create_stmt(this->cloogOptions, NULL, pair.first.stmt_id);
	clast_insert_before((struct clast_stmt *)this->root,
						(struct clast_stmt *)outter_for,
						(struct clast_stmt *)header_stmt);

	struct clast_user_stmt *body_stmt =
		pesto_clast_create_stmt(this->cloogOptions, NULL, pair.second.stmt_id);
	clast_insert_indepth((struct clast_stmt *)outter_for,
						 (struct clast_stmt *)body_stmt);
}

void pesto::ParamTilingUtility::replaceInnerForBounds(
	struct StmtGroup &group, struct clast_for *outter_for,
	struct clast_for *inner_for, int tiling_depth) {

	free_clast_expr(inner_for->LB);
	free_clast_expr(inner_for->UB);
	inner_for->LB = NULL;
	inner_for->UB = NULL;

	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	domain = this->dropTilingConstraints(domain, group);

	inner_for->LB =
		clast_bound_from_isl_set(this->cloogProgram->names, domain, 0);
	inner_for->UB =
		clast_bound_from_isl_set(this->cloogProgram->names, domain, 1);

	isl_set_free(domain);

	char *buf = (char *)malloc(STR_BUF_SIZE * sizeof(char));

	TRAHRHE_GEN_INNER_LB(sprintf, buf,
						 &group.tilingData[tiling_depth]->codegen_data.second);
	auto lb_ptr = std::make_shared<std::string>(buf);
	struct clast_name *param_lb = new_clast_name(lb_ptr->c_str());
	this->names.push_back(lb_ptr);

	TRAHRHE_GEN_INNER_UB(sprintf, buf,
						 &group.tilingData[tiling_depth]->codegen_data.second);
	auto ub_ptr = std::make_shared<std::string>(buf);
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

	free(buf);
}

void pesto::ParamTilingUtility::replaceOutterForBounds(
	struct StmtGroup &group, struct clast_for *outter_for,
	struct clast_for *inner_for, int tiling_depth) {
	free_clast_expr(outter_for->LB);
	free_clast_expr(outter_for->UB);
	outter_for->LB = NULL;
	outter_for->UB = NULL;

	cloog_int_t zero;
	cloog_int_init(zero);
	outter_for->LB = (struct clast_expr *)new_clast_term(zero, NULL);

	char *buffer = (char *)malloc(STR_BUF_SIZE * sizeof(char));

	TRAHRHE_GEN_OUTTER_UB(sprintf, buffer,
						  &group.tilingData[tiling_depth]->codegen_data.first);
	auto ub_ptr = std::make_shared<std::string>(buffer);
	outter_for->UB = (struct clast_expr *)new_clast_name(ub_ptr->c_str());
	this->names.push_back(ub_ptr);

	free(buffer);
}

void pesto::ParamTilingUtility::generateStatements(struct StmtGroup &group) {
	char *buffer = (char *)malloc(STR_BUF_SIZE * sizeof(char));

	for (auto &data : group.tilingData) {
		FILE *buf_fd = fmemopen(buffer, STR_BUF_SIZE, "w");
		auto pair = data->codegen_data;

		for (int i = 0; i < 2; i += 1) {
			struct trahrhe_codegen_data *data =
				i == 0 ? &pair.first : &pair.second;
			rewind(buf_fd);
			trahrhe_codegen_stmt(buf_fd, data);
			fflush(buf_fd);
			this->statements.push_back(
				std::make_pair(data->stmt_id, std::string(buffer)));
		}

		fclose(buf_fd);
	}

	free(buffer);
}

isl_set *
pesto::ParamTilingUtility::dropTilingConstraints(__isl_take isl_set *set,
												 struct StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (std::vector<std::unique_ptr<struct TilingData>>::reverse_iterator
			 riter = group.tilingData.rbegin();
		 riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

		int ndim = isl_set_dim(
			isl_set_from_cloog_domain(data->outter_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set,
													  ndim - 1, 1);
	}

	return res;
}

isl_set *
pesto::ParamTilingUtility::removeTilingDimensions(__isl_take isl_set *set,
												  struct StmtGroup &group) {
	isl_set *res = isl_set_copy(set);
	isl_set_free(set);

	// we need to drop the constraints that are not related to the tiling
	// dimensions
	// but from the inner most to the outter most
	// so we sort the tiling_for pair by descending depth of the outter for
	for (std::vector<std::unique_ptr<struct TilingData>>::reverse_iterator
			 riter = group.tilingData.rbegin();
		 riter != group.tilingData.rend(); ++riter) {
		auto &data = *riter;

		int ndim = isl_set_dim(
			isl_set_from_cloog_domain(data->outter_for->domain), isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set,
													  ndim - 1, 1);
		res = isl_set_remove_dims(res, isl_dim_set, ndim - 1, 1);
	}

	return res;
}

void pesto::ParamTilingUtility::setScop(osl_scop_p scop) { this->scop = scop; }

std::vector<std::shared_ptr<std::string>>
pesto::ParamTilingUtility::getNames() {
	return this->names;
}

std::vector<std::pair<int, std::string>>
pesto::ParamTilingUtility::getStatements() {
	return this->statements;
}

void pesto::ParamTilingUtility::setIdTrahrhePrefix(int id) { this->id = id; }

std::vector<std::string> pesto::ParamTilingUtility::getVarList() {
	return this->varList;
}

void pesto::ParamTilingUtility::declareVariable(std::string var) {
	// we do not want to declare the same variable twice
	if (std::find(this->varList.begin(), this->varList.end(), var) !=
		this->varList.end()) {
		return;
	}
	this->varList.push_back(var);
}

void pesto::ParamTilingUtility::declareGroupVariables(struct StmtGroup &group) {
	char *buffer = (char *)malloc(STR_BUF_SIZE * sizeof(char));
	for (auto &data : group.tilingData) {
		auto pair = data->codegen_data;

		TRAHRHE_GEN_PCMAX(sprintf, buffer, &pair.first);
		this->declareVariable(std::string(buffer));

		TRAHRHE_GEN_TARGET_VOL(sprintf, buffer, &pair.first);
		this->declareVariable(std::string(buffer));

		TRAHRHE_GEN_OUTTER_UB(sprintf, buffer, &pair.first);
		this->declareVariable(std::string(buffer));

		TRAHRHE_GEN_INNER_LB(sprintf, buffer, &pair.second);
		this->declareVariable(std::string(buffer));

		TRAHRHE_GEN_INNER_UB(sprintf, buffer, &pair.second);
		this->declareVariable(std::string(buffer));
	}
	free(buffer);
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
	char *buffer = (char *)malloc(STR_BUF_SIZE * sizeof(char));

	// omp firstprivate directive
	sprintf(buffer, "omp parallel for firstprivate(");
	TRAHRHE_GEN_OUTTER_UB(sprintf, buffer + strlen(buffer),
						  &group.tilingData[0]->codegen_data.first);
	sprintf(buffer + strlen(buffer), ", ");
	TRAHRHE_GEN_PCMAX(sprintf, buffer + strlen(buffer),
					  &group.tilingData[0]->codegen_data.first);
	sprintf(buffer + strlen(buffer), ", ");
	TRAHRHE_GEN_TARGET_VOL(sprintf, buffer + strlen(buffer),
						   &group.tilingData[0]->codegen_data.first);
	sprintf(buffer + strlen(buffer), ") \\\n");

	parallel_for->user_directive = strdup(buffer);

	memset(buffer, 0, STR_BUF_SIZE);
	// private variables

	// we keep already existing private variables
	sprintf(buffer, "%s,", parallel_for->private_vars);
	// TODO: tile scheduling

	// now we can free old private vars
	free(parallel_for->private_vars);
	parallel_for->private_vars = NULL;

	TRAHRHE_GEN_INNER_LB(sprintf, buffer + strlen(buffer),
						 &group.tilingData[tiling_depth]->codegen_data.second);
	sprintf(buffer + strlen(buffer), ", ");
	TRAHRHE_GEN_INNER_UB(sprintf, buffer + strlen(buffer),
						 &group.tilingData[tiling_depth]->codegen_data.second);

	for (std::size_t i = tiling_depth + 1; i < group.tilingData.size();
		 i += 1) {
		sprintf(buffer + strlen(buffer), ", ");
		TRAHRHE_GEN_OUTTER_UB(sprintf, buffer + strlen(buffer),
							  &group.tilingData[i]->codegen_data.first);
		sprintf(buffer + strlen(buffer), ", ");
		TRAHRHE_GEN_PCMAX(sprintf, buffer + strlen(buffer),
						  &group.tilingData[i]->codegen_data.first);
		sprintf(buffer + strlen(buffer), ", ");
		TRAHRHE_GEN_TARGET_VOL(sprintf, buffer + strlen(buffer),
							   &group.tilingData[i]->codegen_data.first);
		sprintf(buffer + strlen(buffer), ", ");
		TRAHRHE_GEN_INNER_LB(sprintf, buffer + strlen(buffer),
							 &group.tilingData[i]->codegen_data.second);
		sprintf(buffer + strlen(buffer), ", ");
		TRAHRHE_GEN_INNER_UB(sprintf, buffer + strlen(buffer),
							 &group.tilingData[i]->codegen_data.second);
	}
	parallel_for->private_vars = strdup(buffer);

	free(buffer);
}

void pesto::ParamTilingUtility::computeTrahrheDomain(struct StmtGroup &group) {

	isl_ctx *ctx = isl_ctx_alloc();

	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(group.parent_for->domain));

	// define param names
	for (int i = 0; i < this->cloogProgram->names->nb_parameters; i += 1) {
		domain = isl_set_set_dim_name(domain, isl_dim_param, i,
									  this->cloogProgram->names->parameters[i]);
	}

	for (int i = 0; i < isl_set_n_dim(domain); i += 1) {
		domain = isl_set_set_dim_name(domain, isl_dim_set, i,
									  this->cloogProgram->names->scattering[i]);
	}
	isl_set_dump(domain);

	// we are going to build a map
	// that will transform the current dimensions to keep only the tiled
	// dimensions in the order of the tiling iterators
	isl_space *space = isl_set_get_space(domain);
	isl_map *trans = isl_map_universe(isl_space_copy(space));
	isl_space_free(space);

	// we move all dimensions to input dimensions
	trans = isl_map_move_dims(trans, isl_dim_in, 0, isl_dim_set, 0,
							  isl_map_dim(trans, isl_dim_set));
	// we have as much output dimensions as tiled dimensions
	// TODO: move untiled dimensions as parameters
	trans = isl_map_add_dims(trans, isl_dim_out, group.tilingData.size());
	isl_map_dump(trans);

	char *buffer = (char *)malloc(STR_BUF_SIZE * sizeof(char));

	// for each tiled dimensionss
	for (std::size_t i = 0; i < group.tilingData.size(); i += 1) {
		auto &data = group.tilingData[i];

		// we the dimension
		TRAHRHE_GEN_TRAHRHE_ITERATOR(sprintf, buffer,
									 &data->codegen_data.first);
		trans = isl_map_set_dim_name(trans, isl_dim_out, i, buffer);

		// we add a constraint [input_dim] = [output_dim]
		// where [input_dim] is the dimension in the original set
		// and [output_dim] is the corresponding dimension in the new space
		isl_constraint *cst = isl_constraint_alloc_equality(
			isl_local_space_from_space(isl_map_get_space(trans)));

		int in_idx = isl_map_find_dim_by_name(trans, isl_dim_in,
											  data->inner_for->iterator);
		cst = isl_constraint_set_coefficient_si(cst, isl_dim_in, in_idx, -1);
		cst = isl_constraint_set_coefficient_si(cst, isl_dim_out, i, 1);
		trans = isl_map_add_constraint(trans, cst);
	}
	free(buffer);

	// apply the map to original domain
	isl_set *out = isl_set_apply(isl_set_copy(domain), isl_map_copy(trans));

	isl_map_free(trans);

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

	isl_ctx_free(ctx);
}

std::vector<std::pair<int, std::string>>
pesto::ParamTilingUtility::getTrahrheDomains() {
	std::vector<std::pair<int, std::string>> res;
	for (auto &group : this->stmtGroups) {
		res.push_back(group->trahrheDomain);
	}
	return res;
}