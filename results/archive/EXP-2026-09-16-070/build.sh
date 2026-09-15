#!/bin/bash
# EXP-2026-09-16-070: build the model-free three-arm transfer harness in its own build dir.
# The CUDA 12.6 nvcc cannot compile against glibc 2.43, so this harness uses the Driver API and
# is built with a plain host compiler. No device code is needed for a memcpy-only workload.
set -euo pipefail
cd "$(dirname "$0")"
ROOT=../../..
BUILD="$ROOT/build/exp070-transfer3"
mkdir -p "$BUILD"
CUDA_INC=/usr/local/cuda-12.6/include
CUDA_LIB=/usr/lib/x86_64-linux-gnu
g++-13 -O2 -std=c++17 -Wall -Wextra -o "$BUILD/exp070_transfer3" exp070_transfer3.cpp \
    -I"$CUDA_INC" -L"$CUDA_LIB" -lcuda -pthread
g++-13 -O2 -std=c++17 -Wall -Wextra -o "$BUILD/exp070_pinprobe" exp070_pinprobe.cpp \
    -I"$CUDA_INC" -L"$CUDA_LIB" -lcuda
echo "built: $BUILD/exp070_transfer3, $BUILD/exp070_pinprobe"
