"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-321-goose-kv-state-ranges'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('empty cells are written', 'host',
     '   if at(cpos,base+i)==((0 as i64)-1 as i64){ add=false; }', ''),
    ('the sequence bit is not checked', 'host',
     '     if (at(cseqw,(base+i)*W+wi)&bit_of(seq_id))==(0 as i64){ add=false; }', ''),
    ('the sliding window is not checked', 'host',
     '     if is_masked{ add=false; }', ''),
    ('a range remembers the wrong start', 'host',
     '    if begin==n_cells{ begin=i; }', '    if begin==(0 as i64){ begin=i; }'),
    ('the last range ends one cell early', 'host',
     '   setat(outr,(2 as i64*rr+1 as i64),n_cells);', '   setat(outr,(2 as i64*rr+1 as i64),n_cells-(1 as i64));'),
    ('the per-stream count is not incremented', 'host',
     '    cnt=cnt+1 as i64;', ''),
    ('partial-only never drops a section', 'host',
     '  if swa_type==(0 as i64){', '  if false{'),
    ('the sliding window also applies to all sequences', 'host',
     '    if seq_id!=(0 as i64)-1 as i64{\n     let is_masked:bool=is_masked_swa(n_swa,swa_type,at(cpos,base+i),at(spmax,s));',
     '    if true{\n     let is_masked:bool=is_masked_swa(n_swa,swa_type,at(cpos,base+i),at(spmax,s));'),
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
