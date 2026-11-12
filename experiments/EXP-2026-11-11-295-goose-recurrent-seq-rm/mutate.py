"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-295-goose-recurrent-seq-rm'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('rollback ignores the upper position bound', 'host',
     '   if p0>(0 as i64)&&p0<=cpos&&p1>cpos{', '   if p0>(0 as i64)&&p0<=cpos{'),
    ('rollback accepts one snapshot too far', 'host',
     '     if rollback>=(1 as i64)&&rollback<=n_rs_seq{', '     if rollback>=(1 as i64)&&rollback<n_rs_seq{'),
    ('pending rollback is treated as free', 'host',
     '    if get_rs(a,n,sw,seq_id)!=(0 as i64){ pending=true; }', ''),
    ('shared cell may roll back for one sequence', 'host',
     '    if seq_size(a,n,tail_id,sw)>(1 as i64){ return 0 as i64; }', '    if seq_size(a,n,tail_id,sw)>(2 as i64){ return 0 as i64; }'),
    ('full-range removal keeps the snapshot index', 'host',
     ' if rm_all{ set_rs_idx(a,n,sw,n_seq_max,n_rs_seq,seq_id,0 as i64); }', ''),
    ('tail is not invalidated for a removed cell', 'host',
     '   if p0<=cpos&&cpos<p1{\n    st(a,tail_off(n,seq_id),(0 as i64)-1 as i64);\n   }', ''),
    ('emptying a cell leaves the used count alone', 'host',
     '      st(a,used_off(n,sw),ld(a,used_off(n,sw))-(1 as i64));', ''),
    ('head moves forward instead of back', 'host',
     '  if new_head<ld(a,head_off(n,sw)){ st(a,head_off(n,sw),new_head); }', '  if new_head>ld(a,head_off(n,sw)){ st(a,head_off(n,sw),new_head); }'),
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
