"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-322-goose-kv-state-meta'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a named sequence is not matched', 'host',
     '    if cur==seq_id{ take=true; }', '    if false{ take=true; }'),
    ('an unnamed write carries nothing', 'host',
     '    if seq_id==(0 as i64)-1 as i64{ take=true; }', '    if false{ take=true; }'),
    ('the cell extension is not written', 'host',
     '   if has_ext!=(0 as i64){', '   if false{'),
    ('each written id is shifted by one', 'host',
     '      setat(out,ids_start+ids,cur);', '      setat(out,ids_start+ids,cur+(1 as i64));'),
    ('the position is not written', 'host',
     '   setat(out,w,at(cpos,i));', '   setat(out,w,0 as i64);'),
    ('the id count ignores the filter', 'host',
     '   setat(out,w+(1 as i64),ids);', '   setat(out,w+(1 as i64),(0 as i64));'),
    ('range ends are inclusive', 'host',
     '  var i:i64=lo;\n  while i<hi{', '  var i:i64=lo;\n  while i<=hi{'),
    ('ids start where the position is', 'host',
     '   var ids_start:i64=w+(2 as i64)+(has_ext*3 as i64);', '   var ids_start:i64=w+(2 as i64);'),
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
