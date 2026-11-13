"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-319-goose-kv-cpy-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('k copy skips the cross-stream reshape', 'host',
     '  if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }', ''),
    ('k cross-stream reshape covers one stream only', 'host',
     '  if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }',
     '  if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size); }'),
    ('plain-v view keeps one head per row', 'host',
     '    emit(out,cntp,0 as i64,gqa,ntok);\n    if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }\n    emit(out,cntp,4 as i64,0 as i64,0 as i64);',
     '    emit(out,cntp,0 as i64,nhead,ntok);\n    if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }\n    emit(out,cntp,4 as i64,0 as i64,0 as i64);'),
    ('transposed v copies when it could reshape', 'host',
     '    if ts*gqa==nb2{\n     emit(out,cntp,1 as i64,gqa,ntok);', '    if ts*gqa!=nb2{\n     emit(out,cntp,1 as i64,gqa,ntok);'),
    ('transposed v pads one element early', 'host',
     '    if gqa<v_ne0{ emit(out,cntp,3 as i64,v_ne0-gqa,0 as i64); }', '    if gqa<=v_ne0{ emit(out,cntp,3 as i64,v_ne0-gqa,0 as i64); }'),
    ('transposed v pads the whole cache width', 'host',
     '    if gqa<v_ne0{ emit(out,cntp,3 as i64,v_ne0-gqa,0 as i64); }', '    if gqa<v_ne0{ emit(out,cntp,3 as i64,v_ne0,0 as i64); }'),
    ('v indices count one per token', 'host',
     '    if v_trans==(0 as i64){ ret=ntok; }else{ ret=ntok*gqa_max; }', '    if v_trans==(0 as i64){ ret=ntok; }else{ ret=ntok; }'),
    ('k view drops the token row', 'host',
     '  emit(out,cntp,0 as i64,gqa,ntok);\n  if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }\n  emit(out,cntp,4 as i64,0 as i64,0 as i64);\n  ret=at(cntp,0 as i64);',
     '  emit(out,cntp,0 as i64,gqa,ntok-(1 as i64));\n  if n_stream>(1 as i64){ emit(out,cntp,1 as i64,gqa,kv_size*n_stream); }\n  emit(out,cntp,4 as i64,0 as i64,0 as i64);\n  ret=at(cntp,0 as i64);'),
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
