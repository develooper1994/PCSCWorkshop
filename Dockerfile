# Dockerfile for C++ CMake Development Environment
FROM ubuntu:24.04

# Avoid prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build tools
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    gcc-13 \
    g++-13 \
    clang-18 \
    clang-tools-18 \
    libclang-18-dev \
    lcov \
    git \
    python3 \
    python3-pip \
    doxygen \
    && rm -rf /var/lib/apt/lists/*

# Set GCC 13 and Clang 18 as defaults
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100 \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-18 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100

# Set working directory
WORKDIR /workspace

# Set default user (optional, for dev containers)
# USER developer

CMD ["/bin/bash"]
