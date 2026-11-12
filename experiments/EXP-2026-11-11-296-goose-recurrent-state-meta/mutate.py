"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-296-goose-recurrent-state-meta'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('write drops the sequence count', 'host',
     '   if seq_id==(0 as i64)-1 as i64{ ns=seq_size(a,n,i,sw); }', ''),
    ('write skips every second sequence id', 'host',
     '     s=s+1 as i64;\n    }\n   }\n   i=i+1 as i64;', '     s=s+2 as i64;\n    }\n   }\n   i=i+1 as i64;'),
    ('read rejects an exactly full cache', 'host',
     ' if cell_count>n{ return 0 as i64; }', ' if cell_count>=n{ return 0 as i64; }'),
    ('read accepts a duplicate tail', 'host',
     '   if t!=(0 as i64)-1 as i64{ return 0 as i64; }', ''),
    ('read accepts a far out-of-range sequence id', 'host',
     '   if sid>=n_seq_max{ return 0 as i64; }', '   if sid>n_seq_max+(2 as i64){ return 0 as i64; }'),
    ('read leaves the used count at zero', 'host',
     ' st(a,used_off(n,sw),cell_count);', ' st(a,used_off(n,sw),0 as i64);'),
    ('read does not stage the source index', 'host',
     '  st(a,src_off(n,i),i);', '  st(a,src_off(n,i),0 as i64);'),
    ('read forgets the sequence bit', 'host',
     '   seq_set(a,n,i,sw,sid);', ''),
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
