#!/bin/sh
autoreconf -i
if test -f pluto/autogen.sh; then
    (
        cd pluto || exit 1
        git submodule update --init
        ./autogen.sh
    )
fi
