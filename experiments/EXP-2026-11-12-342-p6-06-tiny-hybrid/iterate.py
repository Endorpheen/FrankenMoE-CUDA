#!/usr/bin/env python3
"""Drive the generator against the real loader: fix shapes from the loader's 'expected' line, up to N rounds."""
import re, subprocess, sys
from pathlib import Path
E = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/experiments/EXP-2026-11-12-342-p6-06-tiny-hybrid')
GEN = E/'make_tiny.py'; GGUF = E/'tiny-hybrid.gguf'
BIN = '/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/bin'
LIB = BIN
def run():
    p = subprocess.run(['env','-i','PATH=/usr/bin:/bin',f'LD_LIBRARY_PATH={LIB}','HOME=/tmp','timeout','40',
                        f'{BIN}/llama-server','-m',str(GGUF),'-c','256','-ngl','0','--port','8051'],
                       capture_output=True, text=True)
    out = p.stdout + p.stderr
    for l in out.splitlines():
        if 'listening on' in l: return ('ok', None, None)
        m = re.search(r"tensor '([^']+)' has wrong shape; expected\s+([\d,\s]+?)got", l)
        if m: return ('shape', m.group(1), tuple(int(x) for x in m.group(2).replace(' ','').strip(',').split(',') if x.strip()))
        m = re.search(r"tensor '([^']+)' not found", l)
        if m: return ('missing', m.group(1), None)
    return ('other', None, None)
for r in range(1, 16):
    kind, name, dims = run()
    print(f'round {r}: {kind} {name} {dims}')
    if kind == 'ok':
        print('MODEL LOADS'); break
    if kind != 'shape':
        break
    s = GEN.read_text()
    suffix = re.sub(r'^blk\.\d+\.', '', name)
    if name.startswith('blk.'):
        pat = re.compile(r'T\(pre\+"' + re.escape(suffix) + r'", \([^)]*\)\)')
        new = f'T(pre+"{suffix}", {dims})'
    else:
        pat = re.compile(r'T\("' + re.escape(name) + r'", \([^)]*\)\)')
        new = f'T("{name}", {dims})'
    if pat.search(s):
        s = pat.sub(new, s, count=1)
    else:
        # insert before the pack of ffn tensors in the layer block, or after the first T( line
        anchor = '    T(pre+"ffn_gate_inp.weight"'
        s = s.replace(anchor, '    ' + new + '\n' + anchor, 1)
    GEN.write_text(s)
    subprocess.run([sys.executable, str(GEN), str(GGUF)], capture_output=True)
