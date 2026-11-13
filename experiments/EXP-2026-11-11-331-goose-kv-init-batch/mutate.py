"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-331-goose-kv-init-batch'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('a single stream splits by sequence', 'host',
     ' if n_stream==(1 as i64){ strategy=0; }else{', ' if n_stream==(1 as i64){ strategy=1; }else{'),
    ('a single-assignment batch splits equally', 'host',
     '  if embd_all!=(0 as i64){ strategy=1; }', ''),
    ('the drain ignores the zero ubatch', 'host',
     '  if t==(0 as i64){\n   i=n_ub;   // the zero ubatch ends the drain\n  }else{', '  if false{\n  }else{'),
    ('unplaced tokens are accepted', 'host',
     ' if n_used<n_tokens{', ' if false{'),
    ('a refused preparation is accepted', 'host',
     ' if prepare_ok==(0 as i64){', ' if false{'),
    ('a failed batch reports success', 'host',
     '  put(packet,3,1,1 as i64 as u64);\n  return true;\n }\n\n if prepare_ok',
     '  put(packet,3,1,0 as i64 as u64);\n  return true;\n }\n\n if prepare_ok'),
    ('the zero ubatch is counted as taken', 'host',
     '  if t==(0 as i64){\n   i=n_ub;', '  if t==(0 as i64){\n   taken=taken+1 as i64;\n   i=n_ub;'),
    ('the strategy is never reported', 'host',
     ' emit(out,cntp,0 as i64,strategy,0 as i64);', ' emit(out,cntp,0 as i64,2 as i64,0 as i64);'),
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
