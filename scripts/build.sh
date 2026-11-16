#!/usr/bin/env bash
# Configure and build the GPU runtime.
#
# The build-environment decisions — the BUILD_DIR/JOBS defaults, the configure
# vector, the build type and the subdirectory mask — are the P10-05 S2 slice,
# `goose/misc/build-env.goose`, reached through `build/goose-hosts/env`
# (`bash tools/goose-hosts/build.sh`). The script keeps the filesystem work and
# the cmake invocation. BMOE_GOOSE_ENV=0 restores the inline decisions.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=../tools/goose-hosts/client.sh
. "${ROOT_DIR}/tools/goose-hosts/client.sh"

"${ROOT_DIR}/scripts/prepare_upstreams.sh"

if [ -n "${BUILD_DIR+x}" ] && [ -n "${BUILD_DIR}" ]; then bd_set=1; else bd_set=0; fi
if [ -n "${JOBS+x}" ] && [ -n "${JOBS}" ]; then jobs_set=1; else jobs_set=0; fi

if goosehost_init "${ROOT_DIR}" env BMOE_GOOSE_ENV; then
    BUILD_DIR=$(goosehost_raw "$(goosehost_ask "DIR $(goosehost_hex "$ROOT_DIR") ${bd_set} $(goosehost_hex "${BUILD_DIR:-}")")")
    JOBS=$(goosehost_raw "$(goosehost_ask "JOBS ${jobs_set} $(goosehost_hex "${JOBS:-}")")")
    # The vector is the script's own order, one token per line: -S <root>/runtime
    # -B <BUILD_DIR> -G Ninja -DCMAKE_BUILD_TYPE=Release, the four overrides with
    # their defaults, then the three BMOE_/GGML switches.
    mapfile -t CONFIGURE < <(goosehost_raw "$(goosehost_ask "ARGS $(goosehost_hex "$ROOT_DIR") $(goosehost_hex "$BUILD_DIR") \
        1 $(goosehost_hex "${CC:-}") 1 $(goosehost_hex "${CXX:-}") 1 $(goosehost_hex "${CUDACXX:-}") 1 $(goosehost_hex "${CUDA_ARCH:-}")")")
    cmake "${CONFIGURE[@]}"
else
    BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/franken-cuda}"
    JOBS="${JOBS:-8}"
    cmake -S "${ROOT_DIR}/runtime" -B "${BUILD_DIR}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="${CC:-gcc-13}" \
        -DCMAKE_CXX_COMPILER="${CXX:-g++-13}" \
        -DCMAKE_CUDA_COMPILER="${CUDACXX:-/usr/bin/nvcc}" \
        -DCMAKE_CUDA_ARCHITECTURES="${CUDA_ARCH:-89}" \
        -DGGML_CUDA=ON \
        -DBMOE_BUILD_CLI=ON \
        -DBMOE_BUILD_TESTS=ON
fi

cmake --build "${BUILD_DIR}" -j "${JOBS}"
