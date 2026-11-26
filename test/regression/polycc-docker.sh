#!/usr/bin/env bash
# polycc from docker/pluto.dockerfile, for the probe's Pluto reference pipeline:
#   docker build -t pesto-pluto-ref:0.13.0 -f docker/pluto.dockerfile docker
#   cmake -B build -DPESTO_BENCH_POLYCC=$PWD/test/regression/polycc-docker.sh
# Mounts the current directory at the same path. Assumes rootless Docker (or
# root), where files written by the container belong to the calling user.
exec docker run --rm -v "$PWD:$PWD" -w "$PWD" \
    "${PESTO_PLUTO_IMAGE:-pesto-pluto-ref:0.13.0}" polycc "$@"
