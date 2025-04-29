#!/usr/bin/env bash

extract_scop_bin="@EXTRACT_SCOP_BIN@"
replace_scop_bin="@REPLACE_SCOP_BIN@"
pesto_bin="valgrind --leak-check=full @PESTO_TOOL_BIN@"

# TODO: custom compiler support
CPP=@CMAKE_C_COMPILER@
pesto_gen_prefix=".pesto.gen"

usage() {
    echo "Usage: pesto [options] <input_file>" >&2
    echo "Options:" >&2
    printf "%-30s%s\n" "  -o, --output <file>" "Path of output file" >&2
    printf "%-30s%s\n" "  --config <file>" "Path of JSON config file" >&2
    printf "%-30s%s\n" "  --readscop" "Input file is using Openscop format" >&2
    printf "%-30s%s\n" "  -i, --indent" "Format generated code" >&2
    printf "%-30s%s\n" "  -h, --help" "Print this help message" >&2
    printf "%-30s%s\n" "  -v, --version" "Print the current version of the software" >&2
    exit 1
}

cleanup() {
    rm -f .pesto* .vardecls .kernel .exec-once
}

OPTS=$(getopt -o o:ihv --long output:,indent,config:,readscop,version,help -- "$@")
if [ $? != 0 ]; then
    echo "[pesto-cli] Failed to parse options" >&2
    exit 1
fi

# Set the parsed options
eval set -- "$OPTS"

# Parse command line arguments
output_file=""
indent=0
forward_opts=()
while true; do
    case "$1" in
    -o | --output)
        output_file="$2"
        shift 2
        ;;
    -i | --indent)
        indent=1
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    -v | --version)
        echo "Pesto CLI O.1"
        exit 0
        ;;
    --config)
        forward_opts+=("$1" "$2")
        shift 2
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

echo "[pesto-cli] Forwarding options:" "${forward_opts[@]}" >&2

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

output_dir=$(dirname "$output_file")

# We work on a copy of the input file
# Because we do in-place work
working_file=$(mktemp)
# Copy input file to temp file to start processing
# input file must not be modified
cp "$input_file" "$working_file"

echo "[pesto-cli] Working on file $working_file" >&2

# file to store include directives
include_file=$(mktemp)

echo "$pesto_gen_prefix" >".pesto_prefix"

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

# Function to process the input file and extract pragma sections
process_file() {
    local infile="$1"
    local scop_file
    local body_file

    echo "[pesto-cli] Processing file $infile" >&2

    # scop_file is a temporary file that contains only the input code delimited by the pragmas
    scop_file=$(mktemp)

    echo "[pesto-cli] Extracting scop to $scop_file" >&2

    echo "#pragma scop" >"$scop_file"
    ${extract_scop_bin} "$infile" >>"$scop_file"
    echo "#pragma endscop" >>"$scop_file"

    rm -f ${pesto_gen_prefix}.*
    # DEBUG
    echo "@COMMAND" >&2
    echo "${pesto_bin} $scop_file" "${forward_opts[@]}" >&2
    # run pesto tool on the extracted scop
    # shellcheck disable=SC2068
    ${pesto_bin} "$scop_file" ${forward_opts[@]}
    if [ $? -ne 0 ]; then
        echo "[pesto-cli] ERROR: pesto-tool failed" >&2
        cleanup
        exit 1
    fi

    generated_files="${pesto_gen_prefix}.*"

    # body_file is a temporary file that contains the generated code
    body_file=$(mktemp)
    echo "" >"${body_file}"

    # for each generated file
    for file in $generated_files; do
        if [[ ! -f "$file" ]]; then
            echo "[pesto-cli] ERROR: generated file $file not found" >&2
            cleanup
            exit 1
        fi
        echo "[pesto-cli] DEBUG: processing $file" >&2

        # extract include directives
        grep "^#include" "${file}" >>"${include_file}"

        # remove in place all include directives
        sed -i '/^#include/d' "${file}"

        # run preprocessor on file write it to output file
        ${CPP} -E -P "$file" >>"$body_file"
        if [ $? -ne 0 ]; then
            echo "[pesto-cli] ERROR: Preprocessor failed" >&2
            cleanup
            exit 1
        fi

        # rm -f "$file"
    done

    # if there is a .vardecls file, append it to the body file
    # this file should contain variable declarations
    if [[ -f ".vardecls" ]]; then
        cat .vardecls "$body_file" >.tmp && mv .tmp "$body_file"
        rm -f .vardecls
    fi

    echo "[pesto-cli] DEBUG: replacing scop in $infile with $body_file" >&2
    cat "$include_file" >".kernel"
    ${replace_scop_bin} "$infile" "$body_file" >>".kernel"
    mv ".kernel" "$infile"

    # cleanup generated files
    rm -f ${pesto_gen_prefix}.*
}

# Function to check if file contains #pragma scop
contains_pragma() {
    grep -q "#pragma scop" "$1"
}

echo "[pesto-cli] Starting pesto" >&2
echo "[pesto-cli] Input file: $input_file" >&2
echo "[pesto-cli] Filename : $filename" >&2
echo "[pesto-cli] Output file: $output_file" >&2
echo "[pesto-cli] Include file: $include_file" >&2

# Loop until there are no more pragmas to process
while contains_pragma "$working_file"; do
    # TODO: handle openscop input format
    process_file "$working_file"
done

if [[ $indent -ne 0 ]]; then
    if which clang-format &>/dev/null; then
        clang-format --style="{BasedOnStyle: LLVM, ColumnLimit: 120}" -i "${working_file}"
    else
        echo "[pesto-cli] Indent flag is set but clang-format is not found, skipping indentation" >&2
    fi
fi

# Move the final processed file to the specified output file
echo "[pesto-cli] Done working on $output_file" >&2
mv "$working_file" "$output_file"

# cleanup
cleanup
