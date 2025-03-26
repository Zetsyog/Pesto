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

CODE_HEADER='
#pragma omp parallel
{
    METRICS_ALLOC_CLOCK()
    METRICS_START_CLOCK();
'

CODE_FOOTER='
    }
    METRICS_STOP_CLOCK();
}

'

# we add our include
echo "#include <per_thread_metrics.h>" >"$OUTPUT_FILE"
echo "#include <omp.h>" >>"$OUTPUT_FILE"

STATE_COPY=0
STATE_READ_PRAGMA=1
STATE_IN_FOR=2

state=$STATE_COPY
braces=0

# read input file line by line
while IFS= read -r line; do
    if [[ "$state" -eq "$STATE_COPY" ]]; then
        # if we are not in an omp for
        if [[ "$line" == *"#pragma omp parallel for"* ]]; then
            # we enter an omp for

            # extract everything after the for
            omp_clauses=$(echo "$line" | sed 's/.*#pragma omp parallel for//')

            # print the header
            echo "$CODE_HEADER" >>"$OUTPUT_FILE"
            echo "#pragma omp for nowait $omp_clauses" >>"$OUTPUT_FILE"

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
        else
            echo "$line" >>"$OUTPUT_FILE"
            # if line contains array access, we want to insert the bump metric macro
            if [[ "$line" == *"]"* ]]; then
                # insert the bump metric macro
                echo "METRICS_IT_COUNT_BUMP();" >>"$OUTPUT_FILE"
            fi
        fi
    fi
done <"$INPUT_FILE"

# insert after the line "polybench_print_instruments();" the following code
# METRICS_PRINT_CLOCKS();
sed -i '/polybench_print_instruments;/a METRICS_PRINT_CLOCKS();' "$OUTPUT_FILE"
