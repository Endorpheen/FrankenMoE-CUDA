"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-332-goose-kv-apply-ubatch'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the overwritten position recorded is the lowest, not the highest', 'host',
     '    if p>cur{ setat(maxrm,found,p); }', '    if p<cur{ setat(maxrm,found,p); }'),
    ('an occupied destination is not emptied', 'host',
     '    setat(cmask,g,0 as i64);\n    setat(cpos,g,(0 as i64)-1 as i64);', ''),
    ('the extension is written only for 2-D positions', 'host',
     '   if pos2d!=(0 as i64)||tok_present!=(0 as i64)||ple_n>(0 as i64){', '   if pos2d!=(0 as i64){'),
    ('mixed embeddings keep their token id', 'host',
     '     if is_mixed!=(0 as i64){\n      if at(ub_type,i)!=(0 as i64){ is_embd=true; }\n     }', ''),
    ('the PLE fallback prefers the image token', 'host',
     '      if ple_img!=(0 as i64){ tok=ple_img; }else{ tok=ple_eos; }', '      if ple_img!=(0 as i64){ tok=ple_eos; }else{ tok=ple_img; }'),
    ('the purge is skipped', 'host',
     '    if mn<=m{', '    if false{'),
    ('only positions strictly above the minimum are purged', 'host',
     '    if mn<=m{', '    if mn<m{'),
    ('the head stops at the last slot instead of past it', 'host',
     '  let v:i64=at(idxs,(s+1 as i64)*size-(1 as i64))+(1 as i64);', '  let v:i64=at(idxs,(s+1 as i64)*size-(1 as i64));'),
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
