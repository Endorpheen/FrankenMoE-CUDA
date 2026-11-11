"""Mutation gate for the cooperative softmax module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-285-goose-cuda-softmax-coop'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('grid stride drops blockDim', 'device', 'let step:i64=gx*blk;', 'let step:i64=gx;'),
    ('global max load misses the last CTA', 'device', 'if tid<gx as i64{ gm=ld_f32(tmpmax,((tid*4 as i64) as u64)); }', 'if tid<gx-1 as i64{ gm=ld_f32(tmpmax,((tid*4 as i64) as u64)); }'),
    ('exp pass subtracts the CTA max instead of the global max', 'device', 'let t:f32=expf(v0-gm);', 'let t:f32=expf(v0-lm);'),
    ('divide pass skips the division', 'device', 'st_f32(dr,((i0*4 as i64) as u64),v0/ts);', 'st_f32(dr,((i0*4 as i64) as u64),v0);'),
    ('host block 256 -> 128', 'host', 'put(p,1,0,256 as u64);', 'put(p,1,0,128 as u64);'),
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
        t = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=1800)
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
