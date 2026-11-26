FROM ubuntu:25.10

# Install dependencies
RUN apt-get update && apt-get install -y \
    bash wget cmake gcc-14 g++-14 git make \
    pkgconf libtool libtool-bin autoconf automake \
    libyaml-dev libgmp3-dev \ 
    bison libbison-dev libfl-dev mpc libmpc-dev \
    libntl-dev bzip2 texinfo libglpk-dev \  
    llvm-14-dev libclang-14-dev llvm-14 &&\  
    rm -rf /var/lib/apt/lists/*

# create llvm aliases for pluto to work
RUN ln -s /usr/bin/FileCheck-14 /usr/bin/FileCheck
RUN ln -s /usr/bin/llvm-config-14 /usr/bin/llvm-config

WORKDIR /build
RUN git clone https://github.com/bondhugula/pluto.git pluto
WORKDIR /build/pluto
RUN git checkout 0.13.0 &&\
    git submodule update --init 
RUN ./autogen.sh
RUN CC=gcc-14 CXX=g++-14 ./configure
# bypass cloog doc gen
RUN touch cloog-isl/doc/cloog.pdf
RUN make -j &&\
    make install

