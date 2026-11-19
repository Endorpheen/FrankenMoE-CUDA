#!/usr/bin/env bash
# Answer CMAKE_CUDA_ARCHITECTURES from the Goose decision module, before CMake runs.
#
# P10-06's configure-time gap: CMake needs the architecture list at configure time, when the main
# program does not exist yet. The decision lives in goose/cuda/cuda-arch-policy.goose and needs a
# small thing that can run first - build/goose-hosts/arch, built by tools/goose-hosts/build.sh.
#
# This script detects the three inputs the decision needs (the CUDA version, the CMake version and
# whether a native list was requested), asks the host, and prints the CMake value. When the host is
# not built it falls back to the value the build used before this existed, and says so, so a
# checkout without the toolchain still configures.
#
# Usage: bash tools/cuda-arch.sh [--native]
set -uo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST=$ROOT/build/goose-hosts/arch
NATIVE=0
[ "${1:-}" = "--native" ] && NATIVE=1

cuda_major=0; cuda_minor=0
if command -v nvcc >/dev/null 2>&1; then
    v=$(nvcc --version 2>/dev/null | grep -oE 'release [0-9]+\.[0-9]+' | head -1 | awk '{print $2}')
    [ -n "${v:-}" ] && { cuda_major=${v%%.*}; cuda_minor=${v##*.}; }
fi
cmake_major=0; cmake_minor=0
if command -v cmake >/dev/null 2>&1; then
    v=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+' | head -1)
    [ -n "${v:-}" ] && { cmake_major=${v%%.*}; cmake_minor=${v##*.}; }
fi

if [ -x "$HOST" ]; then
    answer=$("$HOST" DEFAULT "${cuda_major:-0}" "${cuda_minor:-0}" "$NATIVE" "${cmake_major:-0}" "${cmake_minor:-0}")
    if [ -n "${answer:-}" ]; then
        printf '%s\n' "$answer"
        exit 0
    fi
fi

# No host: the list this project used before the decision moved to Goose.
printf '89\n'
printf 'cuda-arch: the Goose host is not built, falling back to 89\n' >&2
