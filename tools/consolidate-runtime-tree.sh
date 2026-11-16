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

mkdir -p "$DST"/{our-src,modules,module-src,bridges,flint-objects,extra-objects,libs,build,upstream-include,upstream-src}
cp "$ROOT"/runtime/reconstructed/src/*.cpp "$ROOT"/runtime/reconstructed/src/*.c "$DST/our-src/" 2>/dev/null || true
cp "$ROOT"/runtime/reconstructed/src/*.h "$DST/our-src/" 2>/dev/null || true

# Modules: the same rule build.py applies - a file DEFINES the prefix when it carries <prefix>_enabled, and the
# highest experiment number wins. Resolved once here and frozen, instead of re-decided at every build.
python3 - "$DST" "$BASE" "${OBJDIR:-$BASE/runtime/reconstructed/build}" <<'PY'
import re, shutil, sys
from pathlib import Path
dst, base = Path(sys.argv[1]), Path(sys.argv[2])
B = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments')
# Reproduce build.py's rule exactly, because approximations produced 27 then 0 modules:
#   1. the prefixes are the ones OUR compiled objects reference as undefined symbols;
#   2. an explicit list of 25 prefixes is skipped because those are the bridges linked separately.
# Guessing from file names is what went wrong, so nothing here is inferred from a name.
import subprocess
objs_dir = Path(sys.argv[3]) if len(sys.argv) > 3 else None
BRIDGE_PREFIXES = {'fg345w','fg367s','fg373s','fg375s','fg376s','fg377s','fg380s','fg381s','fg382s','fg383s',
                   'fg384s','fg385s','fg386s','fg387s','fg388s','fg389s','fg390s','fg391s','fg392s','fg393s',
                   'fg394s','fg395s','fg396s','fg397s','fg398s','fg401s'}
prefixes = []
for o in sorted(list(Path(objs_dir).glob('our-*.o')) + list(Path(objs_dir).glob('*.cpp.o'))) if objs_dir else []:
    out = subprocess.run(['nm','-u',str(o)], capture_output=True, text=True).stdout
    for line in out.splitlines():
        if not line.strip(): continue
        m = re.match(r'^(fg\d{3}[a-z])', line.split()[-1])
        if m and m.group(1) not in BRIDGE_PREFIXES and m.group(1) not in prefixes:
            prefixes.append(m.group(1))
print('modules needed:', len(prefixes))
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
        # A module whose source is a bridge.c belongs to the BRIDGE list, not here. build.py's module loop only
        # produces <prefix>.o for files that are not bridges - fg401s.o does not exist in the real build tree, and
        # its symbol comes from b401.o alone. Compiling bridge.c as a module too defines every entry point twice.
        # No exclusion needed: modules are frozen as objects below, so this path only records the mapping.
        shutil.copy2(hu, dst/'modules'/(p + hu.suffix))
        # a module may #include a generated companion from its own directory; freeze the whole directory
        d = dst/'module-src'/hu.parent.parent.name
        d.mkdir(parents=True, exist_ok=True)
        for f in hu.parent.iterdir():
            if f.is_file(): shutil.copy2(f, d/f.name)
        chosen.append((p, exp, hu.name))
for p, exp, name in chosen: print(f"{p} EXP{exp} {name}")
(dst/'module-objects.txt').write_text(''.join(p + '.o\n' for p, _, _ in chosen))
PY

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
W=${BUILD:-$BASE/runtime/reconstructed/build}
# Every object build.py itself produced: the 39 Goose module objects, the 22 bridge objects, and the hand-built
# extras. Frozen as bytes, exactly like the flint objects, so the clean build does not re-decide which file owns a
# prefix at every run.
# Only the objects build.py actually links: its module loop (fgNNNs.o), its bridge loop (bNNN.o) and the explicit
# extra_mod list. A blanket copy pulled in old non-PIC variants (fx286.o, fx291.o) that the real link never uses -
# fx286pic.o is the PIC one - and the clean build then failed on relocations.
# Only the module objects this resolution actually chose. The build directory also holds stale objects from
# earlier runs - fg367s.o is there although build.py excludes fg367s as a bridge - and a blanket copy of fg*.o
# reproduced exactly the stale-object problem this consolidation exists to remove.
while IFS= read -r mo; do [ -f "$W/$mo" ] && cp "$W/$mo" "$DST/extra-objects/"; done < "$DST/module-objects.txt"
for o in "$W"/b[0-9][0-9][0-9].o \
         "$W"/b291.o "$W"/fx286pic.o "$W"/fx262a.o "$W"/fx263-pic.o "$W"/gates.o \
         "$W"/slru-policy.o "$W"/slru-runtime.o "$W"/slru-shim.o \
         "$W"/fg-ar.o "$W"/fg-ar-bridge.o "$W"/fg-q35.o "$W"/fg-qg.o "$W"/fg-q35-bridge.o \
         "$W"/fg-plan-ffi.o "$W"/fg-gr.o "$W"/fg-gr-bridge.o "$W"/fg-q35s.o "$W"/fg-q35s-bridge.o; do
    [ -f "$o" ] && cp "$o" "$DST/extra-objects/"
done
# Only the seven libraries the runtime loads, and only their real files: resolving every symlink variant with -L
# turned each into a full 220 MB copy and each consolidated tree into 9.6 GB.
for l in libggml.so.0 libggml-base.so.0 libggml-cpu.so.0 libggml-cuda.so.0 \
         libllama-common.so.0 libllama-server-impl.so libmtmd.so.0; do
    [ -f "$W/stage-run/$l" ] && cp -L "$W/stage-run/$l" "$DST/libs/"
done

( cd "$DST" && find . -type f -exec sha256sum {} \; | sort -k2 > SHA256SUMS )
echo "consolidated tree: $DST"
for d in our-src modules bridges flint-objects extra-objects libs; do printf '  %-15s %s files\n' "$d" "$(find "$DST/$d" -type f | wc -l)"; done
