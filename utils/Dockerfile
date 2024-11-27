FROM debian:bookworm-slim

# Install dependencies
RUN apt-get update && apt-get install -y \
    bash wget cmake gcc g++ git make \
    pkgconf libtool autoconf automake\
    libyaml-dev libgmp3-dev \ 
    llvm-14 llvm-14-dev libclang-14-dev \
    bison libbison-dev libfl-dev mpc libmpc-dev \
    libntl-dev maxima &&\    
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

ARG repository_dir

WORKDIR /pesto

# Copy the repository to the container
ADD ${repository_dir} ./

# Create a symbolic link to the clang-14 and llvm-14 executables
RUN ln -s $(which FileCheck-14) /usr/local/bin/FileCheck
RUN ln -s $(which llvm-config-14) /usr/local/bin/llvm-config

# Cleanup the build directory (host might have built the project before)
RUN rm -rf build && mkdir -p build

# Build the project
RUN mkdir -p build && cd build && cmake .. && cmake --build . -j

# Set the environment variable
ENV PESTO=/pesto/build/pesto