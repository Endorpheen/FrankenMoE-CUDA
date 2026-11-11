"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-316-goose-kv-kq-mask'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('no cell is recorded for reuse', 'host',
     '       if !prev{', '       if false{'),
    ('row reuse skips the first cell', 'host',
     '    let src:i64=at(srct,sid);\n    var q:i64=0;', '    let src:i64=at(srct,sid);\n    var q:i64=1;'),
    ('causal mask keeps the token itself out of range', 'host',
     '       if causal{ if pcell>p1{ drop=true; } }', '       if causal{ if pcell>p1-(1 as i64){ drop=true; } }'),
    ('sliding window is not applied', 'host',
     '        if swa_on{ if is_masked_swa(n_swa,swa_type,pcell,p1){ drop=true; } }', '        if false{ if is_masked_swa(n_swa,swa_type,pcell,p1){ drop=true; } }'),
    ('cells without the sequence still match', 'host',
     '      if !has{\n       setat(mask,idst+j,neg_inf());\n      }else{', '      if false{\n       setat(mask,idst+j,neg_inf());\n      }else{'),
    ('row reuse copies one element everywhere', 'host',
     '     setat(mask,idst+q,at(mask,n_kv*src+q));', '     setat(mask,idst+q,at(mask,n_kv*src));'),
    ('sequence minimum becomes the maximum', 'host',
     '  if p0v<at(smin,sid0){ setat(smin,sid0,p0v); }', '  if p0v>at(smin,sid0){ setat(smin,sid0,p0v); }'),
    ('the recorded count is not stored', 'host',
     '         setat(counts,sid,nidx);', ''),
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
