FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    bash cmake gcc g++ git make \
    pkgconf libtool autoconf automake\
    libyaml-dev libgmp3-dev \ 
    llvm-14 llvm-14-dev libclang-14-dev \
    bison libbison-dev libfl-dev mpc libmpc-dev &&\
    rm -rf /var/lib/apt/lists/*

ARG repository_dir

WORKDIR /pesto

ADD ${repository_dir} ./

RUN ln -s $(which FileCheck-14) /usr/local/bin/FileCheck
RUN ln -s $(which llvm-config-14) /usr/local/bin/llvm-config

RUN rm -rf build && mkdir -p build

RUN mkdir -p build && cd build && cmake .. && cmake --build . -j