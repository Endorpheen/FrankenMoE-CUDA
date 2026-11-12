"""Mutation gate for the mmvf half/bfloat16 module."""
from pathlib import Path
import subprocess, json, re

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F/'experiments/EXP-2026-11-11-308-goose-dsv4-cache-policy'
files = {'host': E/'source'/'host.goose'}
orig = {k: v.read_text() for k, v in files.items()}

MUTATIONS = [
    ('indexer keeps two key heads', 'host',
     '   setat(out,il,1 as i64);            // n_head_kv_arr', '   setat(out,il,2 as i64);            // n_head_kv_arr'),
    ('indexer SWA value width uses the key head', 'host',
     '  setat(out,5 as i64*n_layer+3 as i64,idx_head);   // v_swa', '  setat(out,5 as i64*n_layer+3 as i64,head_k);   // v_swa'),
    ('indexer rope is not NeoX', 'host',
     '  setat(out,5 as i64*n_layer+4 as i64,2 as i64);   // rope NeoX', '  setat(out,5 as i64*n_layer+4 as i64,0 as i64);   // rope NeoX'),
    ('CSA filter takes the HCA ratio', 'host',
     '     if r==(4 as i64){ f_csa=1 as i64; }', '     if r==(128 as i64){ f_csa=1 as i64; }'),
    ('CSA cache is sized with the HCA ratio', 'host',
     '    setat(out,0 as i64,pad256(comp_size(kv_size,4 as i64)));', '    setat(out,0 as i64,pad256(comp_size(kv_size,8 as i64)));'),
    ('HCA compressor state halves its rows', 'host',
     '     setat(out,4 as i64,128 as i64);', '     setat(out,4 as i64,64 as i64);'),
    ('raw cache becomes unified', 'host',
     '      setat(out,0 as i64,0 as i64);', '      setat(out,0 as i64,1 as i64);'),
    ('K-only leaves the value width alone', 'host',
     '      setat(out,3 as i64,head_k);', '      setat(out,3 as i64,head_k+(1 as i64));'),
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
