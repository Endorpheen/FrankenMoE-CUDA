#!/usr/bin/env python3
"""Regenerate, build and run the EXP190 differential fixture.

The generated C is produced by the qualified Goose compiler from source/device.goose,
then compiled together with source/bridge.c (which supplies the extern primitives) and
linked against the real libggml-base.so so the comparison is against
quantize_row_q8_K_ref rather than a hand-written reference.
"""
from pathlib import Path
import json, re, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-08-190-goose-cpu-q8k'
B = F / 'runtime/exp190/build'
LL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
LB = LL / 'build-cuda/bin'
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
        print(out[-3000:], flush=True)
    if q.returncode:
        raise SystemExit(q.returncode)

# 1. generate C from Goose
run('generate', [producer, '-o', B / 'device-raw.c', E / 'source/device.goose'])

# 2. qualify identifiers so the generated body cannot collide with the bridge or ggml
raw = (B / 'device-raw.c').read_text()
out = re.sub(r'\bgg_function_', 'fg190q_fn_', raw)
out = re.sub(r'\bgg_view\b', 'fg190q_view', out)
out = re.sub(r'\bgg_slice\b', 'fg190q_slice', out)
out = re.sub(r'\bgg_index\b', 'fg190q_index', out)
(B / 'device-generated.c').write_text(out)
(E / 'source/device-generated.c').write_text(out)

# 3. build the bridge and the fixture
inc = ['-I' + str(LL / 'ggml/src'), '-I' + str(LL / 'ggml/include'),
       '-I' + str(LL / 'include'), '-I' + str(E / 'source')]
cc = ['gcc-13', '-O2', '-fno-strict-aliasing', '-ffp-contract=off'] + inc
run('bridge-build', cc + ['-c', E / 'source/bridge.c', '-o', B / 'bridge.o'])
run('fixture-build', cc + ['-c', E / 'tests/fixture.c', '-o', B / 'fixture.o'])
run('fixture-link', ['g++-13', B / 'fixture.o', B / 'bridge.o',
                     '-L' + str(LB), '-Wl,-rpath,' + str(LB), '-lggml-base', '-lm',
                     '-o', B / 'fixture'])

# 4. run
run('fixture', [B / 'fixture'])
print('EXP190_LOCAL_GATES_PASS')
