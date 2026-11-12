"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-311-goose-hybrid-idx-state'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('write puts the indexer first', 'host',
     '  setat(out,0 as i64,0 as i64);                 // base hybrid section', '  setat(out,0 as i64,1 as i64);                 // base hybrid section'),
    ('restore forgets to record the attention section', 'host',
     '    setat(out,ne,0 as i64); ne=ne+1 as i64;', '    ne=ne+0 as i64;'),
    ('restore reads the recurrent part before attention', 'host',
     '    if b!=(0 as i64){ failed=true; }else{\n     setat(out,ne,1 as i64); ne=ne+1 as i64;',
     '    if false{ failed=true; }else{\n     setat(out,ne,1 as i64); ne=ne+1 as i64;'),
    ('indexer restore does not invalidate the layout', 'host',
     '       if at(arr,0)!=0 as i64{', '       if at(arr,0)<0 as i64{'),
    ('failed restore does not drop the sequence', 'host',
     '    setat(out,ne,4 as i64); ne=ne+1 as i64;\n    ret=(0 as i64)-1 as i64;', '    ne=ne+1 as i64;\n    ret=(0 as i64)-1 as i64;'),
    ('dropping all records nothing', 'host',
     '     setat(out,0 as i64,7 as i64);              // clear(true)', '     '),
    ('drop forgets to invalidate the layout', 'host',
     '      setat(out,ne,3 as i64); ne=ne+1 as i64;   // stale_set(seq, 0)', ''),
    ('stream count drops the inclusive one', 'host',
     '      setat(out,i,s1-s0+(1 as i64));', '      setat(out,i,s1-s0);'),
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
