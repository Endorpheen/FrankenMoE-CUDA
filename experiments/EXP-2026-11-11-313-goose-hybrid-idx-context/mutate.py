"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-313-goose-hybrid-idx-context'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('track accepts a zero pool size', 'host',
     ' if kpool<=(0 as i64){ return false; }', ' if kpool<(0 as i64){ return false; }'),
    ('track accepts an empty stream list', 'host',
     ' if ns_len<=(0 as i64){ return false; }', ''),
    ('stale marks are cleared on every step', 'host',
     '   if i_cur==(0 as i64){\n    if b!=(0 as i64){', '   if true{\n    if b!=(0 as i64){'),
    ('the step counter does not advance', 'host',
     '   i_cur=i_cur+(1 as i64);', ''),
    ('apply accepts either side succeeding', 'host',
     '    if a!=(0 as i64){ if b!=(0 as i64){ res=1 as i64; } }', '    if a!=(0 as i64){ res=1 as i64; }else{ if b!=(0 as i64){ res=1 as i64; } }'),
    ('apply does not pin the pool step', 'host',
     '      setat(out,ne,2 as i64); ne=ne+1 as i64;     // i_kpool pinned', ''),
    ('apply rebuilds even when a side failed', 'host',
     '    if res!=(0 as i64){', '    if res>=(0 as i64){'),
    ('n_stream accepts a step past the list', 'host',
     '      if i_cur>=n{ ret=(0 as i64)-1 as i64; }', '      if i_cur>n{ ret=(0 as i64)-1 as i64; }'),
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
