#!/usr/bin/env bash
set -euo pipefail

pesto=$1
cc=$2
pids=()

# All generators share a working directory; each must own its intermediate files.
for i in 1 2 3 4; do
    rm -f "output-$i.c"
    sed "s/i \* n + j/i * n + j + $i/" input.c > "input-$i.c"
    "$pesto" "input-$i.c" -o "output-$i.c" >"generator-$i.log" 2>&1 &
    pids+=("$!")
done

status=0
for pid in "${pids[@]}"; do
    wait "$pid" || status=1
done
if ((status)); then
    cat generator-*.log
    exit 1
fi

for i in 1 2 3 4; do
    "$cc" -O2 -fopenmp "input-$i.c" -o "baseline-$i" -lm
    "$cc" -O2 -fopenmp "output-$i.c" -o "transformed-$i" -lm
    "./baseline-$i" > "expected-$i"
    OMP_NUM_THREADS=2 "./transformed-$i" > "actual-$i"
    cmp "expected-$i" "actual-$i"
done
