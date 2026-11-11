"""Mutation gate for the softmax-with-sinks module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-282-goose-cuda-softmax-sink'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('sink term dropped from the denominator', 'tmp=tmp+expf(ld_f32(sinks,((i02*4 as i64) as u64))-maxv);', 'tmp=tmp;'),
    ('sink exponent adds the max instead of subtracting', 'tmp=tmp+expf(ld_f32(sinks,((i02*4 as i64) as u64))-maxv);', 'tmp=tmp+expf(ld_f32(sinks,((i02*4 as i64) as u64))+maxv);'),
    ('f16 mask offset divides by 4 instead of 2', '/(2 as i64);', '/(4 as i64);'),
    ('mask term drops the alibi slope', 'let val:f32=xv*scale+(slope*mval);', 'let val:f32=xv*scale+mval;'),
    ('f32 entry passes bx as the sink index', 'moff,0 as i64,by,blk,tid,bx,by,bz,gw,gh,use_shared);', 'moff,0 as i64,bx,blk,tid,bx,by,bz,gw,gh,use_shared);'),
]

rows = []
try:
    for name, old, new in MUTATIONS:
        assert old in orig, ('anchor missing', name)
        g.write_text(orig.replace(old, new, 1))
        e = subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True, text=True)
        if e.returncode:
            rows.append(dict(mutation=name, status='EMIT_FAILED')); print(name,'EMIT_FAILED'); continue
        t = subprocess.run(['python3', str(E/'test_local.py')], capture_output=True, text=True, timeout=1800)
        out = t.stdout + t.stderr
        fails = sum(int(x) for x in re.findall(r'failures=(\d+)', out)) + sum(int(x) for x in re.findall(r'launch_fail=(\d+)', out))
        rows.append(dict(mutation=name, status='CAUGHT' if fails>0 else 'MISSED', failures=fails))
        print(name, 'failures=', fails)
finally:
    g.write_text(orig)
    subprocess.run(['python3', str(E/'emit_sources.py')], capture_output=True)
    subprocess.run(['python3', str(E/'test_local.py')], capture_output=True)

(E/'mutations.json').write_text(json.dumps(rows, indent=2)+'\n')
missed = [x for x in rows if x['status'] != 'CAUGHT']
print('MISSED' if missed else 'ALL_MUTATIONS_CAUGHT', len(rows))
raise SystemExit(1 if missed else 0)
