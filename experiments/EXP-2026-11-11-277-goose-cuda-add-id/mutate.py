"""Mutation gate for the add_id device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-277-goose-cuda-add-id'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('device src1 row uses nb01 instead of nb11', 'device', 'let s1row:i64=i11*nb11;', 'let s1row:i64=i11*nb01;'),
    ('device dst row drops the batch term', 'device', 'let drow:i64=i1*nb1+i2*nb2;', 'let drow:i64=i1*nb1;'),
    ('device adds src0 twice', 'device', '+ld_f32(src1,((s1row+i0*4 as i64) as u64))', '+ld_f32(src0,((s0row+i0*4 as i64) as u64))'),
    ('device loop drops the last column', 'device', 'while i0<ne0 as i64{', 'while i0<ne0-1 as i64{'),
    ('host thread cap 768 -> 767', 'host', 'if thr>768 as i64{ thr=768 as i64; }', 'if thr>768 as i64{ thr=767 as i64; }'),
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
        fails = sum(int(x) for x in re.findall(r'failures=(\d+)', out))
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
