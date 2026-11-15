#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}"
JOBS="${JOBS:-8}"

"${ROOT_DIR}/scripts/prepare_upstreams.sh"

cmake -S "${ROOT_DIR}/runtime" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC:-gcc-13}" \
    -DCMAKE_CXX_COMPILER="${CXX:-g++-13}" \
    -DCMAKE_CUDA_COMPILER="${CUDACXX:-/usr/bin/nvcc}" \
    -DCMAKE_CUDA_ARCHITECTURES="${CUDA_ARCH:-89}" \
    -DGGML_CUDA=ON \
    -DBMOE_BUILD_CLI=ON \
    -DBMOE_BUILD_TESTS=ON

cmake --build "${BUILD_DIR}" -j "${JOBS}"
