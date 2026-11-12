#!/usr/bin/env python3
"""Link libggml-cuda from the clang build plus the Goose modules.

Each module defines a public C++ symbol that the native object also defines. The native definition is
privatised with objcopy --redefine-sym (keeping the .cold companions intact), and the module supplies the
public name, exactly as the per-experiment build_runtime.py stages did.
"""
import json, os, re, shlex, shutil, subprocess, sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
OBJDIR = Path('/tmp/cuda-build2/ggml/src/ggml-cuda/CMakeFiles/ggml-cuda.dir')
OUT = Path('/tmp/libggml-cuda-chain.so.0.27.0')
WORK = Path('/tmp/cuda-priv'); WORK.mkdir(exist_ok=True)

MODULES = [
    ('EXP-2026-11-08-173-goose-cuda-flash-attention', 'fattn-vec-instance-q4_0-q4_0.cu'),
    ('EXP-2026-11-08-174-goose-cuda-set-rows', 'set-rows.cu'),
    ('EXP-2026-11-08-175-goose-cuda-concat', 'concat.cu'),
    ('EXP-2026-11-08-176-goose-cuda-get-rows', 'getrows.cu'),
    ('EXP-2026-11-08-177-goose-cuda-cpy', 'cpy.cu'),
    ('EXP-2026-11-08-179-goose-cuda-moe-weighted', 'moe-weighted-reduction.cu'),
    ('EXP-2026-11-08-181-goose-cuda-mm-ids', 'mmid.cu'),
    ('EXP-2026-11-08-182-goose-cuda-dequant-q4k', 'convert.cu'),
    ('EXP-2026-11-08-183-goose-cuda-fwht', 'fwht.cu'),
    ('EXP-2026-11-08-184-goose-cuda-mmvf', 'mmvf.cu'),
    ('EXP-2026-11-08-186-goose-cuda-mmvq-q6k', 'mmvq.cu'),
    ('EXP-2026-11-08-188-goose-cuda-dequant-q4-0', 'mmvq.cu'),
    ('EXP-2026-11-08-208-goose-mmq-typed-x', 'quantize.cu'),
]

def defined(path, kinds=('T','W')):
    q = subprocess.run(['nm','--defined-only',str(path)],capture_output=True,text=True)
    out = {}
    for line in q.stdout.splitlines():
        ps = line.split()
        if len(ps) < 3 or ps[1] not in kinds: continue
        out[ps[2]] = ps[1]
    return out

priv = {}
link_objs = []
# names the modules expect for the privatised natives, collected from their sources
EXPECT = {}
for src in F.glob('experiments/EXP-*/source/cuda-runtime.cu'):
    for m in re.finditer(r'\b(fg[0-9a-z]+_native_([A-Za-z0-9_]+))', src.read_text()):
        EXPECT[m.group(2)] = m.group(1)
print('expected native names:', len(EXPECT))
# every public name any module defines: a native object that also defines one of these must be privatised,
# even if that particular module does not overlap with it (convert.cu carries both fp32 and fp16)
pub = set()
sorted_mods = []
for exp, target in MODULES:
    num0 = re.search(r'-(\d{3})-', exp).group(1)
    m0 = Path(f'/tmp/mod-{target}-{num0}.o')
    if m0.exists():
        pub |= set(defined(m0))
        sorted_mods.append((exp, target, m0))
print('public names across modules:', len(pub))

targets = sorted({q.name[:-2] for q in OBJDIR.glob('*.o')})
for t in targets:
    nat = OBJDIR/(t + '.o')
    if not nat.exists():
        print('skip (no native)', t); continue
    dst = WORK/(t + '.priv.o')
    shutil.copy2(nat, dst)
    renamed = 0
    for sym in sorted(defined(nat)):
        if sym.endswith('.cold'): continue
        if sym not in pub: continue
        if sym.startswith('fg'): continue   # goose entry points are consumed by the per-module .so files
        # The modules call the privatised native symbol by an agreed name, e.g.
        # fg185q_native_ggml_cuda_mul_mat_vec_q, so the rename must use that name rather than a suffix.
        m = re.match(r'_Z(\d+)', sym)
        if not m: continue
        n = int(m.group(1)); name = sym[m.end():m.end()+n]; rest = sym[m.end()+n:]
        newname = EXPECT.get(name)
        if newname is None:
            # no module calls this native symbol back; a plain suffix keeps the definition out of the way
            new = sym + '.priv'
        else:
            new = '_Z' + str(len(newname)) + newname + rest
        subprocess.run(['objcopy','--redefine-sym',f'{sym}={new}',str(dst)],check=True)
        renamed += 1
    priv[t] = renamed
    link_objs.append(str(dst))

for exp, target, mod in sorted_mods:
    link_objs.append(str(mod))

for exp, target in MODULES:
    num = re.search(r'-(\d{3})-', exp).group(1)
    h = Path(f'/tmp/host-{num}.o')
    if h.exists():
        link_objs.append(str(h))
    else:
        print('host object missing for', exp)

args = shlex.split((OBJDIR/'link.txt').read_text().strip()) if (OBJDIR/'link.txt').exists() else None
if args is None:
    args = shlex.split(Path('/tmp/cuda-build2/ggml/src/ggml-cuda/CMakeFiles/ggml-cuda.dir/link.txt').read_text().strip())
compiler = '/usr/bin/clang++'
body = args[1:]
base_objs = [a for a in body if a.endswith('.o')]
rest = [a for a in body if not a.endswith('.o')]
replaced = {t + '.o' for t in targets}
base_objs = [o for o in base_objs if Path(o).name not in replaced]
out = []; skip = 0
for a in rest:
    if a == '-o': out.append(a); skip = 1; continue
    if skip: skip = 0; out.append(str(OUT)); continue
    out.append(a)
out += ['-Wl,--version-script=' + str(F/'experiments/EXP-2026-11-08-186-goose-cuda-mmvq-q6k/source/visibility.map')]
q = subprocess.run([compiler]+out+base_objs+link_objs, cwd='/tmp/cuda-build2/ggml/src/ggml-cuda', capture_output=True, text=True, timeout=1800)
print('link rc', q.returncode)
if q.returncode: print((q.stdout+q.stderr)[-2000:]); sys.exit(1)
print('priv', priv)
print('out', OUT, OUT.stat().st_size)
