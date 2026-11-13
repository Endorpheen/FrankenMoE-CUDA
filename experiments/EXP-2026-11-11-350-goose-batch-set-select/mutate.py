"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-350-goose-batch-set-select'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('used rows are accepted', 'host',
     '  if at(used,i)!=(0 as i64){', '  if false{'),
    ('overlapping sets are accepted', 'host',
     '    if (cs&ss)!=(0 as i64){ add=0 as i64; }', ''),
    ('sequential order is not enforced', 'host',
     '      if at(seq_first,i)!=last_seq+(1 as i64){ add=0 as i64; }', ''),
    ('the break happens at the limit, not one past it', 'host',
     '     if chosen>n_ubatch{ stop=1 as i64; }', '     if chosen>=n_ubatch{ stop=1 as i64; }'),
    ('the cursor does not move past used rows', 'host',
     '   if at(used,row)==(0 as i64){ go=0 as i64; }else{ cur=cur+(1 as i64); }', '   go=0 as i64;'),
    ('the cursor skips one row too many', 'host',
     '  emit(out,cntp,4 as i64,s,cur);', '  emit(out,cntp,4 as i64,s,cur+(1 as i64));'),
    ('the final count is reported as zero', 'host',
     ' emit(out,cntp,5 as i64,n_seqs,0 as i64);', ' emit(out,cntp,5 as i64,0 as i64,0 as i64);'),
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
