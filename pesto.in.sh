#!/usr/bin/env bash

# SPDX-License-Identifier: MIT
# Copyright (c) 2024 Clément Rossetti

set -euo pipefail

extract_scop_bin="@EXTRACT_SCOP_BIN@"
replace_scop_bin="@REPLACE_SCOP_BIN@"
pesto_bin="@PESTO_TOOL_BIN@"

# TODO: custom compiler support
CPP=@CMAKE_C_COMPILER@

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'

usage() {
    $pesto_bin -h
    exit 1
}

log() {
    local level="$1"
    shift
    echo -e "[$(date +"%H:%M:%S")] [$level] [pesto-cli] $*" >&2
}

error() {
    log "${RED}error${NC}" "$@"
}

debug() {
    if [[ -z "${DEBUG:-}" ]]; then
        return
    fi
    log "${BLUE}debug${NC}" "$@"
}

info() {
    log "${GREEN}info${NC}" "$@"
}

warn() {
    log "${YELLOW}warn${NC}" "$@"
}

cleanup() {
    rm -f .pesto* .kernel .exec-once
}

OPTS=$(getopt -o "c:o:ihv" --long "config:,output:,readscop,indent,help,version,debug" -- "$@")
if [ $? != 0 ]; then
    error "Failed to parse options"
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
    --debug)
        forward_opts+=("$1")
        DEBUG=1
        shift
        ;;
    --)
        shift
        break
        ;;
    *)
        error "Internal error!"
        exit 1
        ;;
    esac
done

# Check if only one input is provided
if [ $# -ne 1 ]; then
    error "Only one input file is allowed"
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
    error "Output is a directory"
    exit 1
fi

# Function to process the input file and extract pragma sections
process_file() {
    local infile="$1"
    local scop_file
    local body_file
    local tmp_dir
    local basename

    info "Processing file $infile"

    tmp_dir=$(mktemp -d)
    basename=$(basename "$infile")

    # scop_file is a temporary file that contains only the input code delimited by the pragmas
    scop_file=$tmp_dir/$basename.input

    info "Extracting scop to $scop_file"

    echo "#pragma scop" >"$scop_file"
    ${extract_scop_bin} "$infile" >>"$scop_file"
    echo "#pragma endscop" >>"$scop_file"

    pesto_outfile="$tmp_dir/$(basename "$output_file")"

    # run pesto tool on the extracted scop
    pesto_cmd="${pesto_bin} $scop_file ${forward_opts[*]} -o $pesto_outfile"

    # DEBUG
    debug "Running $pesto_cmd"
    if ! $pesto_cmd; then
        error "pesto-tool failed"
        debug "Command: $pesto_cmd"
        cleanup
        exit 1
    fi

    # body_file is a temporary file that contains the generated code
    body_file=${tmp_dir}/$basename.body
    echo "" >"${body_file}"

    debug "Processing $pesto_outfile"

    # extract include directives
    if grep -q "^#include" "$pesto_outfile"; then
        debug "Extracting #include directives from $pesto_outfile"
        grep "^#include" "$pesto_outfile" >>"${include_file}"
        # remove in place all include directives
        debug "Removing #include directives from $pesto_outfile"
        sed -i '/^#include/d' "$pesto_outfile"
    else
        debug "No #include directives found in $pesto_outfile"
    fi

    debug "Preprocessing $pesto_outfile to $body_file"

    # run preprocessor on file write it to output file
    cpp_cmd="${CPP} -E -P $pesto_outfile"
    if ! $cpp_cmd >>"$body_file"; then
        error "Preprocessor failed"
        debug "Command: $cpp_cmd"
        cleanup
        exit 1
    fi

    # handle headers files if some were generated
    header_files="$tmp_dir/*.h"
    for file in $header_files; do
        if [ -f "$file" ]; then
            info "Copying header file $file to $(dirname "$infile")"
        else
            warn "Header file $file does not exist"
            continue
        fi
        cp "$file" "$(dirname "$infile")"
    done

    debug "Replacing scop in $infile with $body_file"
    cat "$include_file" >".kernel"
    ${replace_scop_bin} "$infile" "$body_file" >>".kernel"
    mv ".kernel" "$infile"

}

# Function to check if file contains #pragma scop
contains_pragma() {
    grep -q "#pragma scop" "$1"
}

info "Starting pesto wrapper..."

working_dir=$(mktemp -d)

# We work on a copy of the input file
# Because we do in-place work
working_file=$working_dir/$full_filename
# Copy input file to temp file to start processing
# input file must not be modified
cp "$input_file" "$working_file"

info "Working on file $working_file"

# file to store include directives
include_file=$working_dir/.includes

info "Working dir: $working_dir"
info "Input file: $input_file"
info "Filename : $filename"
info "Output file: $output_file"
info "Include file: $include_file"

# Clear the include file
echo "" >"$include_file"
# extract existing include directives and define macros
if [ -f "$working_file" ]; then
    if grep -q "^#define" "$working_file"; then
        info "Extracting existing #define directives from $working_file"
        grep "^#define" "$working_file" >>"$include_file"
    fi
fi

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
        warn "Indent flag is given but clang-format is not found. Skipping."
    fi
fi

# Move the final processed file to the specified output file
info "Done working on $output_file"
mv "$working_file" "$output_file"

# handle header files
header_files="$working_dir/*.h"
for file in $header_files; do
    if [ -f "$file" ]; then
        info "Copying header file $file to $(dirname "$output_file")"
    else
        warn "Header file $file does not exist"
        continue
    fi
    cp "$file" "$(dirname "$output_file")"
done

rm -rf "$working_dir"

# cleanup
cleanup
