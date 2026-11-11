"""Mutation gate for the argsort device + host module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-283-goose-cuda-argsort'
files = {'device': E/'source'/'device.goose', 'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('ASC first-branch compare uses >= (ties swap)', 'device',
     'if ld_f32(x,((a*4 as i64) as u64))>ld_f32(x,((b*4 as i64) as u64)){ doswap=1 as i64; }',
     'if ld_f32(x,((a*4 as i64) as u64))>=ld_f32(x,((b*4 as i64) as u64)){ doswap=1 as i64; }'),
    ('direction bit uses j instead of k', 'device', 'if ((col&k)==(0 as i64)){', 'if ((col&j)==(0 as i64)){'),
    ('padding test a>=ncols weakened to a>ncols', 'device', 'if a>=ncols as i64{', 'if a>ncols as i64{'),
    ('swap writes the second value to the wrong slot', 'device',
     'sstore_i32(dr+((ixj*4 as i64) as u64),a as i32);', 'sstore_i32(dr+((col*4 as i64) as u64),a as i32);'),
    ('host block cap 1024 -> 512', 'host', 'if np2<1024 as i64{ blk=np2; }', 'if np2<512 as i64{ blk=np2; }'),
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
