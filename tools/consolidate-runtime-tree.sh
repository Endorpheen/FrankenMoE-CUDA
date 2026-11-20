#!/usr/bin/env bash
# Copy every input the runtime build actually consumes into one fixed tree, resolving the implicit dependencies:
# the experiment directories build.py scans, the flint object directory, and the prebuilt extra objects.
#
# The point is that the resulting tree is self-contained: the same sources, the same objects, the same libraries,
# with no dependency on which experiment directory happens to be newest on disk.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DST=${DST:-/home/end0/AI/LlamaServer/runtime-build-tree}
# The module sources come from this repository, not from an external snapshot. Checked 2026-11-19: the
# external snapshot carried 528 experiments and this tree carries 1134, and for all 86 module prefixes
# the rule below picks exactly the same file from both - 86 of 86, no difference, none missing. The
# snapshot was therefore never adding anything the repository does not have, while making the build
# depend on a directory outside it. BASE stays overridable for an archaeology run.
# BASE is the tree the consolidation reads from, and it is used for more than module sources: the
# experiment directories ($BASE/experiments), the bridges, the object directory
# ($BASE/runtime/reconstructed/build) and the hand-built objects. Pointing it at `experiments` instead
# of the tree root broke three of those on 2026-11-19 - the staging run reported "bridges 0 files,
# libs 0 files, extra-objects 0 files" and failed at compose. It is the repository root.
# The libraries are taken from LIBSDIR when given (a build output such as <build>/run), otherwise from
# the tree's own stage-run, because a fresh tree has neither until something has been built.
BASE=${BASE:-$ROOT}
FL=${FL:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
[ -e "$DST" ] && { echo "refusing to overwrite $DST"; exit 1; }

W=${BUILD:-$BASE/runtime/reconstructed/build}
src=${LIBSDIR:-$W/stage-run}
extra_objects=(
    b291.o fx286pic.o fx262a.o fx263-pic.o gates.o
    slru-policy.o slru-runtime.o slru-shim.o
    fg-ar.o fg-ar-bridge.o fg-q35.o fg-qg.o fg-q35-bridge.o
    fg-plan-ffi.o fg-gr.o fg-gr-bridge.o fg-q35s.o fg-q35s-bridge.o
)
libraries=(
    libggml.so.0 libggml-base.so.0 libggml-cpu.so.0 libggml-cuda.so.0
    libllama-common.so.0 libllama-server-impl.so libmtmd.so.0
)
for input in "${extra_objects[@]/#/$W/}" "${libraries[@]/#/$src/}"; do
    if [ ! -f "$input" ]; then
        echo "missing consolidation input: $input" >&2
        exit 1
    fi
done

mkdir -p "$DST"/{our-src,modules,module-src,bridges,flint-objects,extra-objects,libs,build,upstream-include,upstream-src}
cp "$ROOT"/runtime/reconstructed/src/*.cpp "$ROOT"/runtime/reconstructed/src/*.c "$DST/our-src/" 2>/dev/null || true
cp "$ROOT"/runtime/reconstructed/src/*.h "$DST/our-src/" 2>/dev/null || true
# The GGUF reader adapter is a carrier-side source, not a header or a staged tree file, and the unit's loader
# calls llama_goose_register which it defines. Without it the rebuilt artefact loads, starts, prints its Goose
# witnesses and then dies on `undefined symbol: llama_goose_register` - a missing translation unit, not a
# design difference. build.py always compiled it; this tree did not.
cp "$ROOT"/integration/goose-standalone/gguf-reader-owner.cpp "$DST/our-src/" 2>/dev/null || true

# Modules: the same rule build.py applies - a file DEFINES the prefix when it carries <prefix>_enabled, and the
# highest experiment number wins. Resolved once here and frozen, instead of re-decided at every build.
# Resolution lives in its own script so it can be pinned: pass RESOLUTION=<file> and nothing is decided by
# scanning directories. The rule is build.py's - prefixes from nm -u, the 25-bridge exclusion, highest
# experiment number - reproduced there and verified against the preserved tree.
python3 "$ROOT/tools/resolve-modules.py" "$DST" "$BASE" "${OBJDIR:-$BASE/runtime/reconstructed/build}" ${RESOLUTION:-}

# A carrier can include a header the Goose compiler writes for a module (slru-policy.h is produced by
# `goose --header` from goose/cpu/goose-moe-cache-slru.goose). Such a header is generated, never
# committed, and its absence fails the build with "fatal error: slru-policy.h: No such file" - which is
# how this step came to exist. Only the includes that have no tracked file are generated, so the step
# costs one compile per such header and nothing else.
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
for inc in $(grep -rhoE '#include "[a-z0-9_-]+\.h"' "$ROOT"/runtime/reconstructed/src/*.cpp "$ROOT"/integration/m1/common/*.cpp 2>/dev/null \
             | sed 's/.*"\(.*\)"/\1/' | sort -u); do
    # `[ ... ] && continue` returns 1 when the test fails, and under `set -e` that ends the whole
    # script - which is exactly what happened on the first run of this step: exit 2, no message.
    if [ -f "$ROOT/runtime/reconstructed/src/$inc" ] || [ -f "$ROOT/vendor/flint/src/$inc" ] \
       || [ -f "$DST/module-src/$inc" ]; then
        continue
    fi
    stem=${inc%.h}
    # `ls` fails when nothing matches, and with pipefail the assignment fails with it, so the whole
    # script exits 2 without a message - the second form of the same trap in this loop.
    hsrc=$(ls "$ROOT"/goose/*/"$stem".goose 2>/dev/null | head -1 || true)
    # A header's name does not always match its module: slru-policy.h is written from
    # goose/cpu/goose-moe-cache-slru.goose. The map records the cases the name rule cannot find.
    if [ -z "$hsrc" ]; then
        case "$inc" in
            slru-policy.h) hsrc=$ROOT/goose/cpu/goose-moe-cache-slru.goose;;
            config-slice.h) hsrc=$ROOT/goose/cli/config-validate.goose;;
        esac
    fi
    [ -n "$hsrc" ] || continue
    if "$G" -o "$DST/module-src/${stem}.c" --header "$DST/module-src/$inc" "$hsrc" >/dev/null 2>&1; then
        echo "  generated header $inc from goose/${hsrc#$ROOT/goose/}"
    else
        echo "  WARNING could not generate $inc from $hsrc"
    fi
done

# The C++ m1 bridges. They are why the accepted artefact carries about a hundred mangled C++ symbols and
# the rebuilt one did not: seven files in integration/m1/common declare the entry points the composed unit
# calls (goose_tok_enabled was the first missing one), and no consolidated set ever compiled them. The host
# headers they include live next door.
mkdir -p "$DST/m1bridges"
cp "$ROOT"/integration/m1/common/*.cpp "$DST/m1bridges/" 2>/dev/null || true
cp "$ROOT"/integration/m1/common/*.h   "$DST/m1bridges/" 2>/dev/null || true
cp "$ROOT"/integration/m1/host/*.h     "$DST/m1bridges/" 2>/dev/null || true
cp "$ROOT"/integration/m1/host/*.c     "$DST/m1bridges/" 2>/dev/null || true
echo "  m1 bridges      $(ls "$DST"/m1bridges/*.cpp 2>/dev/null | wc -l) cpp, $(ls "$DST"/m1bridges/*.h 2>/dev/null | wc -l) h"

# Bridges: the experiment directories build.py names explicitly.
for m in $(grep -oE "EXP-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]+-[a-z0-9-]+" "$ROOT/runtime/reconstructed/build.py" | sort -u); do
    if [ -f "$BASE/experiments/$m/source/bridge.c" ]; then
        mkdir -p "$DST/bridges/$m"; cp -r "$BASE/experiments/$m/source/." "$DST/bridges/$m/"
    fi
done

# Upstream headers and sources: our files include llama.h / ggml.h, and a module may include a generated
# companion from its own experiment directory. Both are inputs, so both are frozen here.
cp "$FL"/include/*.h "$DST/upstream-include/" 2>/dev/null || true
cp -r "$FL"/ggml/include/. "$DST/upstream-include/" 2>/dev/null || true
cp "$FL"/src/*.h "$DST/upstream-src/" 2>/dev/null || true
cp -r "$FL"/src/models "$DST/upstream-src/" 2>/dev/null || true
for d in "$FL"/src/*/; do cp -r "$d". "$DST/upstream-src/" 2>/dev/null || true; done
cp "$FL"/ggml/src/*.h "$DST/upstream-src/" 2>/dev/null || true

# Prebuilt objects and libraries, as bytes.
find "$FL/build-cuda/src/CMakeFiles/llama.dir" -name '*.o' -exec cp {} "$DST/flint-objects/" \;
# Every object build.py itself produced: the 39 Goose module objects, the 22 bridge objects, and the hand-built
# extras. Frozen as bytes, exactly like the flint objects, so the clean build does not re-decide which file owns a
# prefix at every run.
# Only the objects build.py actually links: its module loop (fgNNNs.o), its bridge loop (bNNN.o) and the explicit
# extra_mod list. A blanket copy pulled in old non-PIC variants (fx286.o, fx291.o) that the real link never uses -
# fx286pic.o is the PIC one - and the clean build then failed on relocations.
# Only hand-built objects are frozen here. Module and bridge objects are NOT copied: build-from-tree.sh compiles
# them from the sources in modules/ and bridges/, which is what makes this a build rather than a re-link. Copying
# them as well produced duplicate definitions, which is how the earlier stale-object problem showed itself.
cp "${extra_objects[@]/#/$W/}" "$DST/extra-objects/"
# Only the seven libraries the runtime loads, and only their real files: resolving every symlink variant with -L
# turned each into a full 220 MB copy and each consolidated tree into 9.6 GB.
cp -L "${libraries[@]/#/$src/}" "$DST/libs/"

( cd "$DST" &&  find . -type f ! -name SHA256SUMS -exec sha256sum {} \; | sort -k2 > SHA256SUMS )
echo "consolidated tree: $DST"
for d in our-src modules bridges m1bridges flint-objects extra-objects libs; do printf '  %-15s %s files\n' "$d" "$(find "$DST/$d" -type f | wc -l)"; done
