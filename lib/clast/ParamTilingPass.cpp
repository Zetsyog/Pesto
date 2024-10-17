#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <cloog/isl/cloog.h>

#include <isl/val.h>

#include <pesto/Pesto.hpp>

#include "clast/clast_support.h"

#include "isl_support.h"

using pesto::pass::metadata::ClastHolder;
using pesto::pass::metadata::MacroMetadata;
using pesto::pass::metadata::VarDeclMetadata;

pesto::pass::ClastParamTilingPass::ClastParamTilingPass()
	: ISinglePass("pass-param-tiling"), gen_trahrhe(false), is_stencil(false),
	  tiling_types() {
	using namespace pesto::pass;

	this->pluto_pass = std::make_unique<PlutoPass>();
	this->tiling_pass = std::make_unique<PlutoPass>();
	this->gen_clast_pass = std::make_unique<GenClastPass>();
	this->gen_clast_pass->codegenContext = 10;

	this->tiling_pass->getOptions().parallel = 1;
	this->tiling_pass->getOptions().tile = 1;
	this->tiling_pass->getOptions().intratileopt = 0;
}

pesto::pass::ClastParamTilingPass::~ClastParamTilingPass() {}

void pesto::pass::ClastParamTilingPass::run(pesto::Program &prog) {
	// run the pass on each nest
	for (auto &nest : prog.getScopNests()) {
		runSingleNest(*nest);
	}
}

void pesto::pass::ClastParamTilingPass::replace_outter_tiled_for_bounds(
	ClastHolder &metadata, struct tiling_info &tiling_info) {
	free_clast_expr(tiling_info.outter_for->LB);
	free_clast_expr(tiling_info.outter_for->UB);

	cloog_int_t zero;
	cloog_int_init(zero);
	tiling_info.outter_for->LB =
		(struct clast_expr *)new_clast_term(zero, NULL);

	char buffer[256] = {0};
	FILE *buf_fd = fmemopen(buffer, 256, "w");
	TRAHRHE_GEN_OUTTER_UB(fprintf, buf_fd, &tiling_info.gen_data[0]);
	fclose(buf_fd);
	auto ub_var = std::make_shared<std::string>(buffer);
	tiling_info.outter_for->UB =
		(struct clast_expr *)new_clast_name(ub_var->c_str());

	metadata.names.push_back(ub_var);
}

void pesto::pass::ClastParamTilingPass::replace_inner_tiled_for_bounds(
	ClastHolder &metadata, struct clast_stmt *root,
	struct tiling_info &tiling_info,
	std::vector<struct tiling_info> &tiling_info_list) {
	struct clast_for *outter_for = tiling_info.outter_for;
	struct clast_for *inner_for = tiling_info.inner_for;

	free_clast_expr(inner_for->LB);
	free_clast_expr(inner_for->UB);

	std::cerr << "replace inner bounds of " << inner_for->iterator << std::endl;

	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(inner_for->domain));
	domain = this->dropTilingConstraints(domain, tiling_info_list);

	inner_for->LB =
		clast_bound_from_isl_set(metadata.cloogProgram->names, domain, 0);
	inner_for->UB =
		clast_bound_from_isl_set(metadata.cloogProgram->names, domain, 1);

	isl_set_free(domain);

	char buffer[256] = {0};
	FILE *buf_fd = fmemopen(buffer, 256, "w");

	TRAHRHE_GEN_INNER_LB(fprintf, buf_fd, &tiling_info.gen_data[1]);
	fflush(buf_fd);
	auto lb_var = std::make_shared<std::string>(buffer);
	struct clast_name *param_lb = new_clast_name(lb_var->c_str());
	metadata.names.push_back(lb_var);
	rewind(buf_fd);

	TRAHRHE_GEN_INNER_UB(fprintf, buf_fd, &tiling_info.gen_data[1]);
	fclose(buf_fd);
	auto ub_var = std::make_shared<std::string>(buffer);
	struct clast_name *param_ub = new_clast_name(ub_var->c_str());
	metadata.names.push_back(ub_var);

	struct clast_reduction *red;

	if (inner_for->LB != NULL) {
		red = new_clast_reduction(clast_red_max, 2);
		red->elts[0] = inner_for->LB;
		red->elts[1] = (struct clast_expr *)param_lb;
		inner_for->LB = (struct clast_expr *)red;
	} else {
		inner_for->LB = (struct clast_expr *)param_lb;
	}

	if (inner_for->UB != NULL) {
		red = new_clast_reduction(clast_red_min, 2);
		red->elts[0] = inner_for->UB;
		red->elts[1] = (struct clast_expr *)param_ub;
		inner_for->UB = (struct clast_expr *)red;
	} else {
		inner_for->UB = (struct clast_expr *)param_ub;
	}
}

void pesto::pass::ClastParamTilingPass::insert_tiling_statements(
	OslScop &nest, struct clast_stmt *root, struct tiling_info &tiling_info,
	std::vector<struct tiling_info> &tiling_info_list) {
	auto metadata = nest.metadata.get<ClastHolder>().lock();

	struct clast_user_stmt *header_stmt = pesto_clast_create_stmt(
		metadata->cloogOptions, NULL, ++metadata->stmtsNb);
	clast_insert_before(root, &tiling_info.outter_for->stmt,
						&header_stmt->stmt);
	tiling_info.gen_data[0].stmt_id = metadata->stmtsNb;
	tiling_info.gen_data[0].stmt_type = trahrhe_stmt_head;

	struct clast_user_stmt *body_stmt = pesto_clast_create_stmt(
		metadata->cloogOptions, NULL, ++metadata->stmtsNb);
	clast_insert_indepth(&tiling_info.outter_for->stmt, &body_stmt->stmt);
	tiling_info.gen_data[1].stmt_id = metadata->stmtsNb;
	tiling_info.gen_data[1].stmt_type = trahrhe_stmt_body;

	char *lb_expr = (char *)malloc(512 * sizeof(char));
	char *ub_expr = (char *)malloc(512 * sizeof(char));

	memset(lb_expr, 0, 512);
	memset(ub_expr, 0, 512);

	// TODO: do not hard code the bound variable names
	CloogNames *ub_names = cloog_names_hard_copy(metadata->cloogProgram->names);
	for (int i = 0; i < ub_names->nb_scattering; i += 1) {
		char *it_name = ub_names->scattering[i];
		ub_names->scattering[i] = (char *)malloc(32 * sizeof(char));
		snprintf(ub_names->scattering[i], 32, "g%d_ub%s",
				 tiling_info.gen_data[0].group_id, it_name);
		free(it_name);
	}

	CloogNames *lb_names = cloog_names_hard_copy(metadata->cloogProgram->names);
	for (int i = 0; i < lb_names->nb_scattering; i += 1) {
		char *it_name = lb_names->scattering[i];
		lb_names->scattering[i] = (char *)malloc(32 * sizeof(char));
		snprintf(lb_names->scattering[i], 32, "g%d_lb%s",
				 tiling_info.gen_data[0].group_id, it_name);
		free(it_name);
	}
	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(tiling_info.inner_for->domain));
	domain = this->dropTilingConstraints(domain, tiling_info_list);

	for (int bound = 0; bound < 2; bound += 1) {
		struct clast_expr *expr = clast_bound_from_isl_set(
			bound == 1 ? ub_names : lb_names, domain, bound);
		FILE *fd = fmemopen(bound == 0 ? lb_expr : ub_expr, 512, "w");
		clast_pprint_expr(metadata->cloogOptions, fd, expr);
		fclose(fd);

		free_clast_expr(expr);

		for (int i = 0; i < 2; i += 1) {
			if (bound == 0) {
				tiling_info.gen_data[i].lb_expr = strdup(lb_expr);
			} else {
				tiling_info.gen_data[i].ub_expr = strdup(ub_expr);
			}
		}
	}

	isl_set_free(domain);

	cloog_names_free(ub_names);
	cloog_names_free(lb_names);
	free(lb_expr);
	free(ub_expr);

	for (int i = 0; i < 2; i += 1) {
		tiling_info.gen_data[i].tiling_type = trahrhe_type_algebraic;

		tiling_info.gen_data[i].inner_it_name =
			strdup(tiling_info.inner_for->iterator);
		tiling_info.gen_data[i].endline = strdup("\\\n");
		tiling_info.gen_data[i].iterator_name =
			strdup(tiling_info.outter_for->iterator);
		tiling_info.gen_data[i].npar =
			metadata->cloogProgram->names->nb_parameters;
		tiling_info.gen_data[i].params =
			(char **)malloc(tiling_info.gen_data[0].npar * sizeof(char *));
		for (int j = 0; j < tiling_info.gen_data[0].npar; j++) {
			tiling_info.gen_data[i].params[j] =
				strdup(metadata->cloogProgram->names->parameters[j]);
		}
		tiling_info.gen_data[i].nest_id = nest.getId();

		// generate variable bound name
		tiling_info.gen_data[i].lb_name = (char *)malloc(256 * sizeof(char));
		tiling_info.gen_data[i].ub_name = (char *)malloc(256 * sizeof(char));
		FILE *fd = fmemopen(tiling_info.gen_data[i].lb_name, 256, "w");
		TRAHRHE_GEN_INNER_LB(fprintf, fd, &tiling_info.gen_data[i]);
		fclose(fd);
		fd = fmemopen(tiling_info.gen_data[i].ub_name, 256, "w");
		TRAHRHE_GEN_INNER_UB(fprintf, fd, &tiling_info.gen_data[i]);
		fclose(fd);
	}
}
void pesto::pass::ClastParamTilingPass::addVarDecls(
	OslScop &nest, struct tiling_info &tiling_info) {
	auto varDeclData = nest.metadata.getOrCreate<VarDeclMetadata>().lock();
	char buffer[256] = {0};
	FILE *buf_fd = fmemopen(buffer, 256, "w");
	TRAHRHE_GEN_INNER_LB(fprintf, buf_fd, &tiling_info.gen_data[1]);
	fflush(buf_fd);
	varDeclData->addVar(buffer);
	rewind(buf_fd);
	TRAHRHE_GEN_INNER_UB(fprintf, buf_fd, &tiling_info.gen_data[1]);
	fflush(buf_fd);
	varDeclData->addVar(buffer);
	rewind(buf_fd);
	TRAHRHE_GEN_OUTTER_UB(fprintf, buf_fd, &tiling_info.gen_data[0]);
	fflush(buf_fd);
	varDeclData->addVar(buffer);
	rewind(buf_fd);
	TRAHRHE_GEN_PCMAX(fprintf, buf_fd, &tiling_info.gen_data[0]);
	fflush(buf_fd);
	varDeclData->addVar(buffer);
	rewind(buf_fd);
	TRAHRHE_GEN_TARGET_VOL(fprintf, buf_fd, &tiling_info.gen_data[0]);
	fflush(buf_fd);
	varDeclData->addVar(buffer);
	fclose(buf_fd);
}

void pesto::pass::ClastParamTilingPass::generate_stmt_macros(
	OslScop &nest, struct tiling_info &tiling_info) {
	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>().lock();

	char buffer[512] = {0};
	for (int i = 0; i < 2; i += 1) {
		FILE *buffer_fp = fmemopen(buffer, 512, "w");
		trahrhe_codegen_stmt(buffer_fp, &tiling_info.gen_data[i]);
		fclose(buffer_fp);
		std::string macro = "#define S" +
							std::to_string(tiling_info.gen_data[i].stmt_id) +
							"() " + buffer;

		macrosData->list.push_back(macro);
	}
}

void pesto::pass::ClastParamTilingPass::compute_trahrhe_isl_domain(
	OslScop &nest, struct StmtGroup *group,
	std::vector<tiling_info> &tiling_info_list) {

	if (!nest.metadata.get<ClastHolder>().lock()) {
		std::cerr << "[warning] Scop does not have ClastHolder. Stopped "
					 "trahrhe gen"
				  << std::endl;
		return;
	}
	auto metadata = nest.metadata.get<ClastHolder>().lock();
	// auto scopData = nest.metadata.get<OpenScopMetadata>().value();

	isl_ctx *ctx = isl_ctx_alloc();

	// compute isl domain to feed trahrhe software with
	// isl_set *domain =
	// 	isl_set_from_osl_stmt(ctx, scopData->scops["original"]->statement);

	// TODO
	for (auto &stmt : group->stmts) {
		if (CLAST_STMT_IS_A(stmt, stmt_user)) {
			struct clast_user_stmt *user_stmt = (struct clast_user_stmt *)stmt;
			std::cerr << "nb stmt = " << user_stmt->statement->number
					  << std::endl;
			isl_set *tmp =
				isl_set_from_osl_stmt(ctx, nest.getScop()->statement);
			isl_set_dump(tmp);
			isl_set_free(tmp);
		}
	}

	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(group->parent_for->domain));

	for (int i = 0; i < metadata->cloogProgram->names->nb_parameters; i += 1) {
		domain =
			isl_set_set_dim_name(domain, isl_dim_param, i,
								 metadata->cloogProgram->names->parameters[i]);
	}

	for (int i = 0; i < metadata->cloogProgram->names->nb_scattering; i += 1) {
		domain =
			isl_set_set_dim_name(domain, isl_dim_set, i,
								 metadata->cloogProgram->names->scattering[i]);
	}

	for (int i = 0; i < tiling_info_list.size(); i += 1) {
		auto &tiling_info = tiling_info_list[i];

		char buffer[256] = {0};
		snprintf(buffer, 256, "n%d_g%d_%s", nest.getId(), group->id,
				 tiling_info.inner_for->iterator);
		int depth = isl_set_n_dim(
			isl_set_from_cloog_domain(tiling_info.inner_for->domain));
		domain = isl_set_set_dim_name(domain, isl_dim_set, depth - 1, buffer);
	}

	domain = this->removeTilingDimensions(domain, tiling_info_list);

	int ndim = isl_set_dim(domain, isl_dim_set);

	for (int i = 0; i < ndim; i += 1) {
		domain = isl_set_lower_bound_val(domain, isl_dim_set, i,
										 isl_val_zero(isl_set_get_ctx(domain)));
	}

	int ntiled_dim = 0;
	for (int i = 0; i < ndim; i += 1) {
		if (this->getTilingType(i) != ParamTilingType::RECTANGULAR) {
			ntiled_dim += 1;
		}
	}

	std::cerr << "TEST DUMP" << std::endl;
	isl_set_dump(domain);
	std::cerr << std::endl << "END DUMP" << std::endl;

	FILE *outfd = fopen(".exec-once", "a");
	fprintf(outfd, "trahrhe_wrapper trahrhe_header.n%d.g%d.h ", nest.getId(),
			group->id);
	fprintf(outfd, " -d\"");

	isl_printer *printer = isl_printer_to_file(isl_set_get_ctx(domain), outfd);
	isl_printer_print_set(printer, domain);
	isl_printer_free(printer);

	fprintf(outfd, "\"");
	fprintf(outfd, " -e -t%d -m\"d\" \n", ntiled_dim);

	fclose(outfd);

	isl_set_free(domain);

	isl_ctx_free(ctx);
}

void pesto::pass::ClastParamTilingPass::applyTiling(OslScop &nest,
													struct clast_stmt *root,
													struct StmtGroup *group) {
	auto metadata = nest.metadata.get<ClastHolder>().lock();
	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>().lock();

	// dump the group
	std::cerr << "Group size: " << group->stmts.size() << std::endl;
	for (struct clast_stmt *stmt : group->stmts) {
		clast_pprint(stderr, stmt, 0, metadata->cloogOptions);
	}

	isl_set *domain = isl_set_from_cloog_domain(group->parent_for->domain);
	std::cerr << "Group domain: " << std::endl;
	isl_set_dump(domain);

	struct clast_for *parent_for = group->parent_for;

	std::vector<tiling_info> tiling_info_list;

	int current_tiling_depth = isl_set_count_tiling_dimensions(domain);
	std::cerr << "ntiled dim " << current_tiling_depth << std::endl;

	// first we need to find all inner tile loops
	// and associate them with their outter loop
	while (parent_for != NULL) {
		if (clast_for_is_inner_tile_loop(parent_for)) {
			std::cerr << parent_for->iterator << " is inner tile loop ";

			struct clast_for *outter_for = clast_get_outter_tiling_for(
				(struct clast_stmt *)root, parent_for);
			if (outter_for == NULL) {
				parent_for = clast_get_parent_clast_for(
					(struct clast_stmt *)root, (struct clast_stmt *)parent_for);
				continue;
			}
			std::cerr << " Outter loop: ";
			std::cerr << outter_for->iterator << std::endl;
			std::cerr << "Outter: "
					  << clast_for_is_outter_tile_loop(domain, outter_for)
					  << std::endl;
			std::cerr << "Depth: "
					  << clast_for_get_outter_tile_depth(domain, root,
														 outter_for)
					  << std::endl;
			struct tiling_info tiling_info = {0};
			tiling_info.depth =
				clast_for_get_outter_tile_depth(domain, root, outter_for);
			tiling_info.inner_for = parent_for;
			tiling_info.outter_for = outter_for;

			tiling_info_list.push_back(tiling_info);
			current_tiling_depth -= 1;
		}
		parent_for = clast_get_parent_clast_for(
			(struct clast_stmt *)root, (struct clast_stmt *)parent_for);
	}

	// sort the tiling info by depth
	std::sort(tiling_info_list.begin(), tiling_info_list.end(),
			  [](tiling_info &a, tiling_info &b) { return a.depth < b.depth; });

	for (int i = 0; i < tiling_info_list.size(); i++) {
		auto &tiling_info = tiling_info_list[i];

		ParamTilingType type = this->getTilingType(tiling_info.depth);

		for (int i = 0; i < 2; i += 1) {
			tiling_info.gen_data[i].group_id = group->id;
			tiling_info.gen_data[i].depth = tiling_info.depth;
		}
		insert_tiling_statements(nest, root, tiling_info, tiling_info_list);
		// is_stencil = true;
		if (type == ParamTilingType::ALGEBRAIC) {
			tiling_info.gen_data[0].tiling_type = trahrhe_type_algebraic;
			tiling_info.gen_data[1].tiling_type = trahrhe_type_algebraic;
		}
		if (type == ParamTilingType::RECTANGULAR) {
			tiling_info.gen_data[0].tiling_type = trahrhe_type_rectangular;
			tiling_info.gen_data[1].tiling_type = trahrhe_type_rectangular;
		}

		for (int j = 0; j < i; j += 1) {
			for (int k = 0; k < 2; k += 1) {
				tiling_info.gen_data[k].npar += 2;
				tiling_info.gen_data[k].params = (char **)realloc(
					tiling_info.gen_data[k].params,
					tiling_info.gen_data[k].npar * sizeof(char *));
				tiling_info.gen_data[k]
					.params[tiling_info.gen_data[k].npar - 2] =
					strdup(tiling_info_list[j].gen_data[0].lb_name);
				tiling_info.gen_data[k]
					.params[tiling_info.gen_data[k].npar - 1] =
					strdup(tiling_info_list[j].gen_data[0].ub_name);
			}
		}
		replace_inner_tiled_for_bounds(*metadata, root, tiling_info,
									   tiling_info_list);
		replace_outter_tiled_for_bounds(*metadata, tiling_info);
		addVarDecls(nest, tiling_info);

		// algebraic scheduling is a special case
		if (type == ParamTilingType::SCHEDULED) {
			log(std::cerr) << "SCHEDULING" << std::endl;
			if (i == 0) {
				continue;
			}
			auto &prev_tiling_info = tiling_info_list[i - 1];
			ParamTilingType prev_type =
				this->getTilingType(prev_tiling_info.depth);
			if (prev_type != ParamTilingType::SCHEDULED) {

				log(std::cerr) << "Error: Algebraic scheduling must have two "
								  "consecutive scheduled loops"
							   << std::endl;
				exit(1);
			}

			this->algebraicScheduleCodegen(nest, tiling_info, prev_tiling_info);
			this->algebraicScheduleHeaderGen(nest, tiling_info,
											 prev_tiling_info);

		} else {
			generate_stmt_macros(nest, tiling_info);
		}
	}

	// parallel loop
	// TODO: wrap this in a function
	for (int i = 0; i < tiling_info_list.size(); i++) {
		auto &tiling_info = tiling_info_list[i];
		if (i + 1 < tiling_info_list.size() &&
			getTilingType(tiling_info.depth) == ParamTilingType::SCHEDULED &&
			getTilingType(tiling_info.depth + 1) ==
				ParamTilingType::SCHEDULED) {
			tiling_info_list[i + 1].outter_for->parallel |= CLAST_PARALLEL_OMP;
		}

		if (tiling_info.outter_for->parallel & CLAST_PARALLEL_OMP) {
			log(std::cerr) << "found parallel loop" << std::endl;
			tiling_info.outter_for->parallel |= CLAST_PARALLEL_USER;
			char buf[512] = {0};
			sprintf(buf, "omp parallel for firstprivate(");
			TRAHRHE_GEN_OUTTER_UB(sprintf, buf + strlen(buf),
								  tiling_info.gen_data);
			sprintf(buf + strlen(buf), ", ");
			TRAHRHE_GEN_PCMAX(sprintf, buf + strlen(buf), tiling_info.gen_data);
			sprintf(buf + strlen(buf), ", ");
			TRAHRHE_GEN_TARGET_VOL(sprintf, buf + strlen(buf),
								   tiling_info.gen_data);
			sprintf(buf + strlen(buf), ") \\\n");

			tiling_info.outter_for->user_directive = strdup(buf);

			memset(buf, 0, 256);
			sprintf(buf, "%s,", tiling_info.outter_for->private_vars);
			TRAHRHE_GEN_INNER_LB(sprintf, buf + strlen(buf),
								 tiling_info.gen_data);
			sprintf(buf + strlen(buf), ", ");
			TRAHRHE_GEN_INNER_UB(sprintf, buf + strlen(buf),
								 tiling_info.gen_data);
			if (getTilingType(tiling_info.depth) ==
				ParamTilingType::SCHEDULED) {
				auto &prev = tiling_info_list[i - 1];
				sprintf(buf + strlen(buf), ", ");
				TRAHRHE_GEN_INNER_LB(sprintf, buf + strlen(buf),
									 &prev.gen_data[1]);
				sprintf(buf + strlen(buf), ", ");
				TRAHRHE_GEN_INNER_UB(sprintf, buf + strlen(buf),
									 &prev.gen_data[1]);
			}

			free(tiling_info.outter_for->private_vars);
			tiling_info.outter_for->private_vars = strdup(buf);

			for (int j = i + 1; j < tiling_info_list.size(); j += 1) {
				memset(buf, 0, 256);
				sprintf(buf, "%s,", tiling_info.outter_for->private_vars);
				TRAHRHE_GEN_OUTTER_UB(sprintf, buf + strlen(buf),
									  tiling_info_list[j].gen_data);
				sprintf(buf + strlen(buf), ", ");
				TRAHRHE_GEN_PCMAX(sprintf, buf + strlen(buf),
								  tiling_info_list[j].gen_data);
				sprintf(buf + strlen(buf), ", ");
				TRAHRHE_GEN_TARGET_VOL(sprintf, buf + strlen(buf),
									   tiling_info_list[j].gen_data);
				sprintf(buf + strlen(buf), ", ");
				TRAHRHE_GEN_INNER_LB(sprintf, buf + strlen(buf),
									 tiling_info_list[j].gen_data);
				sprintf(buf + strlen(buf), ", ");
				TRAHRHE_GEN_INNER_UB(sprintf, buf + strlen(buf),
									 tiling_info_list[j].gen_data);

				free(tiling_info.outter_for->private_vars);
				tiling_info.outter_for->private_vars = strdup(buf);
			}
		}
	}

	if (this->gen_trahrhe) {
		compute_trahrhe_isl_domain(nest, group, tiling_info_list);
	}
	// TODO: function to generate the header file name
	std::string macro = "#include \"trahrhe_header.n" +
						std::to_string(nest.getId()) + ".g" +
						std::to_string(group->id) + ".h\"";
	macrosData->list.push_back(macro);

	for (auto &tiling_info : tiling_info_list) {
		// free the gen_data
		for (int i = 0; i < 2; i += 1) {
			trahrhe_codegen_free(&tiling_info.gen_data[i]);
		}
	}
}

/**
 * Check if the scop can be tiled
 * Prevent errors when the scop is not well formed
 */
bool canBeTiled(osl_scop_p scop, struct clast_root *root) {
	// TODO: this function need a lot of work
	if (scop == NULL || root == NULL) {
		return false;
	}
	if (scop->statement == NULL) {
		return false;
	}
	osl_statement_p stmt = scop->statement;
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

void pesto::pass::ClastParamTilingPass::runSingleNest(OslScop &nest) {
	// this nest will only be used for trahrhe domain calculations
	OslScop tiled_nest = OslScop(nest.getId(), nest.getScop());
	this->tiling_pass->runSingleNest(tiled_nest);

	// First we apply pluto
	log(std::cerr) << "Running pluto pass on nest " << nest.getId()
				   << std::endl;
	((PlutoPass *)this->pluto_pass.get())->runSingleNest(nest);

	// Then we generate the clast
	log(std::cerr) << "Generating clast for nest " << nest.getId() << std::endl;
	this->gen_clast_pass->runSingleNest(nest);

	// We made sure in the run() function that the nest has a clast metadata
	if (!nest.metadata.get<ClastHolder>().lock()) {
		log(std::cerr) << "Clast generation failed" << std::endl;
		return;
	}
	auto metadata = nest.metadata.get<ClastHolder>().lock();

	struct clast_root *root = (struct clast_root *)metadata->root;
	if (root == NULL) {
		return;
	}
	log(std::cerr) << "Running pass on nest " << nest.getId() << std::endl;
	if (!canBeTiled(nest.getScop(), root)) {
		log(std::cerr) << "can't apply pass on this scop " << std::endl;
		return;
	}

	// we retrieve all statements of the nest
	int stmt_count = 0;
	struct clast_stmt **stmt =
		clast_get_user_stmt_list((struct clast_stmt *)root, &stmt_count);

	// we create a vector of statements to process
	std::vector<struct clast_stmt *> stmt_to_process;
	for (int i = 0; i < stmt_count; i++) {
		stmt_to_process.push_back(stmt[i]);
	}
	int group_count = 0;

	while (stmt_to_process.size() != 0) {
		// group statement that have same for parent (ie same domain)
		std::vector<struct clast_stmt *> stmt_group;

		// we create a group with the first statement
		stmt_group.push_back(stmt_to_process.back());
		stmt_to_process.pop_back(); // remove the statement from the to process
									// list

		// we retrieve the parent for of the group
		struct clast_for *parent_for = clast_get_parent_clast_for(
			(struct clast_stmt *)root,
			(struct clast_stmt *)stmt_group.begin()[0]);

		// for each statement that has not been processed yet
		for (std::vector<struct clast_stmt *>::iterator it =
				 stmt_to_process.begin();
			 it != stmt_to_process.end();) {
			// we add the statement to the group if it has the same parent
			// clast_for
			struct clast_for *tmp = clast_get_parent_clast_for(
				(struct clast_stmt *)root, (struct clast_stmt *)*it);
			if (parent_for == tmp) {
				stmt_group.push_back(*it);
				it = stmt_to_process.erase(
					it); // `erase` returns the next iterator
			} else {
				++it; // Increment the iterator only if no element was
					  // erased
			}
		}
		struct StmtGroup group = {
			.id = group_count,
			.stmts = stmt_group,
			.parent_for = parent_for,
		};

		this->applyTiling(nest, (struct clast_stmt *)root, &group);
		group_count += 1;
	}

	free(stmt);
}

void pesto::pass::ClastParamTilingPass::setPlutoPass(
	std::unique_ptr<pass::IPass> pluto_pass) {
	this->pluto_pass = std::move(pluto_pass);
}

void pesto::pass::ClastParamTilingPass::pushTilingType(ParamTilingType type) {
	this->tiling_types.push_back(type);
}

pesto::pass::ParamTilingType
pesto::pass::ClastParamTilingPass::getTilingType(int depth) {
	if (this->tiling_types.size() == 0) {
		return ParamTilingType::ALGEBRAIC;
	}
	if (depth < this->tiling_types.size()) {
		return this->tiling_types[depth];
	}
	return this->tiling_types[this->tiling_types.size() - 1];
}

void pesto::pass::ClastParamTilingPass::algebraicScheduleCodegen(
	OslScop &nest, tiling_info &current, tiling_info &prev) {
	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>().lock();

	// generate the scheduling macros
	char *buffer = (char *)malloc(1024 * sizeof(char));

	FILE *buffer_fp = fmemopen(buffer, 1024, "w");
	trahrhe_codegen_body_stmt_schedule(buffer_fp, &current.gen_data[1],
									   &prev.gen_data[1]);
	fflush(buffer_fp);
	std::string macro = "#define S" +
						std::to_string(current.gen_data[1].stmt_id) + "() " +
						buffer;
	macrosData->list.push_back(macro);
	memset(buffer, 0, 1024);

	rewind(buffer_fp);
	trahrhe_codegen_header_stmt_schedule(buffer_fp, &current.gen_data[0],
										 &prev.gen_data[0]);
	fflush(buffer_fp);

	macro = "#define S" + std::to_string(current.gen_data[0].stmt_id) + "() " +
			buffer;
	macrosData->list.push_back(macro);
	memset(buffer, 0, 1024);

	trahrhe_codegen_body_stmt_schedule(buffer_fp, &prev.gen_data[1],
									   &current.gen_data[1]);
	fflush(buffer_fp);
	macro =
		"#define S" + std::to_string(prev.gen_data[1].stmt_id) + "() " + buffer;
	macrosData->list.push_back(macro);
	memset(buffer, 0, 1024);

	rewind(buffer_fp);
	trahrhe_codegen_header_stmt_schedule(buffer_fp, &prev.gen_data[0],
										 &current.gen_data[0]);
	fflush(buffer_fp);

	macro =
		"#define S" + std::to_string(prev.gen_data[0].stmt_id) + "() " + buffer;
	macrosData->list.push_back(macro);
	memset(buffer, 0, 1024);

	// insert schedule free statement
	auto metadata = nest.metadata.get<ClastHolder>().lock();

	int stmt_id = ++metadata->stmtsNb;
	struct clast_user_stmt *footer_stmt =
		pesto_clast_create_stmt(metadata->cloogOptions, NULL, stmt_id);
	clast_insert_after(&prev.outter_for->stmt, &footer_stmt->stmt);

	macro = "#define S" + std::to_string(stmt_id) + "() " +
			"SCHEDULE_FREE(tiles, schedule);";
	macrosData->list.push_back(macro);

	fclose(buffer_fp);
	free(buffer);
}

void pesto::pass::ClastParamTilingPass::algebraicScheduleHeaderGen(
	OslScop &nest, tiling_info &current, tiling_info &prev) {
	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>().lock();

	macrosData->list.push_back("#include \"schedule.h\"");

	std::string name =
		"trahrhe_schedule.n" + std::to_string(nest.getId()) + ".h";

	macrosData->list.push_back("#include \"" + name + "\"");

	FILE *fd = fopen(name.c_str(), "w");
	fprintf(fd, "#ifndef _TRAHRHE_SCHEDULE_N%d_H_\n", nest.getId());
	fprintf(fd, "#define _TRAHRHE_SCHEDULE_N%d_H_\n", nest.getId());

	fprintf(fd, "INIT_TILES_PROTO(");
	for (int i = 0; i < prev.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "long int %s", prev.gen_data[0].params[i]);
	}
	fprintf(fd, ") {\n");
	fprintf(fd, "long int t0_pcmax = ");
	TRAHRHE_GEN_ERHART_FN(fprintf, fd, &prev.gen_data[0]);
	fprintf(fd, "(");
	for (int i = 0; i < prev.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "%s", prev.gen_data[0].params[i]);
	}
	fprintf(fd, ");\n");
	fprintf(fd, "long int target_vol_l0 = t0_pcmax / ");
	TRAHRHE_GEN_DIV(fprintf, fd, &prev.gen_data[0]);
	fprintf(fd, ";\n");
	fprintf(fd, "long int t1_pcmax, target_vol_l1;\n");
	fprintf(fd, "long int ");
	TRAHRHE_GEN_INNER_LB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, ", ");
	TRAHRHE_GEN_INNER_UB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, ";\n");

	fprintf(fd, "for(long int i = 0; i < "); // begin for i
	TRAHRHE_GEN_DIV(fprintf, fd, &prev.gen_data[0]);
	fprintf(fd, "; i++) {\n");
	TRAHRHE_GEN_INNER_LB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, " = ");
	TRAHRHE_GEN_TRAHRHE_FN(fprintf, fd, &prev.gen_data[0]);
	fprintf(fd, "(max(1, i * target_vol_l0), ");
	for (int i = 0; i < prev.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "%s", prev.gen_data[0].params[i]);
	}
	fprintf(fd, ");\n");
	TRAHRHE_GEN_INNER_UB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, " = ");
	TRAHRHE_GEN_TRAHRHE_FN(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, "(min(t0_pcmax, (i + 1) * target_vol_l0), ");
	for (int i = 0; i < prev.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "%s", prev.gen_data[0].params[i]);
	}
	fprintf(fd, ") - 1;\n");

	fprintf(fd, "if (i == ");
	TRAHRHE_GEN_DIV(fprintf, fd, &prev.gen_data[0]);
	fprintf(fd, " - 1) {\n");
	TRAHRHE_GEN_INNER_UB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, " = %s;\n", prev.gen_data[1].ub_expr);
	fprintf(fd, "}\n");

	fprintf(fd, "t1_pcmax = ");
	TRAHRHE_GEN_ERHART_FN(fprintf, fd, &current.gen_data[0]);
	fprintf(fd, "(");
	for (int i = 0; i < current.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "%s", current.gen_data[0].params[i]);
	}
	fprintf(fd, ");\n");
	fprintf(fd, "target_vol_l1 = t1_pcmax / ");
	TRAHRHE_GEN_DIV(fprintf, fd, &current.gen_data[0]);
	fprintf(fd, ";\n");

	fprintf(fd, "for(long int j = 0; j < "); // begin for j
	TRAHRHE_GEN_DIV(fprintf, fd, &current.gen_data[0]);
	fprintf(fd, "; j++) {\n");

	fprintf(fd, "tiles[i][j].lb0 = ");
	TRAHRHE_GEN_INNER_LB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, ";\n");
	fprintf(fd, "tiles[i][j].ub0 = ");
	TRAHRHE_GEN_INNER_UB(fprintf, fd, &prev.gen_data[1]);
	fprintf(fd, ";\n");

	fprintf(fd, "tiles[i][j].lb1 = ");
	TRAHRHE_GEN_TRAHRHE_FN(fprintf, fd, &current.gen_data[0]);
	fprintf(fd, "(max(1, j * target_vol_l1), ");
	for (int i = 0; i < current.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "%s", current.gen_data[0].params[i]);
	}
	fprintf(fd, ");\n");
	fprintf(fd, "tiles[i][j].ub1 = ");
	TRAHRHE_GEN_TRAHRHE_FN(fprintf, fd, &current.gen_data[1]);
	fprintf(fd, "(min(t1_pcmax, (j + 1) * target_vol_l1), ");
	for (int i = 0; i < current.gen_data[0].npar; i += 1) {
		if (i != 0) {
			fprintf(fd, ", ");
		}
		fprintf(fd, "%s", current.gen_data[0].params[i]);
	}
	fprintf(fd, ") - 1;\n");
	fprintf(fd, "tiles[i][j].marked = 0;\n");
	fprintf(fd, "if(j == ");
	TRAHRHE_GEN_DIV(fprintf, fd, &current.gen_data[0]);
	fprintf(fd, " - 1) {\n");
	fprintf(fd, "tiles[i][j].ub1 = %s;\n", current.gen_data[1].ub_expr);
	fprintf(fd, "}\n");

	fprintf(fd, "}\n"); // end for j
	fprintf(fd, "}\n"); // end for i
	fprintf(fd, "}\n"); // end init_tile fn

	fprintf(fd, "#endif // _TRAHRHE_SCHEDULE_N%d_H_\n", nest.getId());

	fclose(fd);
}

__isl_give isl_set *pesto::pass::ClastParamTilingPass::removeTilingDimensions(
	__isl_take isl_set *set,
	std::vector<struct tiling_info> &tiling_info_list) {

	isl_set *res = isl_set_copy(set);
	isl_set_free(set);
	std::vector<int> idx;
	while (idx.size() < tiling_info_list.size()) {
		int max_idx = -1;
		int max_depth = -1;
		for (int i = 0; i < tiling_info_list.size(); i += 1) {
			// check if we already removed the dimension of this loop
			if (std::find(idx.begin(), idx.end(), i) != idx.end()) {
				continue;
			}
			int cmp_depth =
				isl_set_dim(isl_set_from_cloog_domain(
								tiling_info_list[i].outter_for->domain),
							isl_dim_set);
			if (cmp_depth > max_depth) {
				max_depth = cmp_depth;
				max_idx = i;
			}
		}
		int ndim =
			isl_set_dim(isl_set_from_cloog_domain(
							tiling_info_list[max_idx].outter_for->domain),
						isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set,
													  ndim - 1, 1);
		res = isl_set_remove_dims(res, isl_dim_set, ndim - 1, 1);
		idx.push_back(max_idx);
	}

	return res;
}

__isl_give isl_set *pesto::pass::ClastParamTilingPass::dropTilingConstraints(
	__isl_take isl_set *set,
	std::vector<struct tiling_info> &tiling_info_list) {

	isl_set *res = isl_set_copy(set);
	isl_set_free(set);
	std::vector<int> idx;
	while (idx.size() < tiling_info_list.size()) {
		int max_idx = -1;
		int max_depth = -1;
		for (int i = 0; i < tiling_info_list.size(); i += 1) {
			// check if we already removed the dimension of this loop
			if (std::find(idx.begin(), idx.end(), i) != idx.end()) {
				continue;
			}
			int cmp_depth =
				isl_set_dim(isl_set_from_cloog_domain(
								tiling_info_list[i].outter_for->domain),
							isl_dim_set);
			if (cmp_depth > max_depth) {
				max_depth = cmp_depth;
				max_idx = i;
			}
		}
		int ndim =
			isl_set_dim(isl_set_from_cloog_domain(
							tiling_info_list[max_idx].outter_for->domain),
						isl_dim_set);

		res = isl_set_drop_constraints_involving_dims(res, isl_dim_set,
													  ndim - 1, 1);
		idx.push_back(max_idx);
	}

	return res;
}
