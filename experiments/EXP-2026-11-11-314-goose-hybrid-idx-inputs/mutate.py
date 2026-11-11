"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-314-goose-hybrid-idx-inputs'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('absent sequences are treated as present', 'host',
     '    if at(seq_in_ub,s)==(0 as i64){ inert=true; }', '    if false{ inert=true; }'),
    ('the representative is the first member, not the last', 'host',
     '    let rep_off:i64=j+kpool-(1 as i64);', '    let rep_off:i64=j;'),
    ('padded scatter entries may reuse a representative cell', 'host',
     '     if at(pcell,q)==padc{ taken=true; }', '     if false{ taken=true; }'),
    ('pooled position comes from the last member', 'host',
     '     setat(npos,0 as i64*n_pool_real+i_new,at(cpos,base+j));', '     setat(npos,0 as i64*n_pool_real+i_new,at(cpos,base+rep_off));'),
    ('order mode visible count skips the partial pool', 'host',
     '     nv=(r+(1 as i64))/kpool;', '     nv=r/kpool;'),
    ('order mode tail keeps one cell too many', 'host',
     '     n_tail=(r2+(1 as i64))%kpool;', '     n_tail=(r2+(2 as i64))%kpool;'),
    ('selection keeps the tail slots as visible', 'host',
     '     var z2:i64=0;\n     while z2<nvc*kpool{', '     var z2:i64=0;\n     while z2<nvc{'),
    ('position mode tail drops the inclusive one', 'host',
     '     n_tail=(p-pos_min+(1 as i64))%kpool;', '     n_tail=(p-pos_min)%kpool;'),
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
