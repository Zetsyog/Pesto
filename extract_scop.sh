#!/usr/bin/env bash

# Check if the correct number of arguments are provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 input_file"
    exit 1
fi

input_file="$1"
inside_scop=0

# Remove the output file if it already exists

# Read through the input file line by line
while IFS= read -r line; do
    if [[ "$line" == "#pragma scop" ]]; then
        inside_scop=1
        continue
    fi

    if [[ "$line" == "#pragma endscop" ]]; then
        inside_scop=0
        break
    fi

    if [ "$inside_scop" -eq 1 ]; then
        echo "$line"
    fi
done <"$input_file"
