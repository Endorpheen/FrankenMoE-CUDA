#!/usr/bin/env python3
"""Generate, build and run the EXP195 differential gate for the VMM pool arithmetic."""
from pathlib import Path
import json, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-08-195-goose-cuda-vmm-pool'
B = F / 'runtime/exp195/build'
B.mkdir(parents=True, exist_ok=True)
producer = json.loads((F / 'runtime/compiler/current.json').read_text())['compiler']

rows = []
def run(label, argv, cwd=None):
    q = subprocess.run([str(x) for x in argv], cwd=cwd or E, text=True, capture_output=True)
    (B / (label + '.stdout')).write_text(q.stdout)
    (B / (label + '.stderr')).write_text(q.stderr)
    rows.append(dict(label=label, argv=[str(x) for x in argv], exit_code=q.returncode))
    (E / 'local-test-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('---', label, 'rc=', q.returncode, flush=True)
    out = (q.stdout + q.stderr).strip()
    if out:
        print(out[-2500:], flush=True)
    if q.returncode:
        raise SystemExit(q.returncode)

run('generate', [producer, '-o', B / 'device-raw.c', E / 'source/device.goose'])

raw = (B / 'device-raw.c').read_text()
out = re.sub(r'\bgg_function_', 'gg_function_', raw)   # keep names; the bridge maps them
(B / 'device-generated.c').write_text(out)
(E / 'source/device-generated.c').write_text(out)

cc = ['gcc-13', '-O2', '-fno-strict-aliasing', '-ffp-contract=off',
      '-I' + str(E / 'source'), '-include', 'stdint.h', '-include', 'stdio.h',
      '-include', 'stdlib.h', '-include', 'string.h']
run('bridge-build', cc + ['-c', E / 'source/bridge.c', '-o', B / 'bridge.o'])
run('fixture-build', cc + ['-c', E / 'tests/fixture.c', '-o', B / 'fixture.o'])
run('fixture-link', ['g++-13', B / 'fixture.o', B / 'bridge.o', '-o', B / 'fixture'])
run('fixture', [B / 'fixture'])
print('EXP195_LOCAL_GATES_PASS')
