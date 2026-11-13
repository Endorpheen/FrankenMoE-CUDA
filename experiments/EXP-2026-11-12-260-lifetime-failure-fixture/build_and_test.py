#!/usr/bin/env python3
"""Regenerate the pinned and VMM pool policies with the pinned compiler and run the shared fixture."""
from pathlib import Path
import json, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-260-lifetime-failure-fixture'
S = E / 'source'
B = E / 'build'
B.mkdir(exist_ok=True)
pin = json.loads((F / 'runtime/compiler/current.json').read_text())
CC = pin['compiler']

rows = []


def run(label, argv, cwd=None):
    q = subprocess.run([str(x) for x in argv], cwd=str(cwd or E), text=True, capture_output=True)
    (B / (label + '.stdout')).write_text(q.stdout)
    (B / (label + '.stderr')).write_text(q.stderr)
    rows.append(dict(label=label, argv=[str(x) for x in argv], exit_code=q.returncode))
    (E / 'commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    out = (q.stdout + q.stderr).strip()
    print('---', label, 'rc=', q.returncode)
    if out:
        print(out[-2500:])
    if q.returncode:
        sys.exit(q.returncode)


run('generate-pinned', [CC, '-o', S / 'host-generated.c', S / 'pinned-host.goose'])
run('generate-vmm',    [CC, '-o', S / 'device-generated.c', S / 'vmm-device.goose'])

cc = ['gcc-13', '-O2', '-fPIC', '-fno-strict-aliasing', '-ffp-contract=off', '-I' + str(S),
      '-include', 'stdint.h', '-include', 'stdio.h', '-include', 'stdlib.h', '-include', 'string.h']
run('pinned-bridge', cc + ['-c', S / 'pinned-bridge.c', '-o', B / 'pinned-bridge.o'])
run('vmm-bridge',    cc + ['-c', S / 'vmm-bridge.c', '-o', B / 'vmm-bridge.o'])
run('fixture-build', ['gcc-13', '-O2', '-I' + str(S), '-c', E / 'tests/lifetime_fixture.c',
                      '-o', B / 'fixture.o'])
run('fixture-link',  ['gcc-13', B / 'fixture.o', B / 'pinned-bridge.o', B / 'vmm-bridge.o',
                      '-o', B / 'fixture'])
run('fixture',       [B / 'fixture'])
print('PASS lifetime fixture')
