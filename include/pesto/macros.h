/*
 * Pesto - A polyhedral loop nest optimizer
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2024 Clément Rossetti
 */

#ifndef PESTO_MACROS_H
#define PESTO_MACROS_H

#define PESTO_VERSION "0.1.0"

#define PESTO_NAME_PM "pesto-pm"

// TODO: stop using macros ?
#define PESTO_UUID_PASS_GEN_CLOOG "codegen-cloog"
#define PESTO_UUID_PASS_PARAM_TILING "param-tiling"
#define PESTO_UUID_PASS_SPLIT_NESTED_LOOPS "split-nested-loops"
#define PESTO_UUID_PASS_DUMP_OSL "dump-osl"
#define PESTO_UUID_PASS_PARAM_CONTEXT "param-context"
#define PESTO_UUID_PASS_CLAST_GEN "clast-gen"
#define PESTO_UUID_PASS_PLUTO_AUTO_TRANSFORM "pluto-auto-transform"
#define PESTO_UUID_PASS_PLUTO_TILING "pluto-tiling"
#define PESTO_UUID_PASS_PLUTO_GENERIC "pluto"
#define PESTO_UUID_PASS_MATCH_CRITERIA "match"
#define PESTO_UUID_PASS_CLAST_CUSTOM_VEC_PRAGMA "clast-custom-vec-pragma"
#define PESTO_UUID_PASS_ISL "isl-dev"
#define PESTO_UUID_PASS_SPLIT_TILING "isl-tpz-tiling"

#endif // PESTO_MACROS_H