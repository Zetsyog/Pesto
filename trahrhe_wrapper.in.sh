#!/usr/bin/env bash

TRAHRHE_DIR="@TRAHRHE_INSTALL_DIR@"
BARVINOK_DIR="/home/cross/code/repo/barvinok-dev"

# usage: ./trahrhe_wrapper.sh <output_path> [trahrhe options]
output_path="$1"
shift

echo $TRAHRHE_DIR

sed -i "1s|^#\!.*|#\!/usr/bin/env bash|" "$TRAHRHE_DIR/trahrhe"

(
    cd $TRAHRHE_DIR || (echo "Could not cd to $TRAHRHE_DIR" && exit 1)
    export PATH="$BARVINOK_DIR:$PATH"
    # shellcheck disable=SC2068
    echo "./trahrhe" "$@"
    ./trahrhe "$@"
)

# remove the TILEMIN function (fix for multiple headers)
sed -i "/\* TILEMIN \*/,/end TILEMIN/d" "$TRAHRHE_DIR/trahrhe_tiling_header.h"

# Move the output to the desired location
mv "$TRAHRHE_DIR/trahrhe_tiling_header.h" "$output_path" || echo "Could not move trahrhe_tiling_header.h to $output_path"

# [_PB_N, _PB_M] -> { [t0, t1, t2] : _PB_M > 0 and t0 < _PB_N and 0 <= t1 <= t0 and 0 <= t2 < _PB_M }
