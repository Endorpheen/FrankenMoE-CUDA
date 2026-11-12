"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-342-goose-kpool-layout-full'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('the previous last cell is ignored when resuming', 'host',
     '     if cp==last_pos&&cc>last_cell{ after=true; }', ''),
    ('a changed first position does not force a rebuild', 'host',
     '    if at(prev_pos,offp)!=at(cell_pos,offc){ rebuild=1 as i64; }', ''),
    ('a stale sequence keeps its layout', 'host',
     '   if at(stale,s)!=pos_clean{ rebuild=1 as i64; }', ''),
    ('a count mismatch does not force a rebuild', 'host',
     '    if n_final!=nc{ rebuild=1 as i64; }', ''),
    ('the stream bound is inclusive', 'host',
     '  if unified==(0 as i64)&&s>=n_stream_kv{', '  if unified==(0 as i64)&&s>n_stream_kv{'),
    ('order mode advances one cell at a time', 'host',
     '     n_pools=n_pools+1 as i64;\n     j=j+kpool;\n    }\n   }else{', '     n_pools=n_pools+1 as i64;\n     j=j+1 as i64;\n    }\n   }else{'),
    ('position mode ignores the pool alignment', 'host',
     '     if (p0-pos_min)%kpool!=(0 as i64){\n      j=j+1 as i64;\n     }else{', '     if false{\n      j=j+1 as i64;\n     }else{'),
    ('a rebuild keeps the length and minimum from the resume attempt', 'host',
     '   if rebuild!=(0 as i64){\n    n_final=nc;\n    pos_min=0 as i64;\n    if nc>(0 as i64){ pos_min=at(cell_pos,offc); }\n   }\n', ''),
    ('a resumed sequence starts its scan at zero', 'host',
     '   if rebuild==(0 as i64){ j=at(prev_j_next,s); }', ''),
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
