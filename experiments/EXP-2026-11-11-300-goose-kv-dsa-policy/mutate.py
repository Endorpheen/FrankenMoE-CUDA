"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-300-goose-kv-dsa-policy'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('indexer keeps two key heads', 'host',
     '   st(out,il*(8 as i64),1 as i64);', '   st(out,il*(8 as i64),2 as i64);'),
    ('indexer rope is not NeoX', 'host',
     '  st(out,((n_layer+(1 as i64))*(8 as i64)),neox());', '  st(out,((n_layer+(1 as i64))*(8 as i64)),0 as i64);'),
    ('indexer uses the MLA filter', 'host',
     '    st(out,(2 as i64*il+1 as i64)*(8 as i64),at(filt_lid,il));', '    st(out,(2 as i64*il+1 as i64)*(8 as i64),at(filt_mla,il));'),
    ('seq_rm succeeds if either cache does', 'host',
     '    if a!=(0 as i64)&&b!=(0 as i64){ ret=1 as i64; }', '    if a!=(0 as i64)||b!=(0 as i64){ ret=1 as i64; }'),
    ('position observers read the indexer', 'host',
     '     ret=a;', '     ret=b;'),
    ('can_shift ignores the size match', 'host',
     '      if mla_shift!=(0 as i64)&&lid_shift!=(0 as i64)&&mla_size==lid_size{ ret=1 as i64; }', '      if mla_shift!=(0 as i64)&&lid_shift!=(0 as i64){ ret=1 as i64; }'),
    ('failed restore leaves the MLA part behind', 'host',
     '        mla_cleared=1 as i64;', '        mla_cleared=0 as i64;'),
    ('failed restore also loses the MLA read', 'host',
     '        lid_read=0 as i64;', '        lid_read=0 as i64;\n        mla_read=0 as i64;'),
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
