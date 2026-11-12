"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-289-goose-kv-pos-maintenance'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('pos_add shifts by the wrong sign', 'host',
     'let sh:i64=get_shift(a,n,i)+d;', 'let sh:i64=get_shift(a,n,i)-d;'),
    ('pos_add keeps the sequence words when a cell is released', 'host',
     '  clear_seq(a,n,i);\n', ''),
    ('pos_div absorbs the wrong difference', 'host',
     'set_shift(a,n,i,get_shift(a,n,i)+(p0-p));', 'set_shift(a,n,i,get_shift(a,n,i)+(p-p0));'),
    ('insertion orders equal positions by lowest index', 'host',
     '     if get_sidx(a,n,s,k)>i{', '     if get_sidx(a,n,s,k)<i{'),
    ('removal forgets to shrink the count', 'host',
     ' set_scnt(a,n,seq_n,s,c-(1 as i64));', ' set_scnt(a,n,seq_n,s,c);'),
    ('tok_le ignores a cell exactly at the position', 'host',
     '  if get_spos(a,n,s,k)<=p{ j=k; }', '  if get_spos(a,n,s,k)<p{ j=k; }'),
    ('seq_pos_max returns the first entry', 'host',
     ' return get_spos(a,n,s,c-(1 as i64));', ' return get_spos(a,n,s,0 as i64);'),
    ('pos_add reports a released cell as kept', 'host',
     '  set_used(a,n,i,0 as i64);\n  return 1 as i64;', '  set_used(a,n,i,0 as i64);\n  return 0 as i64;'),
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
