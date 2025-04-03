FROM debian:bookworm-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    bash wget cmake gcc g++ git make \
    pkgconf libtool autoconf automake\
    libyaml-dev libgmp3-dev \ 
    llvm-14 llvm-14-dev libclang-14-dev \
    bison libbison-dev libfl-dev mpc libmpc-dev \
    libntl-dev bzip2 texinfo sbcl  &&\    
    rm -rf /var/lib/apt/lists/*

# install barvinok-0.41.8
WORKDIR /build
RUN wget "https://barvinok.sourceforge.io/barvinok-0.41.8.tar.gz" -O barvinok-0.41.8.tar.gz &&\
    tar -xzvf barvinok-0.41.8.tar.gz &&\
    cd barvinok-0.41.8 &&\
    ./configure --prefix=/usr &&\
    make -j &&\
    make install &&\
    cd .. &&\
    rm barvinok-0.41.8.tar.gz


# install maxima
ENV maxima_build=5.47.0
RUN git clone https://git.code.sf.net/p/maxima/code maxima-code --branch 5.47.0 --single-branch --depth 1 &&\
    cd maxima-code &&\
    mkdir dist &&\
    ./bootstrap &&\
    ./configure --enable-sbcl-exec --prefix=/usr --disable-build-docs && \
    make -j &&\
    make install

ENV CC=gcc
ENV CXX=g++

WORKDIR /pesto
