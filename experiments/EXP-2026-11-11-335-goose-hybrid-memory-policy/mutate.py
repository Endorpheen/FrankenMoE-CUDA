"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-335-goose-hybrid-memory-policy'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('an all-output batch splits equally', 'host',
     '  if embd_all!=(0 as i64){\n   mode=0 as i64;', '  if false{\n   mode=0 as i64;'),
    ('the unified flag is inverted', 'host',
     '   flag=(1 as i64)-unified;', '   flag=unified;'),
    ('the rollback window is one short', 'host',
     '   if n_rs_seq>(0 as i64){ window=n_rs_seq+(1 as i64); }', '   if n_rs_seq>(0 as i64){ window=n_rs_seq; }'),
    ('attention prepares before recurrent', 'host',
     '  emit(out,cntp,3 as i64,recr_ok,0 as i64);\n  if recr_ok==(0 as i64){\n   put(packet,2,0,1 as u64);\n   put(packet,2,1,1 as i64 as u64);\n   return true;\n  }\n  emit(out,cntp,4 as i64,attn_heads,0 as i64);',
     '  emit(out,cntp,4 as i64,attn_heads,0 as i64);\n  if attn_heads==(0 as i64){\n   put(packet,2,0,1 as u64);\n   put(packet,2,1,1 as i64 as u64);\n   return true;\n  }\n  emit(out,cntp,3 as i64,recr_ok,0 as i64);'),
    ('a refusing recurrent cache still runs attention', 'host',
     '   if recr_ok2==(0 as i64){\n    ret=0 as i64;\n   }else{', '   if false{\n    ret=0 as i64;\n   }else{'),
    ('the mutation pair runs recurrent first', 'host',
     '    emit(out,cntp,6 as i64,0 as i64,0 as i64);\n    emit(out,cntp,5 as i64,0 as i64,0 as i64);\n    ret=0 as i64;',
     '    emit(out,cntp,5 as i64,0 as i64,0 as i64);\n    emit(out,cntp,6 as i64,0 as i64,0 as i64);\n    ret=0 as i64;'),
    ('a failed restore leaves attention populated', 'host',
     '      emit(out,cntp,7 as i64,at(get(packet,1,1),0 as i64),0 as i64);', ''),
    ('the total minimum takes the smaller of the two', 'host',
     '      if recr_min>lo{ lo=recr_min; }', '      if recr_min<lo{ lo=recr_min; }'),
    ('the breakdown forgets recurrent-only keys', 'host',
     '        if seen==(0 as i64){', '        if false{'),
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
