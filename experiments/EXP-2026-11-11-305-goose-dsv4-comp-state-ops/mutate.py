"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-305-goose-dsv4-comp-state-ops'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('sequence stream range widens to two streams', 'host',
     '     s0=seq_id;\n     ns=1 as i64;', '     s0=seq_id;\n     ns=2 as i64;'),
    ('whole-state range collapses to one stream', 'host',
     '   ns=n_stream;', '   ns=1 as i64;'),
    ('destination range accepts a mismatched stream count', 'host',
     '    if ns!=(1 as i64){ ret=(0 as i64)-1 as i64; }else{', '    if ns<(1 as i64){ ret=(0 as i64)-1 as i64; }else{'),
    ('clear column ids drop the stream stride', 'host',
     '       setat(out,cnt,dd*n_stream+seq_id);', '       setat(out,cnt,dd+seq_id);'),
    ('clear-all plane count loses one depth', 'host',
     '      ret=n_stream*((1 as i64)+n_rs_seq);', '      ret=n_stream*n_rs_seq;'),
    ('copying a sequence onto itself still queues', 'host',
     '     if src==dst{ ret=0 as i64; }else{', '     if false{ ret=0 as i64; }else{'),
    ('apply copies swap source and destination', 'host',
     '       setat(out,(2 as i64*i),at(arr2,i));       // dst\n       setat(out,(2 as i64*i+1 as i64),at(arr,i)); // src',
     '       setat(out,(2 as i64*i),at(arr,i));       // dst\n       setat(out,(2 as i64*i+1 as i64),at(arr2,i)); // src'),
    ('write ids reject the deepest legal rollback', 'host',
     '         if rb>n_rs_seq{ ok=false; }else{', '         if rb>=n_rs_seq{ ok=false; }else{'),
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
