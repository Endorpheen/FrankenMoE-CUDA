"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-333-goose-kv-shift-bucket'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an empty cell reports its stale shift', 'host',
     '    if at(pos,off+i)!=(0 as i64)-1 as i64{ v=at(shift,off+i); }', '    v=at(shift,off+i);'),
    ('every stream uses the first stream width', 'host',
     '    emit(out,cntp,0 as i64,s*sz+i,v);', '    emit(out,cntp,0 as i64,s*at(sizes,0 as i64)+i,v);'),
    ('the matrix copy forgets the element width', 'host',
     '   let bytes:i64=n_rot*n_rot*(4 as i64);', '   let bytes:i64=n_rot*n_rot;'),
    ('the matrix copy forgets the square', 'host',
     '   let bytes:i64=n_rot*n_rot*(4 as i64);', '   let bytes:i64=n_rot*(4 as i64);'),
    ('a width missing from the table is copied anyway', 'host',
     '   if rot_present==(0 as i64){', '   if false{'),
    ('a mismatched width is copied anyway', 'host',
     '   if n_rot_src!=n_rot{', '   if false{'),
    ('the shift is read without the stream offset', 'host',
     '    if at(pos,off+i)!=(0 as i64)-1 as i64{ v=at(shift,off+i); }', '    if at(pos,off+i)!=(0 as i64)-1 as i64{ v=at(shift,i); }'),
    ('the reported count is zero', 'host',
     '  ret=off;', '  ret=0 as i64;'),
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
        # A mutation that breaks an invariant trips a validation code and aborts before the
        # summary prints; that is detection too, so the exit status counts as a channel.
        caught = fails > 0 or t.returncode != 0
        rows.append(dict(mutation=name, status='CAUGHT' if caught else 'MISSED', failures=fails, exit_code=t.returncode))
        print(name, 'failures=', fails, 'rc=', t.returncode)
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
