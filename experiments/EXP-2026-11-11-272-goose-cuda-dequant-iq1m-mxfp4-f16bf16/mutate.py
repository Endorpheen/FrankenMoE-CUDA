"""Mutation gate for the IQ1_M / MXFP4 F16/BF16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-272-goose-cuda-dequant-iq1m-mxfp4-f16bf16'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('q1_m half pattern s3 mask 0xf000 -> 0x0f00', '(s3&(0xf000 as u32))', '(s3&(0x0f00 as u32))'),
    ('q1_m qh index drops il/2 (f16 body)', 'blk+32 as i64+2*ib+il/2', 'blk+32 as i64+2*ib'),
    ('mxfp4 E8M0 shift 23 -> 22 (f16 body)', 'bits=e<<sh;', 'bits=e<<(sh-1 as u32);'),
    ('mxfp4 drops the 0.5 factor (f16 body)', 'd*((v0) as f32)*(0.5 as f32)', 'd*((v0) as f32)'),
    ('bf16 store uses half conversion (q1_m body)', 'st_bf16(yy,(((ybase+j)*(2 as i64)) as u64),d*(((qb) as f32)+delta));', 'st_half(yy,(((ybase+j)*(2 as i64)) as u64),d*(((qb) as f32)+delta));'),
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
        m = re.search(r'failures=(\d+) launch_fail=(\d+)', out)
        fails = int(m.group(1)) + int(m.group(2)) if m else -1
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
