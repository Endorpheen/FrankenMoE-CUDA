"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-334-goose-kv-prev-tokens'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the first cell at a position wins instead of the last', 'host',
     '     if cp>best_p||(cp==best_p&&i>best_i){', '     if cp>best_p{'),
    ('the target position itself is excluded', 'host',
     '    if cp<=p{', '    if cp<p{'),
    ('cells of other sequences count', 'host',
     '   if ((at(cmask,i)&((1 as i64)<<seq))!=(0 as i64)){', '   if true{'),
    ('only a token-less batch goes by order', 'host',
     '  if is_mixed!=(0 as i64){ by_order=1 as i64; }', ''),
    ('the order depth is added instead of subtracted', 'host',
     '    let k:i64=cb-d;', '    let k:i64=cb+d;'),
    ('the pre-chunk extension forgets the depth', 'host',
     '     p=at(ub_pos,v0)+k;', '     p=at(ub_pos,v0);'),
    ('the answers are laid out by token plus depth', 'host',
     '   emit(out,cntp,0 as i64,i*n+j,v);', '   emit(out,cntp,0 as i64,i+j,v);'),
    ('the order counts every token, not just the sequence', 'host',
     '  if at(ub_seq0,i)==seq{ seen=seen+1 as i64; }', '  seen=seen+1 as i64;'),
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
