#!/usr/bin/env python3
"""Relink libllama-server-impl.so with the MERGED server-context object (four ports) and all four bridges."""
from pathlib import Path
import json, shlex, shutil, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
FL = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
M = F / 'runtime/reconstructed/server-merge'
B = M / 'build'; B.mkdir(exist_ok=True)

cc = json.loads((FL / 'build-cuda/compile_commands.json').read_text())
entry = [x for x in cc if x['file'].endswith('tools/server/server-context.cpp')][0]
args = shlex.split(entry['command']); out = []; skip = False
for a in args:
    if skip: skip = False; continue
    if a in ('-o', '-c'): skip = True; continue
    if a.endswith('server-context.cpp'): continue
    out.append(a)
obj = B / 'server-context-merged.o'
out += ['-c', str(M / 'server-context.cpp'), '-o', str(obj)]
q = subprocess.run(out, cwd=str(FL), capture_output=True, text=True)
(B / 'compile.stderr').write_text(q.stderr)
print('compile rc =', q.returncode)
if q.returncode: print(q.stderr[-1500:]); sys.exit(1)

src_ar = FL / 'build-cuda/tools/server/libserver-context.a'
dst_ar = B / 'libserver-context.a'
shutil.copy2(src_ar, dst_ar)
for a in (['ar', 'd', str(dst_ar), 'server-context.cpp.o'], ['ar', 'r', str(dst_ar), str(obj)]):
    r = subprocess.run(a, capture_output=True, text=True)
    if r.returncode: print('ar failed', r.stderr[:300]); sys.exit(1)

raw = subprocess.run(['ninja', '-t', 'commands', 'bin/libllama-server-impl.so'], cwd=str(FL / 'build-cuda'),
                     capture_output=True, text=True).stdout.strip().splitlines()[-1]
cmd = shlex.split(raw)
while cmd and cmd[0] in (':', '&&'): cmd = cmd[1:]
while cmd and cmd[-1] in (':', '&&'): cmd = cmd[:-1]
so = B / 'libllama-server-impl.so'
for i, a in enumerate(cmd):
    if a.endswith('tools/server/libserver-context.a'): cmd[i] = str(dst_ar)
    if a == 'bin/libllama-server-impl.so': cmd[i] = str(so)
for e in ('269-goose-checkpoint-policy-live', '273-goose-slot-selection-live',
          '275-goose-batch-compat-live', '277-goose-stop-strings-live'):
    cmd.append(str(F / 'experiments' / ('EXP-2026-11-12-' + e) / 'bridge.o'))
q = subprocess.run(cmd, cwd=str(FL / 'build-cuda'), capture_output=True, text=True)
(B / 'link.stderr').write_text(q.stderr)
print('link rc =', q.returncode, '| size =', so.stat().st_size if so.exists() else '-')
if q.returncode: print(q.stderr[-2000:]); sys.exit(1)
