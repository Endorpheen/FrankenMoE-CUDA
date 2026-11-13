#!/usr/bin/env python3
"""EXP261: build libggml-cpu with the corrected pointwise module and run the P2-01 contract probe."""
from pathlib import Path
import json, shutil, subprocess, sys

LIVE = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-261-pointwise-contract-fixes'
S = E / 'source'
B = E / 'build'
B.mkdir(exist_ok=True)
STAGE = B / 'stage'
STAGE.mkdir(exist_ok=True)

# unit.c includes ../../generated/cpu-backend.c; point it at the regenerated file in this experiment.
unit = (LIVE / 'source/cpu-backend/unit.c').read_text()
assert '../../generated/cpu-backend.c' in unit
(S / 'unit.c').write_text(unit.replace('../../generated/cpu-backend.c', 'cpu-backend.c'))

cmds = json.loads((LIVE / 'variants/cpu-backend/build/integration-commands.json').read_text())
rows = cmds if isinstance(cmds, list) else [cmds]
by = {r['label']: r for r in rows}

def run(label, argv, cwd):
    q = subprocess.run([str(a) for a in argv], cwd=str(cwd), capture_output=True, text=True)
    (B / (label + '.stdout')).write_text(q.stdout)
    (B / (label + '.stderr')).write_text(q.stderr)
    print('---', label, 'rc=', q.returncode)
    if q.returncode:
        print((q.stdout + q.stderr)[-2500:]); sys.exit(q.returncode)

cc = by['goose-cpu']['argv']
i = cc.index('-c') + 1
cc = list(cc); cc[i] = str(S / 'unit.c'); cc[cc.index('-o') + 1] = str(B / 'goose-cpu.o')
cc[1:1] = ['-I' + str(LIVE / 'source/cpu-backend'), '-I' + str(LIVE / 'source/cpu-backend/control')]
run('goose-cpu', cc, by['goose-cpu'].get('cwd') or LIVE)

lk = list(by['link']['argv'])
out_i = lk.index('-o') + 1
lk[out_i] = str(B / 'libggml-cpu.so.0.27.0')
for j, a in enumerate(lk):
    if a.endswith('/goose-cpu.o'):
        lk[j] = str(B / 'goose-cpu.o')
run('link', lk, by['link'].get('cwd') or (LIVE / 'variants/cpu-backend/build'))

# Stage: the accepted set, with our library substituted by a real file.
accepted = F / 'runtime/reconstructed/build/stage-run'
for p in accepted.iterdir():
    q = STAGE / p.name
    if q.is_symlink() or q.exists():
        q.unlink()
    q.symlink_to(p.resolve())
tgt = STAGE / 'libggml-cpu.so.0.27.0'
tgt.unlink()
shutil.copy2(B / 'libggml-cpu.so.0.27.0', tgt)
print('staged library', tgt.stat().st_size, 'bytes')

q = subprocess.run([str(F / 'tools/p2-01-contract/build-and-compare.sh'), str(STAGE),
                    '/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin'],
                   capture_output=True, text=True)
print(q.stdout[-4000:])
print(q.stderr[-1500:])
print('probe rc =', q.returncode)
