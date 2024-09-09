#!/usr/bin/env bash

# Clone subrepositories
git submodule init
git submodule update

# Download trahrhe

(
    rm -rf trahrhe
    mkdir trahrhe
    cd trahrhe || exit 1
    wget https://webpages.gitlabpages.inria.fr/trahrhe/trahrhe_packages/trahrhe-4.2.zip
    unzip trahrhe-4.2.zip
    rm trahrhe-4.2.zip
    make
)
