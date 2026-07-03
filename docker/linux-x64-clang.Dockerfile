# syntax=docker/dockerfile:1.7
# Linux x86_64 / Clang 18 build environment for PixelForge.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        clang-18 lld-18 libc++-18-dev libc++abi-18-dev \
        cmake ninja-build git ca-certificates \
        python3 python3-dev python3-pip python3-venv \
        python3-pytest python3-numpy \
    && update-alternatives --install /usr/bin/clang   clang   /usr/bin/clang-18   100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-18 100 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

CMD set -eux; \
    rm -rf build/linux-clang; \
    cmake --preset linux-clang; \
    cmake --build --preset linux-clang; \
    ctest --preset linux-clang --output-on-failure
