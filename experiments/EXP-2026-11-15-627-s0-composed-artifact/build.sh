#!/usr/bin/env bash
# Step 1 toward one Goose GPU runtime: compose the five canonical sets, build one
# shared object per set, scope their exported symbols so the sets cannot collide,
# link them into a single unified artifact, and run an ABI-level smoke host.
#
# The production runtime is not touched: everything is written under this
# directory (build/ by default). No model, server or benchmark is started.
#
# Layout: build/src/<set>.goose, build/gen/<set>.{c,h,api,localize},
#         build/obj/*.o, build/lib/*.so, build/smoke, build/log/
# Usage: bash build.sh [clean]
# Environment: GOOSE (compiler), CC, FM_ROOT (canonical tree), OUT (build root)
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# The artifact is pinned to the assignation base commit. `build.sh pin <rev>`
# writes that tree to ./canon and every later run reads it, so a repository that
# moves on under the working copy cannot change the result. FM_ROOT overrides.
BASE_REV=${BASE_REV:-29a930e6}
if [ -n "${FM_ROOT:-}" ]; then
    ROOT=$FM_ROOT
elif [ -d "$HERE/canon/goose" ]; then
    ROOT=$HERE/canon
else
    ROOT=/home/end0/AI/FrankenMoE-CUDA
fi
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CC=${CC:-gcc}
OUT=${OUT:-$HERE/build}
OPT=-O2
# The quant port borrows ggml's primitives and IQ tables; they come from the
# reference tree, not from work/llama.cpp-integration.
LLAMA=${FG_LLAMA:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
MAKE_TABLES=${MAKE_TABLES:-$ROOT/experiments/EXP-2026-11-14-611-p7-04-quant-rows2/tests/make-tables.py}

if [ "${1:-}" = "pin" ]; then
    rev=${2:-$BASE_REV}
    src=${FM_REPO:-/home/end0/AI/FrankenMoE-CUDA}
    rm -rf "$HERE/canon"
    mkdir -p "$HERE/canon"
    git -C "$src" archive "$rev" | tar -x -C "$HERE/canon"
    echo "pinned $rev into $HERE/canon"
    exit 0
fi

if [ "${1:-}" = "clean" ]; then
    rm -rf "$OUT"
    echo "cleaned $OUT"
    exit 0
fi

[ -x "$GOOSE" ] || { echo "no Goose compiler at $GOOSE" >&2; exit 1; }
mkdir -p "$OUT"/{src,gen,obj,lib,log}

# name -> canonical modules, in the order tools/check-composition.sh uses.
SETS=(server quant gguf p9-06 jinja-schema)
declare -A MODULES=(
    [server]="goose/server/mcp-core.goose goose/server/mcp-stdio.goose goose/server/decision-core.goose goose/server/server-schema.goose goose/server/tools-pure.goose"
    [quant]="goose/model/quant-types.goose goose/model/quant-allow.goose goose/model/quant-rows.goose goose/model/quant-rows2.goose goose/model/quant-dequant.goose goose/model/quant-dequant2.goose"
    [gguf]="goose/model/gguf-metadata.goose goose/model/gguf-writer.goose goose/model/gguf-tensors.goose goose/model/gguf-split.goose"
    [p9-06]="goose/cli/cli-protocol.goose goose/chat/thinking-control.goose goose/chat/parse-params.goose"
    [jinja-schema]="goose/jinja/runtime.goose goose/json/format-grammar.goose"
)
# Shared ABI scoping ledger. goose_init is renamed per set so five runtimes can
# live in one process; every other export goes to the first set that declares
# it, and later sets keep their identical copy local. tools/compose-goose.py
# guarantees the bodies are identical, so one copy is enough at link time.
declare -A CLAIMED=()

API_RE='^(int64_t|void|double|float|uint64_t|int32_t|uint32_t|uint8_t|int8_t|int16_t|uint16_t|char)[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*\('

# The host-side hooks both json modules borrow (gs_json_strtod, gs_json_fmt_e).
# The canonical gates supply them from their C++ fixtures; one small library does
# it here, and every set resolves them at load time instead of leaving them open.
"$CC" "$OPT" -std=c11 -fPIC -c "$HERE/src/rt_hooks.c" -o "$OUT/obj/rt_hooks.o"
"$CC" -shared "$OUT/obj/rt_hooks.o" -o "$OUT/lib/libgoose_rt.so"

# The quant port borrows ggml's memory/math primitives and the IQ tables that
# native ggml_quantize_init builds. Same extraction script as the accepted gate.
[ -f "$LLAMA/ggml/src/ggml-quants.c" ] || {
    echo "missing $LLAMA/ggml/src/ggml-quants.c (set FG_LLAMA)" >&2; exit 1; }
python3 "$MAKE_TABLES" "$LLAMA/ggml/src/ggml-quants.c" "$OUT/gen/native-tables.c" \
    >"$OUT/log/native-tables.log" 2>&1
"$CC" "$OPT" -std=c11 -fPIC -w -I"$LLAMA/ggml/include" -I"$LLAMA/ggml/src" \
    -c "$OUT/gen/native-tables.c" -o "$OUT/obj/native-tables.o"
"$CC" "$OPT" -std=c11 -fPIC -w -c "$HERE/src/quant_prims.c" -o "$OUT/obj/quant_prims.o"
"$CC" -shared "$OUT/obj/quant_prims.o" "$OUT/obj/native-tables.o" -lm \
    -o "$OUT/lib/libgoose_quantprims.so"

echo "== 1/4 compose, generate C and build one shared object per set"
: >"$OUT/gen/inventory.tsv"

for set in "${SETS[@]}"; do
    log=$OUT/log/$set.build.log
    : >"$OUT/gen/$set.localize"
    : >"$OUT/gen/$set.scoped"
    mods=()
    for m in ${MODULES[$set]}; do mods+=("$ROOT/$m"); done
    {
        echo "== $set"
        # 1. compose the canonical modules into one translation unit
        python3 "$ROOT/tools/compose-goose.py" "$OUT/src/$set.goose" "${mods[@]}"
        # 2. Goose -> C plus the export header
        "$GOOSE" --standalone "$OPT" -o "$OUT/gen/$set.c" --header "$OUT/gen/$set.h" "$OUT/src/$set.goose"
        # 3. C -> PIC object
        "$CC" "$OPT" -std=c11 -fPIC -w -c "$OUT/gen/$set.c" -o "$OUT/obj/$set.o"
    } >"$log" 2>&1 || { echo "FAIL $set (see $log)"; tail -5 "$log"; exit 1; }

    # 4. exported API of this set, in header order
    grep -oE "$API_RE" "$OUT/gen/$set.h" \
        | sed -E 's/.*[[:space:]]+([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*\($/\1/' >"$OUT/gen/$set.api"

    while read -r sym; do
        [ -z "$sym" ] && continue
        if [ "$sym" = "goose_init" ]; then
            continue   # renamed per set below, stays exported
        fi
        if [ -n "${CLAIMED[$sym]:-}" ]; then
            echo "$sym" >>"$OUT/gen/$set.localize"    # identical copy: internal
            echo "$sym" >>"$OUT/gen/$set.scoped"
            CLAIMED[$sym]="${CLAIMED[$sym]},$set"
        else
            CLAIMED[$sym]="$set"
        fi
    done <"$OUT/gen/$set.api"

    # every global the header does not list is internal as well
    nm -g --defined-only --format=posix "$OUT/obj/$set.o" | awk '{print $1}' | sort -u \
        | grep -vxF -f "$OUT/gen/$set.api" >>"$OUT/gen/$set.localize" || true

    objcopy_args=(--redefine-sym "goose_init=goose_init_${set//-/_}")
    if [ -s "$OUT/gen/$set.localize" ]; then
        objcopy_args+=(--localize-symbols="$OUT/gen/$set.localize")
    fi
    objcopy "${objcopy_args[@]}" "$OUT/obj/$set.o" "$OUT/obj/$set.scoped.o"

    # 5. shared object that needs nothing but its declared hooks
    extra=()
    [ "$set" = quant ] && extra+=(-L"$OUT/lib" -lgoose_quantprims)
    "$CC" -shared "$OUT/obj/$set.scoped.o" \
        -L"$OUT/lib" -lgoose_rt "${extra[@]}" -Wl,-rpath,'$ORIGIN' -Wl,--no-undefined \
        -lm -lpthread -o "$OUT/lib/libgoose_$set.so"

    mods_n=$(grep -oE '^composed [0-9]+ module' "$log" | grep -oE '[0-9]+' | head -1)
    lines=$(wc -l <"$OUT/gen/$set.c")
    size=$(stat -c %s "$OUT/lib/libgoose_$set.so")
    exports=$(nm -D --defined-only --format=posix "$OUT/lib/libgoose_$set.so" | wc -l)
    printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$set" "$mods_n" "$lines" "$size" "$exports" \
        "$(tr '\n' ' ' <"$OUT/gen/$set.scoped" | sed 's/ *$//')" >>"$OUT/gen/inventory.tsv"
    echo "ok   $set: modules=$mods_n c_lines=$lines so_bytes=$size exports=$exports"
done

echo "== 2/4 duplicate-symbol audit"
# Raw (pre-scoping) duplicates: the copies the canonical tree ships.
for set in "${SETS[@]}"; do
    nm -g --defined-only --format=posix "$OUT/obj/$set.o" | awk -v s="$set" '{print $1"\t"s}'
done | sort >"$OUT/gen/raw-symbols.tsv"
awk -F'\t' '{c[$1]++; o[$1]=o[$1]" "$2}
             END {for (s in c) if (c[s] > 1) print s"\t"c[s]"\t"o[s]}' \
    "$OUT/gen/raw-symbols.tsv" | sort >"$OUT/gen/raw-duplicates.tsv"

# Scoped duplicates: what the shared objects actually export.
for set in "${SETS[@]}"; do
    nm -D --defined-only --format=posix "$OUT/lib/libgoose_$set.so" | awk -v s="$set" '{print $1"\t"s}'
done | sort >"$OUT/gen/dyn-symbols.tsv"
dupes=$(awk -F'\t' '{c[$1]++} END {n=0; for (s in c) if (c[s] > 1) n++; print n+0}' \
    "$OUT/gen/dyn-symbols.tsv")
rawdupes=$(wc -l <"$OUT/gen/raw-duplicates.tsv")
echo "scoped duplicate exported symbols: $dupes"
echo "raw duplicate definitions before scoping: $rawdupes"
# every shared object must resolve all of its own references
for set in "${SETS[@]}"; do
    if ldd -r "$OUT/lib/libgoose_$set.so" 2>&1 | grep -q "undefined symbol"; then
        echo "unresolved symbols in libgoose_$set.so:" >&2
        ldd -r "$OUT/lib/libgoose_$set.so" 2>&1 | grep "undefined symbol" >&2
        exit 1
    fi
done
echo "resolved: every set .so has no undefined symbol (ldd -r)"

echo "== 3/4 one unified artifact"
"$CC" -shared "$OUT"/obj/*.scoped.o "$OUT/obj/rt_hooks.o" "$OUT/obj/quant_prims.o" \
    "$OUT/obj/native-tables.o" -Wl,--no-undefined -lm -lpthread \
    -o "$OUT/lib/libgoose_unified.so"
unified=$(nm -D --defined-only --format=posix "$OUT/lib/libgoose_unified.so" | wc -l)
echo "unified exports=$unified unified_bytes=$(stat -c %s "$OUT/lib/libgoose_unified.so")"

echo "== 4/4 smoke host"
"$CC" "$OPT" -std=c11 -w -I"$OUT/gen" -DSMOKE_LIBDIR="\"$OUT/lib\"" \
    -c "$HERE/src/smoke_main.c" -o "$OUT/obj/smoke_main.o"
for set in "${SETS[@]}"; do
    "$CC" "$OPT" -std=c11 -w -I"$OUT/gen" -c "$HERE/src/smoke_$set.c" -o "$OUT/obj/smoke_$set.o"
done
"$CC" "$OUT"/obj/smoke_*.o -L"$OUT/lib" -lgoose_unified -Wl,-rpath,"$OUT/lib" \
    -ldl -o "$OUT/smoke"

set +e
"$OUT/smoke" 2>&1 | tee "$OUT/log/smoke.log"
rc=${PIPESTATUS[0]}
"$OUT/smoke" --dlopen 2>&1 | tee "$OUT/log/dlopen.log"
drc=${PIPESTATUS[0]}
set -e
echo "smoke rc=$rc dlopen rc=$drc"
if [ "$rc" -ne 0 ] || [ "$drc" -ne 0 ] || [ "$dupes" -ne 0 ]; then
    exit 1
fi

echo "== artifacts"
(cd "$OUT/lib" && sha256sum ./*.so) | tee "$OUT/log/SHA256SUMS"

{
    printf 'set\tmodules\tc_lines\tso_bytes\texports\tscoped_shared\n'
    cat "$OUT/gen/inventory.tsv"
} >"$OUT/inventory.tsv"
echo "done"
