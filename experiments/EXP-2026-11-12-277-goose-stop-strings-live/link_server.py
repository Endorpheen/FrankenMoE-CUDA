#!/usr/bin/env python3
"""EXP269: relink libllama-server-impl.so with the EXP-268 policy object substituted into the static archive."""
from pathlib import Path
import shlex, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
E = F / 'experiments/EXP-2026-11-12-277-goose-stop-strings-live'
B = E / 'build'
B.mkdir(exist_ok=True)

src_ar = FL / 'build-cuda/tools/server/libserver-context.a'
dst_ar = B / 'libserver-context.a'
shutil.copy2(src_ar, dst_ar)
for a in (['ar', 'd', str(dst_ar), 'server-context.cpp.o'],
          ['ar', 'r', str(dst_ar), str(E / 'server-context-goose.o')]):
    q = subprocess.run(a, capture_output=True, text=True)
    print(a[1], q.returncode, q.stderr.strip()[:200])
    if q.returncode:
        sys.exit(1)

raw = subprocess.run(['ninja', '-t', 'commands', 'bin/libllama-server-impl.so'], cwd=str(FL / 'build-cuda'),
                     capture_output=True, text=True).stdout.strip().splitlines()[-1]
cmd = shlex.split(raw)
while cmd and cmd[0] in (':', '&&'):
    cmd = cmd[1:]
while cmd and cmd[-1] in (':', '&&'):
    cmd = cmd[:-1]
out = B / 'libllama-server-impl.so'
for i, a in enumerate(cmd):
    if a.endswith('tools/server/libserver-context.a'):
        cmd[i] = str(dst_ar)
    if a == 'bin/libllama-server-impl.so':
        cmd[i] = str(out)
cmd.append(str(E / 'bridge.o'))
q = subprocess.run(cmd, cwd=str(FL / 'build-cuda'), capture_output=True, text=True)
(E / 'link.stderr').write_text(q.stderr)
print('link rc =', q.returncode)
if q.returncode:
    print(q.stderr[-2500:]); sys.exit(1)
print('linked', out, out.stat().st_size)
