"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-287-goose-cuda-mmvf-halftypes'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('y pair base mixes elements and pairs', 'device',
     'let yo:i64=ybase+col2*(8 as i64);', 'let yo:i64=(yb+col2)*(8 as i64);'),
    ('half/bf16 element size becomes 4 bytes', 'device',
     'if dtype!=(0 as i64){ esz=2 as i64; }', 'if dtype!=(0 as i64){ esz=4 as i64; }'),
    ('half/bf16 pair stride becomes 8 bytes', 'device',
     'if dtype!=(0 as i64){ psz=4 as i64; }', 'if dtype!=(0 as i64){ psz=8 as i64; }'),
    ('half second lane reads the first element', 'device',
     'xc=ld_half(x,((xo+(2 as i64)) as u64));', 'xc=ld_half(x,(xo as u64));'),
    ('warp ladder skips the 8-offset stage', 'device',
     'v=v+shfl_xor(v,8 as i32,32 as i32);', 'v=v+shfl_xor(v,4 as i32,32 as i32);'),
    ('cross-warp slot uses the thread id', 'device',
     'shm_put((tid/32) as i32,sum);', 'shm_put(tid as i32,sum);'),
    ('host grid carries one extra channel', 'host',
     'put(p,1,3,nch as u64);', 'put(p,1,3,(nch+(1 as i64)) as u64);'),
    ('host shared memory is halved', 'host',
     'let shared:i64=warp*(4 as i64);', 'let shared:i64=warp*(2 as i64);'),
]

rows = []
try:
    for name, mod, old, new in MUTATIONS:
        src = orig[mod]
        assert old in src, ('anchor missing', name)
        files[mod].write_text(src.replace(old, new, 1))
        e = subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True, text=True)
        if e.returncode:
            rows.append(dict(mutation=name, status='EMIT_FAILED')); print(name,'EMIT_FAILED')
            files[mod].write_text(src); continue
        t = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=3600)
        out = t.stdout + t.stderr
        fails = sum(int(x) for x in re.findall(r'failures=(\d+)', out)) + sum(int(x) for x in re.findall(r'launch_fail=(\d+)', out))
        rows.append(dict(mutation=name, status='CAUGHT' if fails>0 else 'MISSED', failures=fails))
        print(name, 'failures=', fails)
        files[mod].write_text(src)
finally:
    for k, v in files.items():
        v.write_text(orig[k])
    subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True)
    subprocess.run(['python3', str(E/'test_local.py')], capture_output=True)

(E/'mutations.json').write_text(json.dumps(rows, indent=2)+'\n')
missed = [x for x in rows if x['status'] != 'CAUGHT']
print('MISSED' if missed else 'ALL_MUTATIONS_CAUGHT', len(rows))
raise SystemExit(1 if missed else 0)
