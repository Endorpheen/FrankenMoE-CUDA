"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-309-goose-hybrid-idx-stale'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('mark-all keeps the larger position', 'host',
     '   if at(arr,i)>p0{ setat(arr,i,p0); }', '   if at(arr,i)<p0{ setat(arr,i,p0); }'),
    ('single-sequence mark overwrites instead of minimising', 'host',
     ' if at(arr,seq_id)>p0{ setat(arr,seq_id,p0); }', ' setat(arr,seq_id,p0);'),
    ('first-position test is exclusive', 'host',
     ' if p0<=seq_min{ return 0 as i64; }', ' if p0<seq_min{ return 0 as i64; }'),
    ('negative sequence does not regroup from zero', 'host',
     ' if seq_id < (0 as i64){ return 0 as i64; }', ' if seq_id < (0 as i64){ return p0; }'),
    ('recurrent refusal does not stop the chain', 'host',
     '    if recr_ok==(0 as i64){', '    if false{'),
    ('negative shift marks the unshifted position', 'host',
     '      if shift < (0 as i64){ base=p0+shift; }', ''),
    ('seq_cp does not mark the destination', 'host',
     '       if seq_id==(0 as i64){ stale_set(stale,n_seq,p0,0 as i64); }               // cp: destination', ''),
    ('seq_keep marks only one sequence', 'host',
     '       if seq_id==(1 as i64){ stale_set(stale,n_seq,(0 as i64)-1 as i64,0 as i64); } // keep: all',
     '       if seq_id==(1 as i64){ stale_set(stale,n_seq,(0 as i64),0 as i64); } // keep: all'),
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
