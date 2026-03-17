#!/usr/bin/env bash

# SPDX-License-Identifier: MIT
# Copyright (c) 2024 Clément Rossetti

extract_scop_bin="@EXTRACT_SCOP_BIN@"
replace_scop_bin="@REPLACE_SCOP_BIN@"
pesto_bin="@PESTO_TOOL_BIN@"

# TODO: custom compiler support
CPP=@CMAKE_C_COMPILER@

usage() {
    $pesto_bin -h
    exit 1
}

cleanup() {
    rm -f .pesto* .kernel .exec-once
}

OPTS=$(getopt -o "c:o:ihv" --long "config:,output:,readscop,indent,help,version" -- "$@")
if [ $? != 0 ]; then
    echo "[pesto-cli] Failed to parse options" >&2
    exit 1
fi

# Set the parsed options
eval set -- "$OPTS"

# Parse options
forward_opts=()
output_file=""
indent=0
while true; do
    case "$1" in
    -o | --output)
        output_file="$2"
        shift 2
        ;;
    -i | --indent)
        indent=1
        forward_opts+=("$1")
        shift
        ;;
    -c | --config)
        forward_opts+=("$1" "$2")
        shift 2
        ;;
    -v | --version)
        $pesto_bin -v
        exit 0
        ;;
    -h | --help)
        usage
        ;;
    --readscop)
        forward_opts+=("$1")
        shift
        ;;
    --)
        shift
        break
        ;;
    *)
        echo "[pesto-cli] Internal error!" >&2
        exit 1
        ;;
    esac
done

# Check if only one input is provided
if [ $# -ne 1 ]; then
    echo "[pesto-cli] ERROR: Only one input file is allowed" >&2
    usage
fi

# prevent uncleanups files from previous runs to interfere
cleanup

input_file=$1
full_filename=$(basename "$input_file")
filename=${full_filename##*/}
filename=${filename%%.*}

# Check if input file is provided
if [ -z "$input_file" ]; then
    usage
fi

# Set default output file if not provided
if [ -z "$output_file" ]; then
    output_file="${filename}.pesto.c"
fi

if [[ -d "$output_file" ]]; then
    echo "[pesto-cli] ERROR: Output is a directory" >&2
    exit 1
fi

# Function to process the input file and extract pragma sections
process_file() {
    local infile="$1"
    local scop_file
    local body_file
    local tmp_dir
    local basename

    echo "[pesto-cli] Processing file $infile" >&2

    tmp_dir=$(mktemp -d)
    basename=$(basename "$infile")

    # scop_file is a temporary file that contains only the input code delimited by the pragmas
    scop_file=$tmp_dir/$basename.input

    echo "[pesto-cli] Extracting scop to $scop_file" >&2

    echo "#pragma scop" >"$scop_file"
    ${extract_scop_bin} "$infile" >>"$scop_file"
    echo "#pragma endscop" >>"$scop_file"

    pesto_outfile="$tmp_dir/$(basename "$output_file")"

    # run pesto tool on the extracted scop
    pesto_cmd="${pesto_bin} $scop_file ${forward_opts[*]} -o $pesto_outfile"

    # DEBUG
    echo "[Debug] Running $pesto_cmd"
    if ! $pesto_cmd; then
        echo "[pesto-cli] ERROR: pesto-tool failed" >&2
        echo "$pesto_cmd" >&2
        cleanup
        exit 1
    fi

    # body_file is a temporary file that contains the generated code
    body_file=${tmp_dir}/$basename.body
    echo "" >"${body_file}"

    echo "[pesto-cli] DEBUG: processing $pesto_outfile" >&2

    # extract include directives
    grep "^#include" "$pesto_outfile" >>"${include_file}"

    # remove in place all include directives
    sed -i '/^#include/d' "$pesto_outfile"

    # run preprocessor on file write it to output file
    cpp_cmd="${CPP} -E -P $pesto_outfile"
    echo "$cpp_cmd"
    if ! $cpp_cmd >>"$body_file"; then
        echo "[pesto-cli] ERROR: Preprocessor failed" >&2
        cleanup
        exit 1
    fi

    # handle headers files if some were generated
    header_files="$tmp_dir/*.h"
    for file in $header_files; do
        if [ -f "$file" ]; then
            echo "[pesto-cli] Copying header file $file to $(dirname "$infile")" >&2
        else
            echo "[pesto-cli] WARNING: Header file $file does not exist" >&2
            continue
        fi
        cp "$file" "$(dirname "$infile")"
    done

    echo "[pesto-cli] DEBUG: replacing scop in $infile with $body_file" >&2
    cat "$include_file" >".kernel"
    ${replace_scop_bin} "$infile" "$body_file" >>".kernel"
    mv ".kernel" "$infile"

}

# Function to check if file contains #pragma scop
contains_pragma() {
    grep -q "#pragma scop" "$1"
}

echo "[pesto-cli] Starting pesto" >&2

working_dir=$(mktemp -d)

# We work on a copy of the input file
# Because we do in-place work
working_file=$working_dir/$full_filename
# Copy input file to temp file to start processing
# input file must not be modified
cp "$input_file" "$working_file"

echo "[pesto-cli] Working on file $working_file" >&2

# file to store include directives
include_file=$working_dir/.includes

echo "[pesto-cli] Working dir: $working_dir" >&2
echo "[pesto-cli] Input file: $input_file" >&2
echo "[pesto-cli] Filename : $filename" >&2
echo "[pesto-cli] Output file: $output_file" >&2
echo "[pesto-cli] Include file: $include_file" >&2

# Clear the include file
echo "" >"$include_file"
# extract existing include directives and define macros
grep "^#define" "$working_file" >>"$include_file"

# check if include file contains a line starting with #define min(x,y)
if ! grep -q "#define ceild(n,d)" "$include_file"; then
    {
        echo "#include <math.h>"
        echo "#define ceild(n,d)  (((n)<0) ? -((-(n))/(d)) : ((n)+(d)-1)/(d))"
        echo "#define floord(n,d) (((n)<0) ? -((-(n)+(d)-1)/(d)) : (n)/(d))"
        echo "#define max(x,y)    (((x) > (y)) ? (x) : (y))"
        echo "#define min(x,y)    (((x) < (y)) ? (x) : (y))"

        for ((i = 0; i <= 3; i++)); do
            echo "#ifndef DIV${i}"
            echo "#define DIV${i} 8"
            echo "#endif"
        done

    } >>"$include_file"
fi

# Loop until there are no more pragmas to process
while contains_pragma "$working_file"; do
    # TODO: handle openscop input format
    process_file "$working_file"
done

if [[ $indent -ne 0 ]]; then
    if which clang-format &>/dev/null; then
        clang-format --style="{BasedOnStyle: LLVM, ColumnLimit: 120}" -i "${working_file}"
    else
        echo "[pesto-cli] Indent flag is given but clang-format is not found. Skipping." >&2
    fi
fi

# Move the final processed file to the specified output file
echo "[pesto-cli] Done working on $output_file" >&2
mv "$working_file" "$output_file"

# handle header files
header_files="$working_dir/*.h"
for file in $header_files; do
    if [ -f "$file" ]; then
        echo "[pesto-cli] Copying header file $file to $(dirname "$infile")" >&2
    else
        echo "[pesto-cli] WARNING: Header file $file does not exist" >&2
        continue
    fi
    cp "$file" "$(dirname "$output_file")"
done

rm -rf "$working_dir"

# cleanup
cleanup
