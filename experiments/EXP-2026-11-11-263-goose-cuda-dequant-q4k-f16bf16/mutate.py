"""Mutation gate for the 32-element legacy dequantization callbacks."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-263-goose-cuda-dequant-q4k-f16bf16'
g = E/'source'/'device.goose'
orig = g.read_text()

MUTATIONS = [
    ('scale_min m uses q[j+4] -> q[j+3]', 'mv=(ld_u8(vx,((base+j+4 as i64) as u64))&63) as i64;', 'mv=(ld_u8(vx,((base+j+3 as i64) as u64))&63) as i64;'),
    ('q offset 32*il -> 16*il (f16 body)', 'blk+(16 as i64)+32*il+n*ir', 'blk+(16 as i64)+16*il+n*ir'),
    ('second half offset +32 -> +16 (f16 body)', '(ybase+l+32)*(2 as i64)', '(ybase+l+16)*(2 as i64)'),
    ('bf16 store uses half conversion', 'st_bf16(yy,(o0 as u64),d1*(lo as f32)-m1);', 'st_half(yy,(o0 as u64),d1*(lo as f32)-m1);'),
    ('affine sign -m1 -> +m1 (f16 body)', 'd1*(lo as f32)-m1', 'd1*(lo as f32)+m1'),
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
