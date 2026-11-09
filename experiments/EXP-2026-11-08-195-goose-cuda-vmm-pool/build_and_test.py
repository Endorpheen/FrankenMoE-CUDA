#!/usr/bin/env python3
"""Generate, build and link the EXP195 VMM pool integration.

Three artifacts:
  1. the Goose module (bridge.o) holding the pool arithmetic,
  2. a modified copy of the core translation unit (shared-cuda.cu) whose
     ggml_cuda_pool_vmm::alloc/free call into it,
  3. a shared library built from the active link line with those two swapped in.

The core copy lives here rather than in EXP160 so that stage stays exactly as it was; the
only coupling is that a full chain rebuild must pass EXP160 the same bridge object, which
the README states.
"""
from pathlib import Path
import json, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-08-195-goose-cuda-vmm-pool'
B = F / 'runtime/exp195/build'
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

run('generate', [producer, '-o', B / 'device-raw.c', E / 'source/device.goose'])
raw = (B / 'device-raw.c').read_text()
(B / 'device-generated.c').write_text(raw)
(E / 'source/device-generated.c').write_text(raw)

host = ['gcc-13', '-O2', '-fPIC', '-fno-strict-aliasing', '-ffp-contract=off',
        '-I' + str(E / 'source'), '-include', 'stdint.h', '-include', 'stdio.h',
        '-include', 'stdlib.h', '-include', 'string.h']
run('bridge-build', host + ['-c', E / 'source/bridge.c', '-o', B / 'bridge.o'])
run('fixture-build', host + ['-c', E / 'tests/fixture.c', '-o', B / 'fixture.o'])
run('fixture-link', ['g++-13', B / 'fixture.o', B / 'bridge.o', '-o', B / 'fixture'])
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
args[args.index('-o') + 1] = str(B / 'libggml-cuda-vmmpool.so.0.27.0')
args.append(str(B / 'bridge.o'))
run('link', args, lk.get('cwd'))

print('EXP195_BUILD_PASS')
