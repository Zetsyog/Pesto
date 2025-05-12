#!/usr/bin/env bash

# Check if the correct number of arguments are provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 input_file"
    exit 1
fi

input_file="$1"

# Read through the input file line by line
awk '/#pragma scop/ {inblock=1} inblock; /#pragma endscop/ && inblock {exit}' "$input_file"
