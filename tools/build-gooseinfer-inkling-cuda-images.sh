#!/usr/bin/env bash
set -euo pipefail

out=$(realpath -m -- "${1:?Usage: build-gooseinfer-inkling-cuda-images.sh OUTPUT_DIR}")
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
src="$root/integration/goose-standalone/universal/cuda/images"
shared="$root/integration/goose-standalone/universal/bonsai-frozen-cuda"
goose=${GOOSE_BIN:-goose}
cuda=${CUDA_HOME:-/usr/local/cuda}
scratch=$(mktemp -d)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p -- "$out"
cat "$src/inkling-online.goose" "$src/inkling-native-flash.goose" > "$scratch/inkling-native-flash.goose"
"$goose" --standalone -o "$scratch/inkling-native-flash.c" --header "$scratch/inkling-native-flash.h" \
    "$scratch/inkling-native-flash.goose"
python3 - "$scratch" <<'PY_FRAGMENT'
from pathlib import Path
import re
import sys

scratch = Path(sys.argv[1])
source = (scratch / "inkling-native-flash.c").read_text()
body = source.split("/* ---- prototypes ---- */", 1)[1].split("/* ---- host ---- */", 1)[0]
if "gs_sp" in body or "i2_online_adjust_key_g" not in body:
    raise ValueError("Unexpected Inkling online attention fragment")
types = source.split("/* ---- types ---- */", 1)[1].split("/* ---- data ---- */", 1)[0]
types = "\n".join(line for line in types.splitlines()
                  if not line.endswith(("gs_rhdr;", "gs_rref;", "gs_pref;", "sl_u8;")))
entry = re.search(r"static void i2_native_flash_g\([^\n]+\) \{ (gg_function_\d+)\(", body)
if entry is None:
    raise ValueError("Missing generated native attention device entry")
for function in ("i2_native_flash_g", entry[1]):
    body = body.replace(f"static void {function}(", f"static __attribute__((always_inline)) void {function}(")
body = body.replace("static GG_INLINE ", "static inline __attribute__((always_inline)) ")
(scratch / "inkling-native-flash-device.h").write_text(types + "\n" + body)
PY_FRAGMENT
for abi in "$src/"*-entries.json; do
    name=${abi##*/}
    python3 "$root/tools/emit-gooseinfer-cuda-entries.py" "$abi" "$scratch/${name%.json}.h"
done
"$goose" --standalone --emit-provenance "$scratch/inkling-epilogue.c" "$src/inkling-epilogue.goose"
python3 "$shared/frankenmoe-fragment.py" "$scratch" inkling-epilogue
python3 "$root/tools/emit-gooseinfer-cuda-ptx-primitives.py" \
    "$root/integration/goose-standalone/cuda-ptx-primitives.json" "$scratch"
bash "$root/tools/build-gooseinfer-cuda-memory.sh" "$scratch"
"${GOOSE_CUDA_DRIVER_BIN:?Set GOOSE_CUDA_DRIVER_BIN to KeelWing dev.5}" --standalone --emit-provenance \
    "$scratch/inkling-device-io.c" "$src/inkling-device-io.goose"
python3 "$shared/frankenmoe-fragment.py" "$scratch" inkling-device-io
while read -r header symbol entries prelude; do
    stem=${header%.h}
    device_source="$scratch/$stem.cl"
    options=(-Xclang -mlink-builtin-bitcode -Xclang "$cuda/nvvm/libdevice/libdevice.10.bc")
    frontend=(-march=sm_80 -cl-std=CL2.0 -nocudainc -Xclang -finclude-default-header)
    case "$stem" in
        flash-native-image) options+=(-I"$scratch") ;;
        flash-tiles-image)
            device_source="$scratch/flash-native-image.cl"
            options+=(-I"$scratch" -DGOOSE_INKLING_FLASH_TILES_IMAGE)
            ;;
        i2-parallel-conv-image|i2-probe-moe-image|attention-gate-image|situ-image|tanh-image)
            device_source="$scratch/$stem-entries.h"
            options+=(-I"$scratch"
                -include "$scratch/inkling-epilogue-primitives.h" -include "$scratch/inkling-epilogue-device.h")
            ;;
        attention-input-image)
            "$goose" --standalone --emit-provenance "$scratch/i2-attention-input.c" "$src/i2-attention-input.goose"
            python3 "$shared/frankenmoe-fragment.py" "$scratch" i2-attention-input
            options+=(-I"$scratch")
            ;;
        q8-packed-image)
            "$goose" --standalone --emit-provenance "$scratch/i2-q8-packed.c" "$src/i2-q8-packed.goose"
            python3 "$shared/frankenmoe-fragment.py" "$scratch" i2-q8-packed
            options+=(-I"$scratch")
            ;;
        grouped-moe-image)
            "$goose" --standalone --emit-provenance "$scratch/i2-grouped-moe.c" "$src/i2-grouped-moe.goose"
            python3 "$shared/frankenmoe-fragment.py" "$scratch" i2-grouped-moe
            options+=(-I"$scratch")
            ;;
        i2-lazy-prelude-image|bridge-image)
            if [[ "$stem" == bridge-image ]]; then
                device_source="$scratch/i2-lazy-prelude-image.cl"
                options=(-DGOOSE_INKLING_BRIDGE)
            fi
            "$goose" --standalone --emit-provenance "$scratch/i2-lazy-prelude.c" "$src/i2-lazy-prelude.goose"
            python3 "$shared/frankenmoe-fragment.py" "$scratch" i2-lazy-prelude
            options+=(-I"$scratch")
            ;;
        attention-image)
            "$goose" --standalone --emit-provenance "$scratch/inkling-attention.c" "$src/inkling-attention.goose"
            python3 "$shared/frankenmoe-fragment.py" "$scratch" inkling-attention
            frontend=(-Xclang -target-cpu -Xclang sm_80 -cl-std=CL1.2 -I"$scratch")
            ;;
        i2-decode-q8-image|matrix-image)
            "$goose" --standalone -o "$scratch/inkling-matrix.c" "$src/matrix-image.goose"
            (cd -- "$scratch" && python3 "$src/inkling-matrix-fragment.py")
            device_source="$scratch/matrix-image.cl"
            options=(-DGG_ALIGN1= -DGG_UNALIGNED= -I"$scratch")
            if [[ "$stem" == i2-decode-q8-image ]]; then options+=(-DGOOSE_INKLING_DECODE_MATRIX); fi
            ;;
    esac
    if [[ -n $prelude ]]; then options+=(-include "$src/$prelude"); fi
    assembler=(-arch=sm_89)
    if [[ -n $entries && $entries != - ]]; then assembler+=(--entry "$entries"); fi
    "${CLANG:-clang-18}" -target nvptx64-nvidia-cuda "${frontend[@]}" -x cl -O3 -ffp-contract=off \
        -nocudalib -I"$scratch" -include "$scratch/cuda-device-prelude.h" \
        -include "$scratch/typed-memory.h" -include "$scratch/typed-memory-bridge.h" \
        -include "$scratch/inkling-device-io.h" "${options[@]}" \
        -S "$device_source" -o "$scratch/$stem.ptx"
    "$cuda/bin/ptxas" "${assembler[@]}" "$scratch/$stem.ptx" -o "$scratch/$stem.cubin"
    python3 - "$scratch/$stem.cubin" "$out/$header" "$symbol" <<'PY'
from pathlib import Path
import sys

binary, header = map(Path, sys.argv[1:3])
data = binary.read_bytes()
with header.open("w") as stream:
    stream.write(f"unsigned char {sys.argv[3]}[] = {{\n")
    for offset in range(0, len(data), 24):
        stream.write(",".join(map(str, data[offset:offset + 24])) + ",\n")
    stream.write(f"}};\nunsigned int {sys.argv[3]}_len = {len(data)};\n")
PY
done < "$src/inkling-images.txt"
