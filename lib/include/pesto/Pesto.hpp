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
#include <pesto/macros.h>
#include <pesto/pass/Pass.hpp>

/*
	Utilities
*/
#include <pesto/codegen/ParamTiling.hpp>

#include <pesto/ParamTilingUtility.hpp>

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
#include <pesto/pass/CodegenPasses.hpp>
#include <pesto/pass/IslPass.hpp>
#include <pesto/pass/OslPass.hpp>
#include <pesto/pass/PlutoPass.hpp>
#include <pesto/pass/TpzTilingPass.hpp>
#include <pesto/pass/clast/CustomVecPragma.hpp>
#include <pesto/pass/clast/GenClastPass.hpp>
#include <pesto/pass/clast/ParamTilingPass.hpp>
