#!/usr/bin/env bash

# Function to display usage
usage() {
    echo "[replace-scop] Usage: $0 source_file scop_file" >&2
    exit 1
}

# Check if the correct number of arguments are provided
if [ "$#" -ne 2 ]; then
    usage
fi

source_file="$1"
scop_file="$2"

# Check if source_file and scop_file exist
if [ ! -f "$source_file" ]; then
    echo "[replace-scop] Error: Source file '$source_file' not found!" >&2
    exit 1
fi

if [ ! -f "$scop_file" ]; then
    echo "[replace_scop] Error: Scop file '$scop_file' not found!" >&2
    exit 1
fi

inside_scop=0
scop_replaced=0

# Read through the source file and replace the first #pragma scop block
while IFS= read -r line; do
    if [[ "$line" == "#pragma scop" && "$scop_replaced" -eq 0 ]]; then
        inside_scop=1
        cat "$scop_file"
        scop_replaced=1
        continue
    fi

    if [[ "$line" == "#pragma endscop" && "$inside_scop" -eq 1 ]]; then
        inside_scop=0
        continue
    fi

    if [ "$inside_scop" -eq 0 ]; then
        echo "$line"
    fi
done <"$source_file"

# Check if #pragma scop was not found and hence not replaced
if [ "$scop_replaced" -eq 0 ]; then
    echo "[replace-scop] Warning: No #pragma scop block found in the source file." >&2
fi
