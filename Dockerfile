# TIVIDY development and test environment.
# Pinned to Ubuntu 22.04 + GCC 11 + GoogleTest 1.12.1 (last C++11-compatible release).
# Usage:
#   docker build -t tividy-dev .
#   docker run --rm -v "$PWD":/app -u "$(id -u):$(id -g)" tividy-dev make test

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        g++ \
        make \
        cmake \
        git \
        wget \
        ca-certificates \
        doxygen \
        valgrind \
    && rm -rf /var/lib/apt/lists/*

# GoogleTest 1.12.1 — the last release that supports C++11
RUN wget -q https://github.com/google/googletest/archive/refs/tags/release-1.12.1.tar.gz -O /tmp/gtest.tar.gz && \
    mkdir -p /tmp/gtest && \
    tar -xzf /tmp/gtest.tar.gz -C /tmp/gtest --strip-components=1 && \
    cmake -S /tmp/gtest -B /tmp/gtest/build \
        -DCMAKE_CXX_STANDARD=11 \
        -DCMAKE_CXX_STANDARD_REQUIRED=ON \
        -DBUILD_GMOCK=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local && \
    cmake --build /tmp/gtest/build -j"$(nproc)" && \
    cmake --install /tmp/gtest/build && \
    rm -rf /tmp/gtest /tmp/gtest.tar.gz

WORKDIR /app

CMD ["make", "test"]