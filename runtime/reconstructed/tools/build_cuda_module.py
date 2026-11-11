#!/usr/bin/env python3
"""Compile one experiment's cuda-runtime.cu against the clang build's compile command."""
import json, shlex, subprocess, sys
from pathlib import Path
EXP, TARGET, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
S = F/f'experiments/{EXP}/source'
db = json.load(open('/tmp/cuda-build2/compile_commands.json'))
r = [x for x in db if x['file'].endswith('/'+TARGET)][0]
cmd = shlex.split(r['command'])
cmd[1:1] = ['-I'+str(Path(r['file']).parent), '-I'+str(Path(r['file']).parent.parent), '-I'+str(S)]
cmd[cmd.index('-c')+1] = str(S/'cuda-runtime.cu')
cmd[cmd.index('-o')+1] = OUT
cmd = [x for x in cmd if x != '-extended-lambda']
q = subprocess.run(cmd, cwd=r['directory'], capture_output=True, text=True, timeout=1800)
print(EXP, TARGET, 'rc', q.returncode)
if q.returncode:
    print((q.stdout+q.stderr)[-1200:]); sys.exit(1)
