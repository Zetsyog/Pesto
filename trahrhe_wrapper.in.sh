#!/usr/bin/env bash

TRAHRHE_DIR="@TRAHRHE_DIR@"
USE_LEGACY="@TRAHRHE_IS_LEGACY@"
VERSION="@PESTO_VERSION@"

# wrapper around trahrhe software
# This script is used to run trahrhe with the given arguments and move the output to the desired location
# This script can handle multiple trahrhe version
# The legacy versions are version 4.2 and below
# The new versions are version 5.0 and above
usage() {
    echo -e "Usage: $0 [options]"
    echo -e "Options:"
    echo -e "\t-o,--output <output_path>  Path to the output file"
    echo -e "\t-d,--domain <isl_domain>   Domain of the input in isl format"
    echo -e "\t-t,--tiling-depth <depth>  Set the number of dimension to tile"
    echo -e "\t-e,--generate-header       Generate a c header file containing the trahrhe expresions"
    echo -e "\t-m,--tiling-mask <mask>    [LEGACY ONLY] Set the tiling type for each dimension"
    echo -e "\t--no-dichotomy             Do not use dichotomy algorithm and use exact trahrhe expression resolution"
    echo -e "\t-h,--help                  Print this help message"
    echo -e "\t-v,--version               Print the version of trahrhe"
    exit 1
}

# Parse the arguments
# Initialize variables
output_path=""
isl_domain=""
tiling_depth=""
generate_header=false
tiling_mask="d"
no_dichotomy=false

# Use getopt for argument parsing
TEMP=$(getopt -o o:d:t:em:hv --long output:,domain:,tiling-depth:,generate-header,tiling-mask:,no-dichotomy,help,version -n "$0" -- "$@")
if [ $? != 0 ]; then
    echo "Error parsing arguments." >&2
    usage
fi

# Note the quotes around `$TEMP`: they are essential!
eval set -- "$TEMP"

# Parse arguments
while true; do
    case "$1" in
    -o | --output)
        output_path="$2"
        shift 2
        ;;
    -d | --domain)
        isl_domain="$2"
        shift 2
        ;;
    -t | --tiling-depth)
        tiling_depth="$2"
        shift 2
        ;;
    -e | --generate-header)
        generate_header=true
        shift
        ;;
    -m | --tiling-mask)
        tiling_mask="$2"
        shift 2
        ;;
    --no-dichotomy)
        no_dichotomy=true
        shift
        ;;
    -h | --help)
        usage
        ;;
    -v | --version)
        echo "Pesto trahrhe wrapper $VERSION"
        exit 0
        ;;
    --)
        shift
        break
        ;;
    *)
        echo "Unknown option: $1"
        usage
        ;;
    esac
done

# Validate required arguments
if [[ -z "$output_path" ]]; then
    echo "Error: Output path is required."
    usage
fi

if [[ -z "$isl_domain" ]]; then
    echo "Error: Domain is required."
    usage
fi

echo $TRAHRHE_DIR
if [ "$USE_LEGACY" = "true" ]; then
    echo "Using legacy trahrhe"
    CMD=("$TRAHRHE_DIR/trahrhe" "-d" "$isl_domain")
    if [ -n "$tiling_depth" ]; then
        CMD+=("-t" "$tiling_depth")
    fi
    if [ "$generate_header" = true ]; then
        CMD+=("-e")
    fi
    # if tiling mask is not set but no dichotomy is set, set tiling mask to "r"
    if [ "$no_dichotomy" = true ]; then
        if [ -z "$tiling_mask" ]; then
            tiling_mask="r"
        fi
    fi

    if [ -n "$tiling_mask" ]; then
        if [ "$no_dichotomy" = true ]; then
            echo "Warning: --no-dichotomy is ignored when tiling mask is set"
        fi
        CMD+=("-m" "$tiling_mask")
    fi

    # patch legacy shebang
    sed -i "1s|^#\!.*|#\!/usr/bin/env bash|" "$TRAHRHE_DIR/trahrhe"
    (
        cd $TRAHRHE_DIR || (echo "Could not cd to $TRAHRHE_DIR" && exit 1)
        echo "${CMD[@]}"
        if ! "${CMD[@]}"; then
            echo "Error running trahrhe"
            exit 1
        fi
    )

    if [ -n "$tiling_depth" ]; then
        header_path="${TRAHRHE_DIR}/trahrhe_tiling_header.h"
        # remove the TILEMIN function (fix for multiple headers)
        sed -i "/\* TILEMIN \*/,/end TILEMIN/d" "$header_path"
    else
        # remove the TILEMIN function (fix for multiple headers)
        header_path="${TRAHRHE_DIR}/trahrhe_header.h"
    fi

    if [ "$output_path" != "/dev/null" ]; then
        # Move the output to the desired location
        cp "$header_path" "$output_path" || echo "Could not move ${header_path} to ${output_path}"
    fi
    rm -f "$header_path"
else
    echo "Using new trahrhe"
    CMD=("$TRAHRHE_DIR/trahrhe" "--domain" "$isl_domain" "-o" "$output_path")
    if [ -n "$tiling_depth" ]; then
        CMD+=("--tile" "$tiling_depth")
    fi
    if [ "$generate_header" = true ]; then
        CMD+=("--header")
    fi
    if [ "$no_dichotomy" = true ]; then
        CMD+=("--no-dichotomous")
    fi

    echo "${CMD[@]}"
    if ! "${CMD[@]}"; then
        echo "Error running trahrhe"
        exit 1
    fi
fi
