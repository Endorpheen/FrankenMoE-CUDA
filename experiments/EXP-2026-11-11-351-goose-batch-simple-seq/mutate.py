"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-351-goose-batch-simple-seq'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('used tokens are taken again', 'host',
     ' while cur<n_tokens&&at(used,cur)!=(0 as i64){ cur=cur+(1 as i64); }', ' cur=0 as i64;'),
    ('the simple split stops one token early', 'host',
     '    if count>=n_ubatch{ go=0 as i64; }', '    if count+1 as i64>=n_ubatch{ go=0 as i64; }'),
    ('the simple split runs to the end regardless of size', 'host',
     '    if count>=n_ubatch{ go=0 as i64; }', ''),
    ('the sequential split accepts non-subsets', 'host',
     '       if (cur_set&s)!=s{ scan=1 as i64; }else{ scan=0 as i64; }', '       scan=0 as i64;'),
    ('the sequential split keeps the first set after moving', 'host',
     '     cur_set=at(seq_set,cur);', ''),
    ('the sequential split ignores the size limit', 'host',
     '   if count>=n_ubatch{ go2=0 as i64; }else{', '   if false{\n   }else{'),
    ('an exhausted batch reports a token', 'host',
     '  emit(out,cntp,3 as i64,0 as i64,0 as i64);', '  emit(out,cntp,0 as i64,0 as i64,0 as i64);'),
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
