# syntax=docker/dockerfile:1.7
# Linux aarch64 build environment for PixelForge.
# Pull with `--platform=linux/arm64` on an x86 host to run under QEMU.
FROM --platform=linux/arm64 debian:bookworm-slim

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential gcc g++ \
        cmake ninja-build git ca-certificates \
        python3 python3-dev python3-pytest python3-numpy \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# Use the dedicated arm64 preset (CMAKE_SYSTEM_PROCESSOR=aarch64).
CMD set -eux; \
    rm -rf build/linux-gcc-arm64; \
    cmake --preset linux-gcc-arm64; \
    cmake --build --preset linux-gcc-arm64; \
    ctest --preset linux-gcc-arm64 --output-on-failure
