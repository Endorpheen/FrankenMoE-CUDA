"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-315-goose-hybrid-base'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('default attention filter follows the recurrent layers', 'host',
     '   if c!=(0 as i64){ fa=at(arr2,il); }else{ if recr==(0 as i64){ fa=1 as i64; } }',
     '   if c!=(0 as i64){ fa=at(arr2,il); }else{ if recr!=(0 as i64){ fa=1 as i64; } }'),
    ('default recurrent filter takes the non-recurrent layers', 'host',
     '   if d!=(0 as i64){ fr=at(arr3,il); }else{ if recr!=(0 as i64){ fr=1 as i64; } }',
     '   if d!=(0 as i64){ fr=at(arr3,il); }else{ if recr==(0 as i64){ fr=1 as i64; } }'),
    ('an explicit attention filter is ignored', 'host',
     '   if c!=(0 as i64){ fa=at(arr2,il); }else{ if recr==(0 as i64){ fa=1 as i64; } }',
     '   if false{ fa=at(arr2,il); }else{ if recr==(0 as i64){ fa=1 as i64; } }'),
    ('all-output batches use the equal split', 'host',
     '   if a!=(0 as i64){\n    ret=0 as i64;                    // split_seq', '   if false{\n    ret=0 as i64;                    // split_seq'),
    ('the sequential flag follows the wrong condition', 'host',
     '    if !unified{ setat(out,0 as i64,2 as i64); }   // non-sequential', '    if unified{ setat(out,0 as i64,2 as i64); }   // non-sequential'),
    ('the rollback window is one token short', 'host',
     '    if c>(0 as i64){ keep=c+(1 as i64); }', '    if c>(0 as i64){ keep=c; }'),
    ('the attention cache is prepared first', 'host',
     '     setat(out,ne,0 as i64); ne=ne+1 as i64;      // recurrent prepared\n     setat(out,ne,1 as i64); ne=ne+1 as i64;      // attention prepared',
     '     setat(out,ne,1 as i64); ne=ne+1 as i64;      // attention prepared\n     setat(out,ne,0 as i64); ne=ne+1 as i64;      // recurrent prepared'),
    ('empty attention heads are accepted', 'host',
     '     if b!=(0 as i64){', '     if b<(0 as i64){'),
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
