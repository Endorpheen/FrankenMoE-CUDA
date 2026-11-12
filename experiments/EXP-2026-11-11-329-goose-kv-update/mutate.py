"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-329-goose-kv-update'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a mirrored cache does work', 'host',
     '  if other!=(0 as i64){ put(packet,3,0,1 as u64); put(packet,3,1,(1 as i64) as u64); return true; }', ''),
    ('values are copied even where none exist', 'host',
     '     if v_present!=(0 as i64){ emit(out,cntp,1 as i64,ssrc,sdst); }', '     emit(out,cntp,1 as i64,ssrc,sdst);'),
    ('only one layer is copied', 'host',
     '    var il:i64=0;\n    while il<n_layers{', '    var il:i64=0;\n    while il<1 as i64{'),
    ('the shift counters are not reset', 'host',
     '   var s:i64=0;\n   while s<n_stream{\n    emit(out,cntp,3 as i64,s,0 as i64);', '   var s:i64=0;\n   while s<0 as i64{\n    emit(out,cntp,3 as i64,s,0 as i64);'),
    ('a shift runs without RoPE', 'host',
     '   if rope_enabled!=(0 as i64){', '   if true{'),
    ('a shift runs without any layer using RoPE', 'host',
     '    if any_rope!=(0 as i64){', '    if true{'),
    ('a failed compute still reports an update', 'host',
     '      if compute_ok==(0 as i64){\n       // compute failed: no shift ran, counters still reset below\n      }else{',
     '      if false{\n      }else{'),
    ('Step35 is allowed to shift', 'host',
     '   if step35!=(0 as i64){ ok=0 as i64; }', ''),
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
