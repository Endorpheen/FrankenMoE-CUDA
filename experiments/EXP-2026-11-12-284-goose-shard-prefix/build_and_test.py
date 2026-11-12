#!/usr/bin/env python3
from pathlib import Path
import subprocess, sys
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-12-284-goose-shard-prefix'
S, B = E / 'source', E / 'build'
B.mkdir(exist_ok=True)
def run(label, argv):
    q = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    (B / (label + '.stdout')).write_text(q.stdout); (B / (label + '.stderr')).write_text(q.stderr)
    print('---', label, 'rc=', q.returncode)
    out = (q.stdout + q.stderr).strip()
    if out: print(out[-2000:])
    if q.returncode: sys.exit(q.returncode)
run('generate', ['python3', E / 'emit_sources.py'])
run('fixture', ['gcc-13', '-O2', '-I' + str(S), '-c', E / 'tests/shard_fixture.c', '-o', B / 'fixture.o'])
run('link', ['gcc-13', B / 'fixture.o', '-lm', '-o', B / 'fixture'])
run('run', [B / 'fixture'])
print('PASS shard-prefix fixture')
