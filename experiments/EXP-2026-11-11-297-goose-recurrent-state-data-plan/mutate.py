"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-297-goose-recurrent-state-data-plan'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('write skips the ple tensor', 'host',
     '  if at(pp,il)!=(0 as i64){', '  if (1 as i64)==(0 as i64){'),
    ('write offsets the s tensor without its row stride', 'host',
     '     wr(st_out,cntp,il*(4 as i64)+(1 as i64),(lo*at(srow,il)),sz);', '     wr(st_out,cntp,il*(4 as i64)+(1 as i64),lo,sz);'),
    ('write transposed element offsets are doubled', 'host',
     '      let off:i64=(lo+j*size_)*el;', '      let off:i64=(lo+j*size_)*(el*(2 as i64));'),
    ('write miscounts the layers', 'host',
     ' u32e(st_out,cntp,n_layer);', ' u32e(st_out,cntp,n_layer+(1 as i64));'),
    ('read accepts a higher tensor type', 'host',
     '   if tr!=at(rtype,il){', '   if tr<at(rtype,il){'),
    ('read rejects an exactly full cache', 'host',
     ' if cell_count>size_{', ' if cell_count>=size_{'),
    ('read offsets the r tensor by one byte', 'host',
     '    rd(st_out,cntp,il*(4 as i64),head*at(rrow,il),cell_count*at(rrow,il));', '    rd(st_out,cntp,il*(4 as i64),head*at(rrow,il)+(1 as i64),cell_count*at(rrow,il));'),
    ('read transposed rows lose the cell stride', 'host',
     '      rd(st_out,cntp,il*(4 as i64)+(1 as i64),(head+j*size_)*el,cell_count*el);', '      rd(st_out,cntp,il*(4 as i64)+(1 as i64),(head+j)*el,cell_count*el);'),
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
