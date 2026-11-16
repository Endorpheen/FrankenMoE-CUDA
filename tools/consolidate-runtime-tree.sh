#!/usr/bin/env bash
# Copy every input the runtime build actually consumes into one fixed tree, resolving the implicit dependencies:
# the experiment directories build.py scans, the flint object directory, and the prebuilt extra objects.
#
# The point is that the resulting tree is self-contained: the same sources, the same objects, the same libraries,
# with no dependency on which experiment directory happens to be newest on disk.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DST=${DST:-/home/end0/AI/LlamaServer/runtime-build-tree}
BASE=${BASE:-/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108}
FL=${FL:-/home/end0/AI/LlamaServer/llama.cpp-flint-cuda}
[ -e "$DST" ] && { echo "refusing to overwrite $DST"; exit 1; }

mkdir -p "$DST"/{our-src,modules,bridges,flint-objects,extra-objects,libs,build}
cp "$ROOT"/runtime/reconstructed/src/*.cpp "$ROOT"/runtime/reconstructed/src/*.c "$DST/our-src/" 2>/dev/null || true
cp "$ROOT"/runtime/reconstructed/src/*.h "$DST/our-src/" 2>/dev/null || true

# Modules: the same rule build.py applies - a file DEFINES the prefix when it carries <prefix>_enabled, and the
# highest experiment number wins. Resolved once here and frozen, instead of re-decided at every build.
python3 - "$DST" "$BASE" <<'PY'
import re, shutil, sys
from pathlib import Path
dst, base = Path(sys.argv[1]), Path(sys.argv[2])
B = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments')
src = Path('/home/end0/AI/FrankenMoE-CUDA/runtime/reconstructed/build.py').read_text()
prefixes = []
for m in re.finditer(r"'?(fg\d{3}[a-z])'?", src):
    if m.group(1) not in prefixes: prefixes.append(m.group(1))
chosen = []
for p in prefixes:
    cands = []
    for hu in list(base.glob('experiments/*/source/*.c')) + list(base.glob('experiments/*/source/*.cpp')):
        if 'generated' in hu.name: continue
        if (p + '_enabled').encode() in hu.read_bytes():
            mm = re.search(r'EXP-\d{4}-\d{2}-\d{2}-(\d+)', str(hu))
            if mm: cands.append((int(mm.group(1)), hu))
    if cands:
        exp, hu = max(cands)
        shutil.copy2(hu, dst/'modules'/(p + hu.suffix))
        chosen.append((p, exp, hu.name))
for p, exp, name in chosen: print(f"{p} EXP{exp} {name}")
PY

# Bridges: the experiment directories build.py names explicitly.
for m in $(grep -oE "EXP-[0-9]{4}-[0-9]{2}-[0-9]{2}-[0-9]+-[a-z0-9-]+" "$ROOT/runtime/reconstructed/build.py" | sort -u); do
    if [ -f "$BASE/experiments/$m/source/bridge.c" ]; then
        mkdir -p "$DST/bridges/$m"; cp -r "$BASE/experiments/$m/source/." "$DST/bridges/$m/"
    fi
done

# Prebuilt objects and libraries, as bytes.
find "$FL/build-cuda/src/CMakeFiles/llama.dir" -name '*.o' -exec cp {} "$DST/flint-objects/" \;
W=${BUILD:-$BASE/runtime/reconstructed/build}
for o in b291.o fx286pic.o fx262a.o fx263-pic.o gates.o slru-policy.o slru-runtime.o slru-shim.o \
         fg-ar.o fg-ar-bridge.o fg-q35.o fg-qg.o fg-q35-bridge.o fg-plan-ffi.o fg-gr.o fg-gr-bridge.o; do
    [ -f "$W/$o" ] && cp "$W/$o" "$DST/extra-objects/"
done
for l in "$W"/stage-run/lib*.so.0*; do cp -L "$l" "$DST/libs/" 2>/dev/null || true; done

( cd "$DST" && find . -type f -exec sha256sum {} \; | sort -k2 > SHA256SUMS )
echo "consolidated tree: $DST"
for d in our-src modules bridges flint-objects extra-objects libs; do printf '  %-15s %s files\n' "$d" "$(find "$DST/$d" -type f | wc -l)"; done
