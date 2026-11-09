#!/usr/bin/env python3
"""Generate, build and link the EXP196 pinned-memory policy integration."""
from pathlib import Path
import json, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-08-196-goose-cuda-pinned-pool'
B = F / 'runtime/exp196/build'
B.mkdir(parents=True, exist_ok=True)
producer = json.loads((F / 'runtime/compiler/current.json').read_text())['compiler']
CARGO = json.loads((F / 'runtime/exp160/build/commands.json').read_text())
LINK = json.loads((F / 'runtime/exp185/build/commands.json').read_text())

rows = []
def run(label, argv, cwd=None):
    q = subprocess.run([str(x) for x in argv], cwd=cwd, text=True, capture_output=True)
    (B / (label + '.stdout')).write_text(q.stdout)
    (B / (label + '.stderr')).write_text(q.stderr)
    rows.append(dict(label=label, argv=[str(x) for x in argv], cwd=str(cwd or E), exit_code=q.returncode))
    (E / 'local-test-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('---', label, 'rc=', q.returncode, flush=True)
    out = (q.stdout + q.stderr).strip()
    if out:
        print(out[-2500:], flush=True)
    if q.returncode:
        raise SystemExit(q.returncode)

run('generate', [producer, '-o', B / 'host-raw.c', E / 'source/host.goose'])
raw = (B / 'host-raw.c').read_text()
(B / 'host-generated.c').write_text(raw)
(E / 'source/host-generated.c').write_text(raw)

host = ['gcc-13', '-O2', '-fPIC', '-fno-strict-aliasing', '-ffp-contract=off',
        '-I' + str(E / 'source'), '-include', 'stdint.h', '-include', 'stdio.h',
        '-include', 'stdlib.h', '-include', 'string.h']
run('bridge-build', host + ['-c', E / 'source/bridge.c', '-o', B / 'bridge.o'])
run('fixture-build', ['gcc-13', '-O2', '-fPIC', '-I' + str(E / 'source'), '-c',
                      E / 'tests/fixture.c', '-o', B / 'fixture.o'])
run('fixture-link', ['gcc-13', B / 'fixture.o', B / 'bridge.o', '-lm', '-o', B / 'fixture'])
run('fixture', [B / 'fixture'])

cc = [x for x in CARGO if x['label'] == 'shared-cuda-build'][0]
argv = list(cc['argv'])
src_i = argv.index('-c') + 1 if '-c' in argv else None
if src_i is None:
    src_i = next(i for i, x in enumerate(argv) if x.endswith('.cu'))
argv[src_i] = str(E / 'source/shared-cuda.cu')
argv[argv.index('-o') + 1] = str(B / 'shared-cuda-goose.o')
run('core-build', argv, cc.get('cwd'))

lk = [x for x in LINK if x['label'] == 'cuda-mmvq-q4k-link'][0]
args = [str(B / 'shared-cuda-goose.o') if x.endswith('shared-cuda.o') else x for x in lk['argv']]
args[args.index('-o') + 1] = str(B / 'libggml-cuda-pinned.so.0.27.0')
add = [str(B / 'bridge.o')]
# EXP195's VMM pool module travels with the copy this stage wires into.
vmm = F / 'runtime/exp195/build/bridge.o'
if vmm.exists():
    add.append(str(vmm))
args.extend(add)
run('link', args, lk.get('cwd'))

print('EXP196_BUILD_PASS')
