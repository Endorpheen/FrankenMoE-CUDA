"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-288-goose-cuda-out-prod'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('device ignores the src0 dim-2 grouping', 'device',
     'let ea:i64=(i3/dps3)*s03+(i2/dps2)*s02;', 'let ea:i64=(i3/dps3)*s03+(i2)*s02;'),
    ('device multiplies by dps3 instead of dividing', 'device',
     'let ea:i64=(i3/dps3)*s03+(i2/dps2)*s02;', 'let ea:i64=(i3*dps3)*s03+(i2/dps2)*s02;'),
    ('device swaps the b strides', 'device',
     'let eb:i64=i3*s13+i2*s12;', 'let eb:i64=i3*s12+i2*s13;'),
    ('device swaps the c strides', 'device',
     'let ec:i64=i3*s3+i2*s2;', 'let ec:i64=i3*s2+i2*s3;'),
    ('device drops the float scale on the a pointer', 'device',
     'src0+(ea*(4 as i64)) as u64', 'src0+ea as u64'),
    ('device drops i2 from the flat index', 'device',
     'let idx:i64=(i3*ne2+i2)*(8 as i64);', 'let idx:i64=(i3*ne2)*(8 as i64);'),
    ('host treats a grouped src0 as the strided branch', 'host',
     'if dps2==(1 as i64){', 'if dps2>(1 as i64){'),
    ('host batches only ne2 for the pointer branch', 'host',
     'if branch==(1 as i64){ batch=ne2*ne3; }', 'if branch==(1 as i64){ batch=ne2; }'),
    ('host defaults the src1 op to N', 'host',
     'var op:i64=1 as i64;', 'var op:i64=0 as i64;'),
    ('host halves the lda divisor', 'host',
     'let lda:i64=nb01/tsz;', 'let lda:i64=nb01/(2 as i64);'),
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
