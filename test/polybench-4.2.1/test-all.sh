#!/usr/bin/env bash

benchmark_list=(
    "datamining/correlation"
    "datamining/covariance"
    "linear-algebra/blas/gemm"
    "linear-algebra/blas/gemver"
    "linear-algebra/blas/gesummv"
    # "linear-algebra/blas/symm"
    "linear-algebra/blas/syr2k"
    "linear-algebra/blas/syrk"
    "linear-algebra/blas/trmm"
    "linear-algebra/kernels/2mm"
    "linear-algebra/kernels/3mm"
    "linear-algebra/kernels/atax"
    "linear-algebra/kernels/bicg"
    "linear-algebra/kernels/doitgen"
    "linear-algebra/kernels/mvt"
)

CHK_BIN="./tools/pb_chksum"

cleanup=0
variant="none"

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
    --cleanup) cleanup=1 ;;
    --metrics) variant="metrics" ;;
    --pluto) pluto=1 ;;
    --likwid) variant="likwid" ;;
    *)
        echo "Unknown parameter passed: $1"
        exit 1
        ;;
    esac
    shift
done

if [[ $pluto -ne 0 ]]; then
    suffix="pluto"
else
    suffix="pesto"
fi

if [[ $cleanup -ne 0 ]]; then
    echo "Force cleaning polybenchs"
    for bench in "${benchmark_list[@]}"; do
        make -C "$bench" clean
    done
fi

if [[ $variant != "none" ]]; then
    echo "# Building ${suffix} programs ($variant)"
else
    echo "# Building ${suffix} programs"
fi
for bench in "${benchmark_list[@]}"; do
    name=$(basename "$bench")
    if [[ $variant != "none" ]]; then
        bin=$(basename "${bench}_${suffix}_${variant}")
    else
        bin=$(basename "${bench}_${suffix}")
    fi
    printf "%-20s" "$name"
    if ! make -C "$bench" "${bin}" >/dev/null 2>&1; then
        echo "failed"
    else
        echo "ok"
    fi
done

if [[ $variant != "none" ]]; then
    echo "# Checking ($variant)"
else
    echo "# Checking..."
fi

for bench in "${benchmark_list[@]}"; do
    name=$(basename "$bench")
    logfile=".tmp"

    printf "%-20s" "$name"

    vanilla_src="${bench}/${name}.c"

    if [[ $variant != "none" ]]; then
        test_src="${bench}/${name}.${suffix}.${variant}.c"
    else
        test_src="${bench}/${name}.${suffix}.c"
    fi

    # run pb_chksum and get the second line of the output
    vanilla_hash=$($CHK_BIN "$vanilla_src" "-I../include/" 2>/dev/null | awk 'NR==1{print}')
    test_hash=$($CHK_BIN "$test_src" "-I../include/" 2>/dev/null | awk 'NR==1{print}')

    if [[ $vanilla_hash == "$test_hash" ]]; then
        echo "ok"
    else
        echo "failed"
        echo "  vanilla hash: $vanilla_hash"
        echo "  ${suffix} hash: $test_hash"
    fi

    rm -f $logfile
done
