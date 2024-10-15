#!/usr/bin/env bash

benchmark_list=(
    "datamining/correlation"
    "datamining/covariance"
    "linear-algebra/blas/gemm"
    "linear-algebra/blas/gemver"
    "linear-algebra/blas/gesummv"
    # "linear-algebra/blas/symm"
    # "linear-algebra/blas/syr2k"
    # "linear-algebra/blas/syrk"
    "linear-algebra/blas/trmm"
    "linear-algebra/kernels/2mm"
    "linear-algebra/kernels/3mm"
    "linear-algebra/kernels/atax"
    "linear-algebra/kernels/bicg"
    "linear-algebra/kernels/doitgen"
    "linear-algebra/kernels/mvt"
)

cleanup=0

# Parse command-line arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
    --cleanup) cleanup=1 ;;
    *)
        echo "Unknown parameter passed: $1"
        exit 1
        ;;
    esac
    shift
done

if [[ $cleanup -ne 0 ]]; then
    echo "Force cleaning polybenchs"
    for bench in "${benchmark_list[@]}"; do
        make -C "$bench" clean
    done
fi

echo "# Building pesto programs"
for bench in "${benchmark_list[@]}"; do
    name=$(basename "$bench")
    printf "%-20s" "$name"
    if ! make -C "$bench" "${name}_pesto" >/dev/null 2>&1; then
        echo "failed"
    else
        echo "ok"
    fi
done

echo "# Checking..."
for bench in "${benchmark_list[@]}"; do
    name=$(basename "$bench")
    logfile=".tmp"

    printf "%-20s" "$name"

    if ! make -C "$bench" check >"$logfile" 2>&1; then
        echo "failed"
        rm -f $logfile
        continue
    fi

    vanilla_hash=$(awk "/${name}.c/{getline; print}" "$logfile" | cut -d' ' -f2)
    pesto_hash=$(awk "/${name}.pesto.c/{getline; print}" "$logfile" | cut -d' ' -f2)

    if [[ $vanilla_hash == $pesto_hash ]]; then
        echo "ok"
    else
        echo "failed"
        echo "  Vanilla hash: $vanilla_hash"
        echo "  Pesto hash: $pesto_hash"
    fi

    rm -f $logfile
done
