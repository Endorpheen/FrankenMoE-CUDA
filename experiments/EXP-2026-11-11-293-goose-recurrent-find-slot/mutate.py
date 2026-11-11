"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-293-goose-recurrent-find-slot'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('head reset tolerance is wider', 'host',
     ' if head>used+2 as i64*n_seqs{ head=0; }', ' if head>used+3 as i64*n_seqs{ head=0; }'),
    ('shared sequences keep their tail', 'host',
     '   if j>0 as i64{', '   if j>=0 as i64{'),
    ('next-empty scan skips every second cell', 'host',
     '  next_empty=next_empty+1 as i64;', '  next_empty=next_empty+2 as i64;'),
    ('ownership test accepts a shared cell', 'host',
     '   if seq_size(a,n,t,sw)==(1 as i64){ has_cell=true; }', '   if seq_size(a,n,t,sw)>=(1 as i64){ has_cell=true; }'),
    ('swap does not move the sequence words to the source cell', 'host',
     '    st(a,seqw_off(n,src,sw,w),tw);', ''),
    ('final pass keeps the previous position', 'host',
     '  st(a,pos_off(cid),last);', '  st(a,pos_off(cid),ld(a,pos_off(cid)));'),
    ('zero-state scan starts above the range', 'host',
     ' var q:i64=lo;\n while q<=hi{\n  var rc:i64=0;', ' var q:i64=lo+(1 as i64);\n while q<=hi{\n  var rc:i64=0;'),
    ('range check rejects an exact fit', 'host',
     ' if nn>=n_seqs{ ret=1 as i64; }', ' if nn>n_seqs{ ret=1 as i64; }'),
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
