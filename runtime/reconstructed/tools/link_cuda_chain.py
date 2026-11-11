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
for exp, target in MODULES:
    d = F/'experiments'/exp
    if not d.exists():
        d = next(F.glob(f'experiments/*{exp.split("-")[-1]}*'), None)
    num = re.search(r'-(\d{3})-', exp).group(1)
    mod_obj = Path(f'/tmp/mod-{target}-{num}.o')
    if not d or not mod_obj.exists():
        print('skip (missing)', exp, target); continue
    nat = OBJDIR/(target + '.o')
    if not nat.exists():
        print('skip (no native)', target); continue
    mine = defined(mod_obj)
    theirs = defined(nat)
    overlap = sorted(set(mine) & set(theirs))
    if not overlap:
        print('no overlap', exp, target, '-> module only')
        link_objs.append(str(mod_obj)); continue
    dst = WORK/(target + f'.{num}.o')
    shutil.copy2(nat, dst)
    for sym in overlap:
        if sym.endswith('.cold'): continue
        new = sym + '.priv'
        subprocess.run(['objcopy','--redefine-sym',f'{sym}={new}',str(dst)],check=True)
        priv.setdefault(exp,[]).append((sym,new))
    link_objs.append(str(dst))
    link_objs.append(str(mod_obj))
    host = Path(f'/tmp/host-{exp.split("-")[-3] if False else ""}.o')

# host units: compiled earlier as /tmp/host-NNN.o for 180..186; build the rest on demand
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
replaced = {t + '.o' for _, t in MODULES}
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
print('priv', {k: len(v) for k, v in priv.items()})
print('out', OUT, OUT.stat().st_size)
