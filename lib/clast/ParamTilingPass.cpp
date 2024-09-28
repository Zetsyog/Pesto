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

using namespace pesto;

ClastParamTilingPass::ClastParamTilingPass()
	: IPolySingleNestPass("pass-param-tiling"), gen_trahrhe(false) {}

ClastParamTilingPass::~ClastParamTilingPass() {}

void ClastParamTilingPass::run(PolyProgram &prog) {
	// ensure each nest has a clast metadata
	for (auto &nest : prog.getScopNests()) {
		if (!nest->metadata.get<ClastMetadata>().has_value()) {
			log(std::cerr) << "Nest does not have clast metadata. You must "
							  "run the 'clast-gen' pass before this one."
						   << std::endl;
			log(std::cerr) << "Aborting pass." << std::endl;
			return;
		}
	}

	// run the pass on each nest
	for (auto &nest : prog.getScopNests()) {
		runSingleNest(*nest);
	}
}

struct tiling_info {
	int depth;
	struct clast_for *inner_for;
	struct clast_for *outter_for;
	struct trahrhe_codegen_data gen_data[2];
};

void replace_outter_tiled_for_bounds(ClastMetadata &metadata,
									 struct tiling_info &tiling_info) {
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

void replace_inner_tiled_for_bounds(ClastMetadata &metadata,
									struct clast_stmt *root,
									struct tiling_info &tiling_info) {
	struct clast_for *outter_for = tiling_info.outter_for;
	struct clast_for *inner_for = tiling_info.inner_for;

	free_clast_expr(inner_for->LB);
	free_clast_expr(inner_for->UB);

	inner_for->LB = clast_for_compute_original_bound(
		metadata.cloogProgram->names, inner_for, outter_for, 0);
	inner_for->UB = clast_for_compute_original_bound(
		metadata.cloogProgram->names, inner_for, outter_for, 1);

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

	red = new_clast_reduction(clast_red_max, 2);
	red->elts[0] = inner_for->LB;
	red->elts[1] = (struct clast_expr *)param_lb;
	inner_for->LB = (struct clast_expr *)red;

	red = new_clast_reduction(clast_red_min, 2);
	red->elts[0] = inner_for->UB;
	red->elts[1] = (struct clast_expr *)param_ub;
	inner_for->UB = (struct clast_expr *)red;
}

void insert_tiling_statements(ScopNest &nest, struct clast_stmt *root,
							  struct tiling_info &tiling_info) {
	auto metadata = nest.metadata.get<ClastMetadata>().value();

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

	CloogNames *ub_names = cloog_names_hard_copy(metadata->cloogProgram->names);
	for (int i = 0; i < ub_names->nb_scattering; i += 1) {
		char *it_name = ub_names->scattering[i];
		ub_names->scattering[i] = (char *)malloc(32 * sizeof(char));
		snprintf(ub_names->scattering[i], 32, "g%d_ub%s",
				 tiling_info.gen_data[0].group_id, it_name);
		free(it_name);
	}

	for (int bound = 0; bound < 2; bound += 1) {
		struct clast_expr *expr = clast_for_compute_original_bound(
			bound == 1 ? ub_names : metadata->cloogProgram->names,
			tiling_info.inner_for, tiling_info.outter_for, bound);
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

	cloog_names_free(ub_names);
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
void addVarDecls(ScopNest &nest, struct tiling_info &tiling_info) {
	auto varDeclData = nest.metadata.getOrCreate<VarDeclMetadata>();
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

void generate_stmt_macros(ScopNest &nest, struct tiling_info &tiling_info) {
	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>();

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

void compute_trahrhe_isl_domain(ScopNest &nest, struct StmtGroup *group,
								std::vector<tiling_info> &tiling_info_list) {
	auto metadata = nest.metadata.get<ClastMetadata>().value();
	auto scopData = nest.metadata.get<OpenScopMetadata>().value();

	isl_ctx *ctx = isl_ctx_alloc();

	// compute isl domain to feed trahrhe software with
	// isl_set *domain =
	// 	isl_set_from_osl_stmt(ctx, scopData->scops["original"]->statement);

	isl_set *domain =
		isl_set_copy(isl_set_from_cloog_domain(group->parent_for->domain));

	domain = isl_set_remove_tiling_dimensions(domain);

	for (int i = 0; i < metadata->cloogProgram->names->nb_parameters; i += 1) {
		domain =
			isl_set_set_dim_name(domain, isl_dim_param, i,
								 metadata->cloogProgram->names->parameters[i]);
	}

	for (int i = 0; i < tiling_info_list.size(); i += 1) {
		auto &tiling_info = tiling_info_list[i];

		char buffer[256] = {0};
		snprintf(buffer, 256, "n%d_g%d_%s", nest.getId(), group->id,
				 tiling_info.inner_for->iterator);
		domain = isl_set_set_dim_name(domain, isl_dim_set, i, buffer);
	}

	int ndim = isl_set_dim(domain, isl_dim_set);

	for (int i = 0; i < ndim; i += 1) {
		domain = isl_set_lower_bound_val(domain, isl_dim_set, i,
										 isl_val_zero(isl_set_get_ctx(domain)));
	}

	std::cerr << "TEST DUMP" << std::endl;
	isl_set_dump(domain);
	std::cerr << std::endl << "END DUMP" << std::endl;

	FILE *outfd = fopen(".exec-once", "a");
	fprintf(outfd, "trahrhe_wrapper header.n%d.g%d.h ", nest.getId(),
			group->id);
	fprintf(outfd, " -d\"");

	isl_printer *printer = isl_printer_to_file(isl_set_get_ctx(domain), outfd);
	isl_printer_print_set(printer, domain);
	isl_printer_free(printer);

	fprintf(outfd, "\"");
	fprintf(outfd, " -e -t%d -m\"d\" \n", ndim);

	fclose(outfd);

	isl_set_free(domain);

	isl_ctx_free(ctx);
}

void ClastParamTilingPass::applyTiling(ScopNest &nest, struct clast_stmt *root,
									   struct StmtGroup *group) {
	auto metadata = nest.metadata.get<ClastMetadata>().value();
	auto macrosData = nest.metadata.getOrCreate<MacroMetadata>();

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

	// apply tiling
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

	std::sort(tiling_info_list.begin(), tiling_info_list.end(),
			  [](tiling_info &a, tiling_info &b) { return a.depth < b.depth; });

	for (int i = 0; i < tiling_info_list.size(); i++) {
		auto &tiling_info = tiling_info_list[i];

		for (int i = 0; i < 2; i += 1) {
			tiling_info.gen_data[i].group_id = group->id;
			tiling_info.gen_data[i].depth = tiling_info.depth;
		}
		insert_tiling_statements(nest, root, tiling_info);
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
		replace_inner_tiled_for_bounds(*metadata, root, tiling_info);
		replace_outter_tiled_for_bounds(*metadata, tiling_info);
		addVarDecls(nest, tiling_info);
		generate_stmt_macros(nest, tiling_info);
	}

	for (int i = 0; i < tiling_info_list.size(); i++) {
		auto &tiling_info = tiling_info_list[i];
		if (tiling_info.outter_for->parallel & CLAST_PARALLEL_OMP) {
			log(std::cerr) << "found parallel loop" << std::endl;
			tiling_info.outter_for->parallel |= CLAST_PARALLEL_USER;
			char buf[256] = {0};
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
	std::string macro = "#include \"header.n" + std::to_string(nest.getId()) +
						".g" + std::to_string(group->id) + ".h\"";
	macrosData->list.push_back(macro);

	for (auto &tiling_info : tiling_info_list) {
		// free the gen_data
		for (int i = 0; i < 2; i += 1) {
			trahrhe_codegen_free(&tiling_info.gen_data[i]);
		}
	}
}

void ClastParamTilingPass::runSingleNest(ScopNest &nest) {
	// We made sure in the run() function that the nest has a clast metadata
	auto metadata = nest.metadata.get<ClastMetadata>().value();

	struct clast_root *root = (struct clast_root *)metadata->root;
	if (root == NULL) {
		return;
	}
	log(std::cerr) << "Running pass on nest " << nest.getId() << std::endl;

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
