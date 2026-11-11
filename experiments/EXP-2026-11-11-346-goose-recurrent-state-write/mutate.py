"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-346-goose-recurrent-state-write'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an empty cell is selected for a whole-cache write', 'host',
     '   if p3!=(0 as i64)-1 as i64{ sel3=1 as i64; }', '   sel3=1 as i64;'),
    ('a cell of another sequence is selected', 'host',
     '    if (m3&((1 as i64)<<seq_id))!=(0 as i64){ sel3=1 as i64; }', '    sel3=1 as i64;'),
    ('data ranges merge across a gap', 'host',
     '     if d_end==cid2{ d_end=cid2+(1 as i64); }else{', '     if true{ d_end=cid2+(1 as i64); }else{'),
    ('the previous source of a cell is ignored', 'host',
     '    if sc2>=(0 as i64){ row2=sc2; }', ''),
    ('the rollback index does not shift the row block', 'host',
     '    let cid2:i64=rsc2*size+row2;', '    let cid2:i64=row2;'),
    ('disagreeing rollback indices in a shared cell pass', 'host',
     '         if cur!=cv{ done(packet,cntp,1 as i64); return true; }', ''),
    ('device saves accept several ranges', 'host',
     '   if n_ranges>(1 as i64){', '   if false{'),
    ('a failed read clears with the requested count', 'host',
     '   if meta_ok!=(0 as i64){ cc2=read_count; }', '   cc2=read_count;'),
    ('a successful read forgets to reset the rollback index', 'host',
     '  if n_rs_seq!=(0 as i64){ emit(out,cntp,7 as i64,seq_id,0 as i64); }', ''),
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
