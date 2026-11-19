#!/bin/bash
# Build the standalone FrankenMoE runtime: the composed Goose unit, the host
# adapter and nothing else.
#
# This is the direct path P11 asks for: the shipped executable must be the Goose
# runtime itself, not llama.cpp with Goose bolted on. The three inputs are
#
#   build/goose-main/main.{c,h}   the composed unit (tools/build-main-runtime.sh)
#   integration/m1/host/goose_host.c  the host primitives (fp16, bit ops, JSON
#                                  helpers, the one pthread_once)
#   integration/goose-standalone/  per-stage header shims, the v4 host, the bindings
#
# The link does NOT need libllama. It does need libggml-base for the graph and
# tensor substrate, and libggml-base.a contains C++ objects, so libstdc++ and
# libgomp arrive with it - see the note at the link step below. Our own sources are
# C. ldd on the result shows libstdc++, libm, libgomp, libgcc_s, libc; nm shows two
# defined C++ symbols, both gguf_write_to_buf from libggml-base, and no libllama.
#
# Details and the measurements: docs/goose-migration/STANDALONE-DEPS.md.
#
# Generated C is not committed; the unit is built here. The shims are committed.
#
# Usage: bash tools/build-standalone-runtime.sh [--check]
#   --check  build, run the graph gate and v4, fail on a mismatch
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT=$ROOT/builds/goose-standalone
CC=${CC:-gcc-13}
CXX=${CXX:-g++-13}
# OFF ggml libraries: pure substrate, no carrier bridges, so exactly one
# goose_init can exist in the process.
SUB=${FG_SUBSTRATE:-/tmp/opencode/sub-661/tree/build-off/llama.cpp/ggml/src}
# ggml headers are read-only inputs. In a sparse worktree `upstream/` can be absent
# (it is git-ignored), so fall back to the main checkout when it is not here.
INC=$ROOT/upstream/llama.cpp/ggml/include
if [ ! -f "$INC/gguf.h" ]; then
    INC=/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp/ggml/include
fi
if [ ! -f "$INC/gguf.h" ]; then
    echo "ggml headers not found: neither $ROOT/upstream nor the main checkout has them" >&2
    exit 1
fi
# One compiler identity, read from the record the gates also use. A hardcoded path
# here let the build and the gates name different compilers while both looked
# authoritative; the artifact now records which one actually built it. The record
# wins; the pinned path is only a fallback when the record is absent.
if [ -z "${GOOSE:-}" ]; then
    GOOSE=$(python3 -c "import json;print(json.load(open('$ROOT/runtime/compiler/current.json'))['compiler'])" 2>/dev/null || true)
fi
GOOSE=${GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
CHECK=0
[ "${1:-}" = "--check" ] && CHECK=1

mkdir -p "$OUT/inc"

echo "[1/5] compose the unit"
# Rebuild when the composed unit is missing OR stale. The old guard only checked
# existence, so adding a module left main.c untouched and the standalone linked the
# PREVIOUS unit - EXP978 lost a full build to that before noticing hl_open was absent
# from the artifact. Compare the module list and the sources against the timestamp.
_need_unit=0
if [ ! -f "$ROOT/build/goose-main/main.c" ]; then
    _need_unit=1
else
    # By content, not by date. Timestamps cannot see a unit that was copied into
    # place: build/goose-main held a listener without hl_serve_full while every
    # date on it was newer than the sources. The composer writes the hash of what
    # it read, and the comparison is over the same inputs.
    _want=$(cd "$ROOT" && { sed -n '1,400p' tools/build-main-runtime.sh; \
        find goose -name '*.goose' -print0 | sort -z | xargs -0 sha256sum; } \
        | sha256sum | cut -d' ' -f1)
    _have=$(cat "$ROOT/build/goose-main/.source-sha256" 2>/dev/null || true)
    if [ "$_want" != "$_have" ]; then _need_unit=1; fi
    if [ "$ROOT/tools/build-main-runtime.sh" -nt "$ROOT/build/goose-main/main.c" ]; then _need_unit=1; fi
fi
if [ "$_need_unit" -eq 1 ]; then
    printf '  unit:   stale or missing, rebuilding\n'
    bash "$ROOT/tools/build-main-runtime.sh" "$ROOT/build/goose-main"
else
    printf '  unit:   up to date\n'
fi

echo "[2/5] stage the unit header and the per-stage shims"
cp -f "$ROOT/build/goose-main/main.h" "$OUT/inc/main.h"
for h in goose-unit config-slice gguf-stack json-schema-grammar json \
         metrics-traces models quant-stack session spec-decide \
         stream-policy tokenizer-decisions; do
    printf '#pragma once\n// One composed unit, so every per-stage header is a forward to it.\n#include "main.h"\n' \
        > "$OUT/inc/$h.h"
done

echo "[3/5] compile the unit and the adapter"
$CC -O1 -std=c11 -w -c "$ROOT/build/goose-main/main.c" -o "$OUT/main.o"
$CC -O1 -std=c11 -w -I "$ROOT/integration/m1/host" -I "$OUT/inc" \
    -c "$ROOT/integration/m1/host/goose_host.c" -o "$OUT/host.o"

echo "[4b/5] v4: metadata, tensor directory and payload"
# One host covers what v2 and v3 did separately; the superseded copies are gone,
# because a superseded adapter is pure debt (AGENTS.md).
if [ -d "$SUB" ] && [ -f "$ROOT/integration/goose-standalone/host_v4.c" ]; then
    $CC -O1 -std=c11 -w -I "$OUT/inc" -I "$INC" \
        -I "$ROOT/integration/goose-standalone" \
        -c "$ROOT/integration/goose-standalone/host_v4.c" -o "$OUT/host_v4.o"
    $CC -O1 -std=c11 -w -I "$INC" \
        -I "$ROOT/integration/goose-standalone" \
        -c "$ROOT/integration/goose-standalone/tensor-data.c" -o "$OUT/tensor-data.o"
    $CC -O1 -std=c11 -w -I "$INC" \
        -c "$ROOT/integration/goose-standalone/graph-bindings.c" -o "$OUT/graph-bindings.o"
    # The three names the system headers already declare (close/read/write) and
    # accept's pointer form: one-line forwards, no decisions (EXP938).
    $CC -O1 -std=c11 -w -c "$ROOT/integration/goose-standalone/libc-forwards.c" -o "$OUT/libc-forwards.o"
    # Link with $CXX, and the reason matters: libggml-base.a itself contains six C++ objects
    # (ggml.cpp, ggml-backend.cpp, ggml-backend-meta.cpp, ggml-opt.cpp, ggml-threading.cpp) and
    # ggml_critical_section_start references std::__throw_system_error. So libstdc++ is a
    # SUBSTRATE dependency, not ours. Our own sources are now all C - graph-bindings and
    # tensor-data were converted - which is the part we control.
    $CC -O1 -std=c11 -w -I "$OUT/inc" -I "$INC" -I "$ROOT/build/goose-main" \
        -I "$ROOT/integration/goose-standalone" \
        -I "$ROOT/build/goose-main" -c "$ROOT/integration/goose-standalone/goose-host-llm.c" -o "$OUT/goose-host-llm.o"
    $CXX -O1 -fopenmp "$ROOT/build/goose-main/main.o" "$OUT/host.o" "$OUT/host_v4.o" \
        "$OUT/tensor-data.o" "$OUT/graph-bindings.o" "$OUT/goose-host-llm.o" \
        "$OUT/libc-forwards.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread \
        -o "$OUT/frankenmoe-v4"
    printf '  v4:     %s (%s B)\n' "$OUT/frankenmoe-v4" "$(stat -c%s "$OUT/frankenmoe-v4")"

    # The graph-execution gate is a test, not an adapter: it proves that a node the
    # Goose builder creates runs on the CPU backend (EXP681).
    $CC -O1 -std=c11 -w -I "$INC" \
        -c "$ROOT/integration/goose-standalone/tests/graph_exec.c" -o "$OUT/graph_exec.o"
    $CXX -O1 -fopenmp "$ROOT/build/goose-main/main.o" "$OUT/host.o" \
        "$OUT/graph-bindings.o" "$OUT/graph_exec.o" "$OUT/libc-forwards.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread \
        -o "$OUT/graph_exec"
    printf '  gate:   %s\n' "$OUT/graph_exec"

    # The tokenizer gate calls every tok_* decision the unit exports with known inputs, so the status
    # line can say "answers" rather than "linked". It needs only the unit and the host primitives.
    # --gc-sections on the unit: the gate calls eight decisions, and without it the linker demands every
    # host symbol the whole unit references. Function sections let the unused ones drop.
    $CC -O1 -ffunction-sections -fdata-sections -std=c11 -w -c "$ROOT/build/goose-main/main.c" \
        -o "$OUT/main-gc.o"
    # This gate tests the eight tok_* predicates. That interface was replaced by the
    # single bt_bpe entry point when the tokenizer engine landed (EXP949-956), so the
    # gate can no longer link. It is skipped with a reason instead of failing the
    # build; the tokenizer's evidence is the EXP920 differential (70 tokenize cases /
    # 11017 ids, 496640 pieces) and EXP955/956 (13 and 12 checks, byte-identical).
    # grep -c exits 1 when it counts zero, and set -e would end the script there.
    _tok_syms=$(nm "$OUT/main-gc.o" 2>/dev/null | grep -cE " T tok_" || true)
    _tok_syms=${_tok_syms:-0}
    if [ "$_tok_syms" -ge 8 ]; then
        $CC -O1 -std=c11 -w -I "$ROOT/build/goose-main" -I "$ROOT/integration/m1/host" \
            -c "$ROOT/integration/goose-standalone/tests/tokenizer_decisions.c" -o "$OUT/tokenizer_decisions.o"
        $CXX -O1 -Wl,--gc-sections "$OUT/tokenizer_decisions.o" "$OUT/main-gc.o" "$OUT/host.o" \
            -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread -o "$OUT/tokenizer_decisions"
        printf '  gate:   %s\n' "$OUT/tokenizer_decisions"
    else
        printf '  gate:   tokenizer_decisions SKIPPED - the unit has %s tok_* exports;\n' "$_tok_syms"
        printf '          that interface was replaced by bt_bpe (EXP949-956), whose\n'
        printf '          evidence is the EXP920 differential and EXP955/956.\n'
    fi

    # The load-and-tokenise path: mmap -> gr_parse -> btl_load -> bt_bpe, with the
    # expected ids recorded from the accepted libllama. This is the caller the status
    # report says does not exist, and it needs no llama library at run time.
    $CC -O1 -std=c11 -w -I "$ROOT/build/goose-main" -I "$ROOT/integration/m1/host" \
        -c "$ROOT/integration/goose-standalone/tests/tokenize_load.c" -o "$OUT/tokenize_load.o"
    $CXX -O1 -Wl,--gc-sections "$OUT/tokenize_load.o" "$OUT/main-gc.o" "$OUT/host.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread -o "$OUT/tokenize_load"
    printf '  gate:   %s (build only; run with a vocab GGUF)\n' "$OUT/tokenize_load"

    # The user-facing entry point. Its decisions are Goose (goose/cli/tokenize-cli.goose)
    # and its tokenizer is the Goose engine; the file owns argv, stdin and stdout only.
    $CC -O1 -std=c11 -w -I "$ROOT/build/goose-main" -I "$ROOT/integration/m1/host" \
        -I "$OUT/inc" \
        -c "$ROOT/integration/goose-standalone/cli/tokenize-main.c" -o "$OUT/tokenize-main.o"
    $CXX -O1 -Wl,--gc-sections "$OUT/tokenize-main.o" "$OUT/main-gc.o" "$OUT/host.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread -o "$OUT/frankenmoe-tokenize"
    printf '  cli:    %s\n' "$OUT/frankenmoe-tokenize"

    # The HTTP entry point. Before this existed the listener was a set of symbols in
    # the artifact and no executable path reached them, which is not the same thing.
    $CC -O1 -std=c11 -w -I "$ROOT/build/goose-main" -I "$ROOT/integration/m1/host" \
        -I "$OUT/inc" \
        -c "$ROOT/integration/goose-standalone/cli/serve-main.c" -o "$OUT/serve-main.o"
    # -fopenmp for the same reason the v4 link needs it: libggml-base's quant code
    # is OpenMP-parallel and its GOMP symbols are not in libggml itself.
    $CXX -O1 -fopenmp -Wl,--gc-sections "$OUT/serve-main.o" "$OUT/main-gc.o" "$OUT/host.o" \
        "$OUT/tensor-data.o" "$OUT/graph-bindings.o" \
        -L"$SUB" -lggml-base -lggml -lggml-cpu -lm -pthread -o "$OUT/frankenmoe-serve"
    printf '  cli:    %s\n' "$OUT/frankenmoe-serve"
    if [ -n "${FG_TOKENIZE_FIXTURE:-}" ]; then
        if "$OUT/tokenize_load" "$FG_TOKENIZE_FIXTURE"; then
            printf '  gate:   tokenize_load PASS on %s\n' "$FG_TOKENIZE_FIXTURE"
        else
            printf '  gate:   tokenize_load FAILED on %s\n' "$FG_TOKENIZE_FIXTURE"
            exit 1
        fi
    fi
fi

# Provenance: what built this directory, measured now, stored with the artifact. The
# configured compiler and the one that produced these bytes are different facts, and
# only this file can tell them apart after the fact.
python3 - "$OUT" "$GOOSE" "$ROOT" <<'PROV'
import hashlib, json, os, sys
from datetime import datetime, timezone

out, goose, root = sys.argv[1:4]

def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()

cat_path = os.environ.get("FG_GOOSE_CATALOG",
                          "/home/end0/AI/Fried-Goose/.cache/selfhosted-builds/catalog.json")
try:
    with open(cat_path) as f:
        cat = json.load(f)
except (OSError, ValueError):
    cat = None

sha = sha256(goose) if os.path.isfile(goose) else ""
label = version_id = channel = None
if cat:
    for name, ch in (cat.get("channels") or {}).items():
        cur = ch.get("current") or {}
        if cur.get("sha256") == sha:
            label, version_id, channel = cur.get("label"), cur.get("version"), name
            break
    if label is None:
        for v in cat.get("versions") or []:
            if v.get("sha256") == sha:
                label, version_id, channel = v.get("label"), v.get("version"), "selfhosted_subset"
                break

unit = ""
try:
    with open(os.path.join(root, "build", "goose-main", ".source-sha256")) as f:
        unit = f.read().strip()
except OSError:
    pass

rec = {
    "compiler": goose,
    "compiler_sha256": sha,
    "registry_label": label,
    "registry_version": version_id,
    "registry_channel": channel,
    "verdict": "VERSIONED" if label else "NOT_IN_REGISTRY",
    "unit_source_sha256": unit,
    "built_at": datetime.now(timezone.utc).isoformat(),
}
with open(os.path.join(out, "COMPILER.json"), "w") as f:
    json.dump(rec, f, indent=2)
    f.write("\n")
print("  compiler: %s sha256 %s %s" % (os.path.basename(os.path.dirname(goose)), sha[:16],
                                       ("label " + label) if label else "(not in registry)"))
PROV

echo "[5/5] result"
printf '  runtime: %s (%s B)\n' "$OUT/frankenmoe-v4" \
    "$(stat -c%s "$OUT/frankenmoe-v4")"
printf '  deps:    %s\n' "$(ldd "$OUT/frankenmoe-v4" | awk '{print $1}' | tr '\n' ' ')"

if [ "$CHECK" -eq 1 ]; then
    if ldd "$OUT/frankenmoe-v4" | grep -q "libllama"; then
        echo "standalone runtime: the binary links the old llama runtime" >&2
        exit 1
    fi

    # v4 and the graph gate, when the substrate is present.
    if [ -x "$OUT/graph_exec" ]; then
        g=$("$OUT/graph_exec" 2>&1)
        printf '  gate:   %s\n' "$(printf '%s' "$g" | tr '\n' '|')"
        case "$g" in
            *FAIL*) echo "standalone runtime: graph execution gate failed" >&2; exit 1 ;;
        esac
    fi

    # Every tok_* decision the unit exports, called with known inputs. This is what makes the status
    # line say the tokenizer decisions ANSWER rather than are merely linked.
    if [ -x "$OUT/tokenizer_decisions" ]; then
        t=$("$OUT/tokenizer_decisions" 2>&1 | tail -1)
        printf '  gate:   %s\n' "$t"
        case "$t" in
            *PASS*) ;;
            *) echo "standalone runtime: tokenizer decisions gate failed" >&2; exit 1 ;;
        esac
    fi
    if [ -x "$OUT/frankenmoe-v4" ] && [ -f "$ROOT/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid/tiny-hybrid.gguf" ]; then
        v=$("$OUT/frankenmoe-v4" "$ROOT/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid/tiny-hybrid.gguf" 2>&1 | head -1)
        printf '  v4:     %s\n' "$v"
        case "$v" in
            *mismatches=0*) ;;
            *) echo "standalone runtime: tensor layout mismatch in v4" >&2; exit 1 ;;
        esac
    fi
fi
