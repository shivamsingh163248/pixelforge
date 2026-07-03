# syntax=docker/dockerfile:1.7
# Linux x86_64 / GCC 13 build environment for PixelForge.
# Produces a builder image; running it builds + tests the project.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential gcc-13 g++-13 \
        cmake ninja-build git ca-certificates \
        python3 python3-dev python3-pip python3-venv \
        python3-pytest python3-numpy \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# Default command: configure, build, run all tests.
CMD set -eux; \
    rm -rf build/linux-gcc; \
    cmake --preset linux-gcc; \
    cmake --build --preset linux-gcc; \
    ctest --preset linux-gcc --output-on-failure
