#!/usr/bin/env python3
import json, re, subprocess, sys
from pathlib import Path
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-323-p2-13-iq2s-decoder'
B = F / 'runtime/exp323/build'; B.mkdir(parents=True, exist_ok=True)
LL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda'); LB = LL / 'build-cuda/bin'
producer = json.loads((F / 'runtime/compiler/current.json').read_text())['compiler']
rows = []
def run(label, argv):
    q = subprocess.run([str(x) for x in argv], text=True, capture_output=True)
    rows.append(dict(label=label, argv=[str(x) for x in argv], exit_code=q.returncode,
                     stdout=q.stdout[-4000:], stderr=q.stderr[-4000:]))
    (E / 'local-test-commands.json').write_text(json.dumps(rows, indent=2) + '\n')
    print('---', label, 'rc=', q.returncode, flush=True)
    if q.returncode:
        print((q.stdout + q.stderr)[-2500:], flush=True)
        raise SystemExit(q.returncode)
run('generate', [producer, '-o', B / 'iq2s-raw.c', E / 'source/iq2s.goose'])
raw = (B / 'iq2s-raw.c').read_text()
out = re.sub(r'\bgg_function_', 'fg323q_fn_', raw)
for name in ('gg_view', 'gg_slice', 'gg_index'):
    out = re.sub(r'\b' + name + r'\b', 'fg323q_' + name[3:], out)
(B / 'iq2s-generated.c').write_text(out)
(E / 'source/iq2s-generated.c').write_text(out)
inc = ['-I' + str(LL / 'ggml/src'), '-I' + str(LL / 'ggml/include'), '-I' + str(LL / 'include'), '-I' + str(B)]
cc = ['gcc-13', '-O2', '-fno-strict-aliasing', '-ffp-contract=off'] + inc
run('bridge-build', cc + ['-c', E / 'source/bridge.c', '-o', B / 'bridge.o'])
run('fixture-build', cc + ['-c', E / 'tests/fixture.c', '-o', B / 'fixture.o'])
run('fixture-link', ['g++-13', B / 'fixture.o', B / 'bridge.o', '-L' + str(LB), '-Wl,-rpath,' + str(LB), '-lggml-base', '-lm', '-o', B / 'fixture'])
run('fixture', [B / 'fixture'])
print('EXP323_LOCAL_GATES_PASS')
