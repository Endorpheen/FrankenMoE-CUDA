"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-317-goose-kv-kq-mask-full'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('ALiBi keeps the signed distance', 'host',
     '         setat(mask,idst+j,(0 as i64)-d);', '         setat(mask,idst+j,d);'),
    ('ALiBi still reuses rows', 'host',
     '   if !alibi{\n    if at(srct,sid)!=((0 as i64)-1 as i64){', '   if true{\n    if at(srct,sid)!=((0 as i64)-1 as i64){'),
    ('2-D comparison ignores the y axis', 'host',
     '           if ey>ty{ drop=true; }', '           if false{ drop=true; }'),
    ('2-D comparison masks an equal y on the token', 'host',
     '            if ey==ty{ if ex>tx{ drop=true; } }', '            if ey>=ty{ if ex>=tx{ drop=true; } }'),
    ('non-causal span starts one position late', 'host',
     '           if pcell>=at(smin,sid){ in_span=true; }', '           if pcell>at(smin,sid){ in_span=true; }'),
    ('non-causal span is never applied', 'host',
     '          if nc_full{', '          if false{'),
    ('2-D comparison applies to every past cell', 'host',
     '          if pcell==p1{', '          if true{'),
    ('ALiBi distance loses its absolute value', 'host',
     '         if d<(0 as i64){ d=(0 as i64)-d; }', ''),
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
