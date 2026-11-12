"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-310-goose-hybrid-idx-kpool'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('pad returns one extra block', 'host',
     ' if p<(64 as i64){ return 64 as i64; }\n return p;', ' if p<(64 as i64){ return 64 as i64; }\n return p+(64 as i64);'),
    ('pad rounds one block too far', 'host',
     ' let q:i64=(x+(63 as i64))/(64 as i64);', ' let q:i64=(x+(64 as i64))/(64 as i64);'),
    ('rank lower_bound uses <= ', 'host',
     '  if mp==pos{ if mc<cell{ less=true; } }', '  if mp==pos{ if mc<=cell{ less=true; } }'),
    ('rebuild keeps the old scan point', 'host',
     '     j=0 as i64;', '     j=prev_j_next;'),
    ('resume accepts a shrinking sequence', 'host',
     '      if n>=prev_n{', '      if n>prev_n{'),
    ('position grouping lets a misaligned offset start a pool', 'host',
     '      if (p0-pos_min)%kpool!=(0 as i64){', '      if (p0-pos_min)%kpool!=(1 as i64){'),
    ('position grouping only checks the first cell', 'host',
     '        if at(out,100 as i64+j+kk)!=p0+kk{ ok=false; kk=kpool; }else{ kk=kk+1 as i64; }',
     '        if false{ ok=false; kk=kpool; }else{ kk=kk+1 as i64; }'),
    ('order grouping advances one cell too far', 'host',
     '      j=j+kpool;\n     }\n    }else{', '      j=j+kpool+(1 as i64);\n     }\n    }else{'),
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
