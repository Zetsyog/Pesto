/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

/*
	Publicly available library headers
*/

#include <pesto/config.h>

/*
	Pesto Core
*/
#include <pesto/Logging.hpp>
#include <pesto/PassManager.hpp>
#include <pesto/Utils.hpp>
#include <pesto/bundle/Bundle.hpp>
#include <pesto/pass/Pass.hpp>

/*
	Utilities
*/
#include <pesto/codegen/ParamTiling.hpp>

#include <pesto/Pluto.hpp>

/*
	Metadata
*/
#include <pesto/metadata/FileInfo.hpp>
#include <pesto/metadata/Macro.hpp>
#include <pesto/metadata/VarDecl.hpp>
/*
	Byproducts
*/
#include <pesto/bundle/byproduct/HeaderFile.hpp>
#include <pesto/bundle/byproduct/OutputSourceFile.hpp>
#include <pesto/bundle/byproduct/SideSourceFile.hpp>

/*
	IRs
*/
#include <pesto/ir/ClastIR.hpp>
#include <pesto/ir/IR.hpp>
#include <pesto/ir/Isl2Scop.hpp>
#include <pesto/ir/OpenScopIR.hpp>
#include <pesto/ir/Scop2Isl.hpp>

/*
	Passes
*/
#include <pesto/pass/CloogCodegen.hpp>
#include <pesto/pass/DumpOsl.hpp>
#include <pesto/pass/IslTest.hpp>
#include <pesto/pass/MatchCriteria.hpp>
#include <pesto/pass/ParamContext.hpp>
#include <pesto/pass/Pluto.hpp>
#include <pesto/pass/TpzTiling.hpp>
#include <pesto/pass/clast/CustomVecPragma.hpp>
#include <pesto/pass/clast/GenClast.hpp>
#include <pesto/pass/clast/ParamTiling.hpp>