#!/bin/bash
# Symbol and ABI check for staged S1.5.
#
#   * OFF must define no Goose export and must not export anything the base
#     build did not export;
#   * ON must define exactly one copy of each switched export and must export
#     only the bridge entry points on top of the base surface;
#   * the public C ABI (the llama.h surface) must be identical in all three.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT=${OUT:-$ROOT/build-gate}
mkdir -p "$OUT"
cd "$ROOT"

GOOSE_SYMS=(vt_dispatch stp_partition cs_strip)
BRIDGE_SYMS=(fg_goose_vocab_type_decide fg_goose_st_partition fg_goose_cs_strip
             fg_goose_tokenizer_calls fg_goose_tokenizer_pins gs_json_strtod)

echo "--- definitions of the switched Goose exports (all symbols, one object)"
for mode in off on; do
    for sym in "${GOOSE_SYMS[@]}"; do
        n=$(nm "build-$mode/bin/libllama.so" | grep -cE " [TtWw] ${sym}$" || true)
        echo "  build-$mode libllama.so  ${sym}: ${n}"
    done
    echo "  build-$mode libllama-common.so definitions:" \
         "$(nm "build-$mode/bin/libllama-common.so" | grep -cE ' [TtWw] (vt_dispatch|stp_partition|cs_strip)$' || true)"
    echo "  build-$mode libllama-common.so references to the bridge:" \
         "$(nm "build-$mode/bin/libllama-common.so" | grep -E '^ +U ' | grep -c 'fg_goose_' || true)"
done

echo "--- exported (dynamic) Goose names"
for mode in off on; do
    echo "  build-$mode: $(nm -D --defined-only "build-$mode/bin/libllama.so" \
        | grep -cE " (T|W) (${GOOSE_SYMS[0]}|${GOOSE_SYMS[1]}|${GOOSE_SYMS[2]})$" || true) module exports"
done

if [ ! -f build-base/bin/libllama.so ]; then
    echo "--- build-base/bin/libllama.so is absent: the ABI surface comparison"
    echo "    against the pristine base build is skipped (the definition checks"
    echo "    above do not depend on it)."
    exit 0
fi

echo "--- exported surface: base vs OFF vs ON (libllama.so)"
for mode in off on; do
    nm -D --defined-only build-base/bin/libllama.so | awk '{print $3}' | LC_ALL=C sort > "$OUT/base-libllama.txt"
    nm -D --defined-only "build-$mode/bin/libllama.so" | awk '{print $3}' | LC_ALL=C sort > "$OUT/$mode-libllama.txt"
    removed=$(LC_ALL=C comm -23 "$OUT/base-libllama.txt" "$OUT/$mode-libllama.txt" | wc -l)
    added_list=$(LC_ALL=C comm -13 "$OUT/base-libllama.txt" "$OUT/$mode-libllama.txt")
    added=$(printf '%s' "$added_list" | grep -c . || true)
    echo "  build-$mode: removed=$removed added=$added"
    if [ -n "$added_list" ]; then
        printf '    + %s\n' $added_list
    fi
done

echo "--- exported surface: base vs OFF vs ON (libllama-common.so)"
for mode in off on; do
    nm -D --defined-only build-base/bin/libllama-common.so | awk '{print $3}' | LC_ALL=C sort > "$OUT/base-libcommon.txt"
    nm -D --defined-only "build-$mode/bin/libllama-common.so" | awk '{print $3}' | LC_ALL=C sort > "$OUT/$mode-libcommon.txt"
    removed=$(LC_ALL=C comm -23 "$OUT/base-libcommon.txt" "$OUT/$mode-libcommon.txt" | wc -l)
    added_list=$(LC_ALL=C comm -13 "$OUT/base-libcommon.txt" "$OUT/$mode-libcommon.txt")
    added=$(printf '%s' "$added_list" | grep -c . || true)
    echo "  build-$mode: removed=$removed added=$added"
    if [ -n "$added_list" ]; then
        printf '    + %s\n' $added_list
    fi
done

echo "--- the switched public functions are still there, same mangled name"
for sym in string_strip llama_tokenize llama_vocab_type; do
    for mode in base off on; do
        lib=libllama.so
        [ "$sym" = string_strip ] && lib=libllama-common.so
        printf '  %-6s %-16s %s\n' "$mode" "$sym" \
            "$(nm -D --defined-only "build-$mode/bin/$lib" | grep -c "${sym}" || true)"
    done
done
