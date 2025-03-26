#!/usr/bin/env bash

# Edit a file to add the necessary code to measure some metrics for each thread

# Usage: per_thread_metric.sh <input.c> <output.c>

# Check the number of arguments
if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <input.c> <output.c>"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="$2"

# Check if the input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo "File $INPUT_FILE does not exist"
    exit 1
fi

# we add our include
echo '#ifdef LIKWID_PERFMON
#include <likwid-marker.h>
#else
#define LIKWID_MARKER_INIT
#define LIKWID_MARKER_THREADINIT
#define LIKWID_MARKER_SWITCH
#define LIKWID_MARKER_REGISTER(regionTag)
#define LIKWID_MARKER_START(regionTag)
#define LIKWID_MARKER_STOP(regionTag)
#define LIKWID_MARKER_CLOSE
#define LIKWID_MARKER_GET(regionTag, nevents, events, time, count)
#include <omp.h>
#endif
' >"$OUTPUT_FILE"

STATE_COPY=0
STATE_READ_PRAGMA=1
STATE_IN_FOR=2
region_counter=0

state=$STATE_COPY
braces=0

# count the number of parallel regions
n_region=$(grep -c "#pragma omp parallel for" "$INPUT_FILE")

# read input file line by line
while IFS= read -r line; do
    if [[ "$state" -eq "$STATE_COPY" ]]; then
        # if we are not in an omp for
        if [[ "$line" == *"#pragma omp parallel for"* ]]; then
            # we enter an omp for

            # extract everything after the for
            omp_clauses=$(echo "$line" | sed 's/.*#pragma omp parallel for//')

            {
                echo "#pragma omp parallel"
                echo "{"
                echo "LIKWID_MARKER_START(\"nest_$region_counter\");"
                echo "#pragma omp for nowait $omp_clauses"
            } >>"$OUTPUT_FILE"

            # go to read pragma state to handle multiline state
            state=$STATE_READ_PRAGMA
        else # default case we just copy the line
            echo "$line" >>"$OUTPUT_FILE"
        fi
    elif [[ "$state" -eq "$STATE_READ_PRAGMA" ]]; then
        # we simply copy the line, unless the line is a for
        if [[ "$line" == *"for"* ]]; then
            # goto state in for
            state=$STATE_IN_FOR
            # if line contains a bracket, we need to count them
            if [[ "$line" == *"{"* ]]; then
                braces=1
            else
                braces=0
            fi
        fi
        echo "$line" >>"$OUTPUT_FILE"
    elif [[ "$state" -eq "$STATE_IN_FOR" ]]; then
        # we are in an omp for
        # we need to compute the number opening braces
        if [[ "$line" == *"{"* ]]; then
            braces=$((braces + 1))
        fi
        if [[ "$line" == *"}"* ]]; then
            braces=$((braces - 1))
        fi

        # if we are at the end of the for
        if [[ "$braces" -eq 0 ]]; then
            # exit the in for state
            state=$STATE_COPY
            # print the footer
            echo "$CODE_FOOTER" >>"$OUTPUT_FILE"
            {
                echo "}"
                echo "LIKWID_MARKER_STOP(\"nest_$region_counter\");"
                echo "}"
                echo "LIKWID_MARKER_SWITCH;"
            } >>"$OUTPUT_FILE"

            region_counter=$((region_counter + 1))

        else
            echo "$line" >>"$OUTPUT_FILE"
        fi
    fi
done <"$INPUT_FILE"

LIKWID_INIT='
    LIKWID_MARKER_INIT;
    #pragma omp parallel
    {
        LIKWID_MARKER_THREADINIT;
    
'

for i in $(seq 0 $((n_region - 1))); do
    LIKWID_INIT="$LIKWID_INIT
        LIKWID_MARKER_REGISTER(\"nest_$i\");
    "
done
LIKWID_INIT="$LIKWID_INIT
   }"

# Use awk to insert the block
awk -v block="$LIKWID_INIT" '
    /polybench_start_instruments/ {
        print block;
        print;
        next;
    }
    { print }
' "$OUTPUT_FILE" >temp && mv temp "$OUTPUT_FILE"
sed -i '/polybench_print_instruments;/a LIKWID_MARKER_CLOSE;' "$OUTPUT_FILE"
