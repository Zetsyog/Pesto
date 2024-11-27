FROM debian:bookworm-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    bash wget cmake gcc g++ git make \
    pkgconf libtool autoconf automake\
    libyaml-dev libgmp3-dev \ 
    llvm-14 llvm-14-dev libclang-14-dev \
    bison libbison-dev libfl-dev mpc libmpc-dev \
    libntl-dev bzip2 texinfo &&\    
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
    rm -rf barvinok-0.41.8/ barvinok-0.41.8.tar.gz

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
RUN git clone https://git.code.sf.net/p/maxima/code maxima-code --branch 5.47.0 --single-branch &&\
    cd maxima-code &&\
    mkdir dist &&\
    ./bootstrap &&\
    ./configure --enable-sbcl-exec --prefix=/usr && \
    make -j &&\
    make install

ARG repository_dir

WORKDIR /pesto

# Copy the repository to the container
ADD ${repository_dir} ./

# Create a symbolic link to the clang-14 and llvm-14 executables
RUN ln -s $(which FileCheck-14) /usr/local/bin/FileCheck
RUN ln -s $(which llvm-config-14) /usr/local/bin/llvm-config

# Cleanup the build directory (host might have built the project before)
RUN rm -rf build && mkdir -p build

# Cleanup the trahrhe directory that might have been built by host
RUN cd trahrhe-dev && make clean && cd ..

# Build the project
RUN mkdir -p build && cd build && cmake .. && cmake --build . -j

# Set the environment variable
ENV PESTO=/pesto/build/pesto