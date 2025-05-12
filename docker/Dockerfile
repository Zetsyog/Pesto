FROM debian:bookworm-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    bash wget cmake gcc g++ git make \
    pkgconf libtool autoconf automake\
    libyaml-dev libgmp3-dev \ 
    bison libbison-dev libfl-dev mpc libmpc-dev \
    libntl-dev bzip2 texinfo &&\    
    rm -rf /var/lib/apt/lists/*

# install sbcl
RUN wget --quiet 'http://prdownloads.sourceforge.net/sbcl/sbcl-1.4.16-x86-64-linux-binary.tar.bz2' -O /tmp/sbcl.tar.bz2 &&\
    mkdir /sbcl &&\
    tar jxf /tmp/sbcl.tar.bz2 --strip-components=1 -C /sbcl &&\
    cd /sbcl &&\
    sh install.sh &&\
    rm -f /tmp/sbcl.tar.bz2

# install maxima
ENV maxima_build=5.47.0

WORKDIR /build
RUN git clone https://git.code.sf.net/p/maxima/code maxima-code --branch 5.47.0 --single-branch --depth 1 &&\
    cd maxima-code &&\
    mkdir dist &&\
    ./bootstrap &&\
    ./configure --enable-sbcl-exec --prefix=/usr --disable-build-docs && \
    make -j &&\
    make install

ARG repository_dir

WORKDIR /pesto

# Copy the repository to the container
ADD ${repository_dir} ./

# Cleanup the build directory (host might have built the project before)
RUN rm -rf build && mkdir -p build

ENV CC=gcc
ENV CXX=g++

# Build the project
RUN mkdir -p build && cd build && cmake .. && cmake --build . -j

# Set the environment variable
ENV PESTO=/pesto/build/pesto