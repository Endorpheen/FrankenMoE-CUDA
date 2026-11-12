#!/bin/sh
# Reproduce the P2-01 contract probe and compare it against the accepted Goose build.
#
# Two arms:
#   goose  - the accepted library set (the full staging directory by default)
#
# Why the full staging set and not the four-file checkpoint: libggml-cpu references fg160_* and those are defined by the
# libggml-base-* variant libraries that only exist in staging. The checkpoint's four sonames are enough to run the server,
# not enough to link this probe.
#   native - an untouched flint build with no Goose symbols
#
# Usage: ./build-and-compare.sh [goose_lib_dir] [native_lib_dir]
# Defaults are the preserved checkpoint and the flint build tree.

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../.." && pwd)
FLINT=${FLINT:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}

GOOSE=${1:-$REPO/runtime/reconstructed/build/stage-run}
NATIVE=${2:-$FLINT/build-cuda/bin}

OUT=$(mktemp -d)
CC=${CC:-gcc-13}

# --allow-shlib-undefined is required: the handed libraries resolve each other's symbols at run time through their own
# search path, which the static linker does not follow. Without the flag the link fails on fg160_* even though those
# symbols resolve and execute fine in the real process (FG160_SHARED appears three times in the accepted run log).
build() {
    lib=$1; exe=$2
    $CC -O2 -I"$FLINT/ggml/include" "$HERE/fixture.c" -o "$exe" \
        -L"$lib" -Wl,--allow-shlib-undefined \
        -l:libggml-cpu.so.0.27.0 -l:libggml-base.so.0.27.0 -lm -lpthread
}

# The loaders ask for the SONAME (.so.0) while the checkpoint stores versioned files only, so give each arm a small
# directory with the symlinks the dynamic loader expects.
stage_links() {
    lib=$1; dir=$2
    mkdir -p "$dir"
    for f in "$lib"/libggml-*.so.0.27.0; do
        [ -e "$f" ] || continue
        base=$(basename "$f")
        soname=$(echo "$base" | sed 's/\.so\.0\.27\.0$/.so.0/')
        ln -sf "$f" "$dir/$soname"
        ln -sf "$f" "$dir/$(echo "$base" | sed 's/\.0\.27\.0$//')"
    done
}

build "$GOOSE"  "$OUT/probe_goose"
build "$NATIVE" "$OUT/probe_native"
stage_links "$GOOSE"  "$OUT/lib_goose"
stage_links "$NATIVE" "$OUT/lib_native"

# libggml-cpu references fg160_*, which the libggml-base-* variant libraries define; the server gets them through its own
# dynamic loading, so for a standalone probe the first defining variant is preloaded explicitly.
PRELOAD=""
for f in "$GOOSE"/libggml-base-*.so.0.27.0; do
    [ -e "$f" ] || continue
    if nm -D --defined-only "$f" 2>/dev/null | grep -q 'fg160_fusion_enabled'; then PRELOAD=$f; break; fi
done

LD_PRELOAD="$PRELOAD" LD_LIBRARY_PATH="$OUT/lib_goose"  "$OUT/probe_goose"  "$OUT/goose.bin"  >/dev/null
LD_LIBRARY_PATH="$OUT/lib_native" "$OUT/probe_native" "$OUT/native.bin" >/dev/null

echo "goose arm : $GOOSE"
echo "native arm: $NATIVE"
echo

python3 - "$OUT/goose.bin" "$OUT/native.bin" <<'PY'
import sys
from pathlib import Path

def parse(p):
    b = Path(p).read_bytes(); i = 0; out = {}
    while i < len(b):
        j = b.index(b'\n', i)
        name, n = b[i:j].split(); n = int(n); i = j + 1
        out[name.decode()] = b[i:i + 4 * n]; i += 4 * n
    return out

g, n = parse(sys.argv[1]), parse(sys.argv[2])
if set(g) != set(n):
    print("case sets differ between arms"); sys.exit(1)
diff = [k for k in g if g[k] != n.get(k)]
print(f"cases: {len(g)}")
print(f"bit-identical: {len(g) - len(diff)}")
print("differing:")
for k in diff:
    print("  " + k)
# The known findings, so a new one cannot slip in unnoticed.
expected = {"silu_t1", "silu_t8", "silu_1000", "clamp_t1", "clamp_t8", "scale_t1", "scale_t8",
            "norm_finite", "softmax_finite", "mul_mat_32x8x6", "rope_16x2x4"}
extra = sorted(set(diff) - expected)
missing = sorted(expected - set(diff))
if extra:
    print("NEW divergences not in the recorded set: " + ", ".join(extra))
if missing:
    print("recorded divergences no longer reproducing: " + ", ".join(missing))
sys.exit(0 if not extra and not missing else 2)
PY

rc=$?
echo
echo "artifacts: $OUT"
exit $rc
