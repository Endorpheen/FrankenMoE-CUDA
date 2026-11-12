"""Mutation gate for the softmax-back device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-284-goose-cuda-softmax-back'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('dot product drops the gradient factor', 'device',
     'dot=dot+ld_f32(dstf,(((base+col)*4 as i64) as u64))*ld_f32(grad,(((base+col)*4 as i64) as u64));',
     'dot=dot+ld_f32(dstf,(((base+col)*4 as i64) as u64));'),
    ('warp ladder starts at 8', 'device', 'off:i64=16 as i64;', 'off:i64=8 as i64;'),
    ('final expression drops the dstf factor', 'device', 'scale*(g-dot)*f', 'scale*(g-dot)'),
    ('dot loop strides by 16 instead of 32', 'device', 'col=col+32 as i64;', 'col=col+16 as i64;'),
    ('host block 32 -> 64', 'host', 'put(p,1,0,32 as u64);', 'put(p,1,0,64 as u64);'),
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
