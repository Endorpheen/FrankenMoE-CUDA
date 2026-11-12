"""Mutation gate for the softmax device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-280-goose-cuda-softmax'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('block max tracks the last value instead of the maximum', 'device', 'maxv=fmaxf(maxv,val);', 'maxv=val;'),
    ('warp max ladder adds instead of taking the maximum', 'device', 'v=fmaxf(v,t);', 'v=v+t;'),
    ('exp pass forgets to subtract the row max', 'device', 'let e:f32=expf(vv-maxv);', 'let e:f32=expf(vv);'),
    ('inverse sum uses 2.0 instead of 1.0', 'device', 'let inv:f32=1.0 as f32/tmp;', 'let inv:f32=2.0 as f32/tmp;'),
    ('host doubling cap 1024 -> 2048', 'host', 'nth<ncols as i64&&nth<1024 as i64', 'nth<ncols as i64&&nth<2048 as i64'),
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
