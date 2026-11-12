"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-340-goose-recurrent-accessors'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an empty sequence reports the largest position', 'host',
     '  if mn==((1 as i64)<<62 as i64){ mn=(0 as i64)-1 as i64; }', ''),
    ('the minimum is computed as a maximum', 'host',
     '    if at(cpos,i)<mn{ mn=at(cpos,i); }', '    if at(cpos,i)>mn{ mn=at(cpos,i); }'),
    ('the maximum starts at zero', 'host',
     '   var mx:i64=(0 as i64)-1 as i64;', '   var mx:i64=0 as i64;'),
    ('a negative id is taken as a sequence index', 'host',
     '    if seq_id<(0 as i64){', '    if false{'),
    ('the index bound is exclusive', 'host',
     '     ensure(idx<=n_rs_seq,61);          // inclusive bound', '     ensure(idx<n_rs_seq,61);'),
    ('the breakdown keeps only the last buffer size', 'host',
     '       if at(keys,j)==key{ total=total+at(sizes,j); }', '       if at(keys,j)==key{ total=at(sizes,j); }'),
    ('the breakdown repeats a key per buffer', 'host',
     '      if first!=(0 as i64){ emit(out,cntp,0 as i64,key,total); }', '      emit(out,cntp,0 as i64,key,total);'),
    ('null tensors count towards the family size', 'host',
     '       if at(p_present,i4)!=(0 as i64){ p=p+at(p_nb,i4); }', '       p=p+at(p_nb,i4);'),
    ('is_empty is decided by the total, not the list', 'host',
     '        if n_buf==(0 as i64){ empty=1 as i64; }', '        if total2==(0 as i64){ empty=1 as i64; }'),
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
