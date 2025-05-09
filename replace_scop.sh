#!/usr/bin/env bash

# Function to display usage
usage() {
    echo "Usage: $0 <source_file> <replacement_file>"
    exit 1
}

# Check if the correct number of arguments are provided
if [ "$#" -ne 2 ]; then
    usage
fi

source_file="$1"
replacement_file="$2"

# Check if source_file and scop_file exist
if [ ! -f "$source_file" ]; then
    echo "[replace-scop] Error: Source file '$source_file' not found!" >&2
    exit 1
fi

if [ ! -f "$replacement_file" ]; then
    echo "[replace_scop] Error: Scop file '$replacement_file' not found!" >&2
    exit 1
fi

awk -v repl_file="$replacement_file" '
  BEGIN {
    in_block = 0;
    replaced = 0;
  }
  {
    if (!replaced && $0 ~ /#pragma scop/) {
      in_block = 1;
      replaced = 1;
      while ((getline line < repl_file) > 0) print line;
      next;
    }
    if (in_block && $0 ~ /#pragma endscop/) {
      in_block = 0;
      next;
    }
    if (!in_block) {
      print;
    }
  }
' "$source_file"
