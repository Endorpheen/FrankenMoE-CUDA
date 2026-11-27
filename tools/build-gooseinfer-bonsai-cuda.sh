#!/usr/bin/env bash
set -euo pipefail

target=${1:?Usage: build-gooseinfer-bonsai-cuda.sh linux|windows OUTPUT}
output=${2:?Missing output object path}
tools=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
case "$target" in
    linux) compiler=${CC:-gcc-13} ;;
    windows) compiler=${WINDOWS_CC:-x86_64-w64-mingw32-gcc} ;;
    *) printf 'Unknown GooseInfer target: %s\n' "$target" >&2; exit 2 ;;
esac
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT

bash "$tools/build-gooseinfer-bonsai-ptx.sh" "$scratch/frozen.ptx"
bash "$tools/build-gooseinfer-bonsai-mtp.sh" "$scratch/mtp.cubin"
bash "$tools/build-gooseinfer-bonsai-pipeline.sh" "$scratch/pipeline.cubin"
CC="$compiler" bash "$tools/build-gooseinfer-bonsai-images.sh" "$output" \
    "$scratch/pipeline.cubin" "$scratch/mtp.cubin" "$scratch/frozen.ptx"
