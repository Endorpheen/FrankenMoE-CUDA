#!/usr/bin/env python3
"""Build the differential and liveness coverage registry (P0-05).

For every module in the migration this collects, from artifacts that already exist rather
than from prose:

  tier        accepted / rejected / off-path, as recorded in tasks.json
  in_chain    whether the module's object is in the active link line
  witness     the line the module must emit, and whether a live Apex log carries it
  byte        whether the module's differential fixture proved byte equality, and over how
              many cases
  sanitizer   memcheck / racecheck / initcheck result from the milestone artifact

The registry is meant to be regenerated and diffed, not read once. Any module whose
recorded tier disagrees with what the runtime does is reported as an inconsistency at the
end, which is the failure mode that let a rejected candidate run by default for several
iterations.

Usage: python3 build_registry.py [--check]
       --check  exit non-zero if any module's recorded tier conflicts with observation
"""
from pathlib import Path
import glob, json, re, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
OUT = F / 'docs/goose-migration/coverage-registry.json'

# module -> (witness, description)
EXPECTED = {
    174: ('FG176R_CUDA_SET_ROWS', 'set_rows'),
    175: ('FG175C_CUDA_CONCAT', 'concat'),
    176: ('FG176G_CUDA_GET_ROWS', 'get_rows'),
    177: ('FG177C_CUDA_CPY', 'cpy/dup/cont'),
    178: ('FG178Q_CUDA_Q8_1', 'q8_1 quantize'),
    179: ('FG179W_CUDA_MOE_WEIGHTED', 'MoE weighted reduction'),
    180: ('FG180Q_CUDA_MMQ_Q8_1', 'MMQ q8_1 quantize'),
    181: ('FG181M_CUDA_MM_IDS', 'mm_ids helper'),
    182: ('FG182D_CUDA_DEQUANT_Q4_K', 'Q4_K dequantization'),
    183: ('FG183F_CUDA_FWHT', 'FWHT warp path'),
    184: ('FG184M_CUDA_MMVF', 'mul_mat_vec_f'),
    185: ('FG185Q_CUDA_MMVQ_Q4K', 'mul_mat_vec_q Q4_K'),
    186: ('FG186Q_CUDA_MMVQ_Q6K', 'mul_mat_vec_q Q6_K'),
    188: ('FG188D_CUDA_DEQUANT_Q4_0', 'Q4_0 dequantization'),
}

def newest_log():
    cands = sorted(F.glob('runtime/*/results/*-goose/server.log'),
                   key=lambda p: p.stat().st_mtime)
    return cands[-1] if cands else None

def chain_objects():
    """Object files in the active link line."""
    q = F / 'runtime/exp185/build/commands.json'
    if not q.exists():
        return set()
    for c in json.loads(q.read_text()):
        if c['label'] == 'cuda-mmvq-q4k-link':
            # keep the full path so a stage can be matched; basenames collide across stages
            return {x for x in c['argv'] if x.endswith('.o')}
    return set()

def tier_map():
    t = {}
    for h in json.loads((F / 'docs/goose-migration/tasks.json').read_text()).get('history', []):
        e = h.get('experiment', '')
        m = re.fullmatch(r'EXP1(\d\d)', e)
        if m and 74 <= int('1' + m.group(1)) <= 200:
            t[int('1' + m.group(1))] = h.get('status', '?')
    return t

# Two shapes exist in the tree: a '## Verdict' section (added for stages that had none) and
# an inline 'Verdict: **X**' sentence. Both are accepted so the registry reads every stage
# rather than skipping the ones written in the other style.
VERDICT_RE = re.compile(
    r'(?:^## Verdict\s*\n\s*\n\*\*|\bVerdict: \*\*)(ACCEPTED|REJECTED|OFF_PATH)\*\*',
    re.M)

def readme_verdict(n):
    """The README is the primary record of a stage's verdict; the ledger is a summary of it."""
    d = glob.glob(str(F / f'experiments/EXP-2026-11-08-{n}-*'))
    if not d:
        return None
    p = Path(d[0]) / 'README.md'
    if not p.exists():
        return None
    m = VERDICT_RE.search(p.read_text())
    return m.group(1) if m else None

def byte_facts(n):
    d = glob.glob(str(F / f'experiments/EXP-2026-11-08-{n}-*'))
    if not d:
        return None
    f = Path(d[0]) / 'byte-compare.json'
    if not f.exists():
        return None
    try:
        v = json.loads(f.read_text())
    except Exception:
        return None
    ident = v.get('identical')
    if isinstance(ident, list):
        return dict(status=v.get('status'), cases=len(ident))
    return dict(status=v.get('status'), cases=ident)

def sanitizer(n):
    p = F / 'runtime/sanitizer-milestone/milestone.json'
    if not p.exists():
        return None
    for m in json.loads(p.read_text()).get('modules', []):
        if m.get('module') == n:
            return {k: m.get(k) for k in ('memcheck', 'racecheck', 'initcheck')}
    return None

log = newest_log()
log_text = log.read_text(errors='ignore') if log else ''
chain = chain_objects()
tiers = tier_map()

rows = []
conflicts = []
for n, (wit, desc) in sorted(EXPECTED.items()):
    tier = tiers.get(n, 'UNKNOWN')
    in_chain = any(f'/exp{n}/' in o for o in chain)
    live = bool(wit and wit in log_text)
    bf = byte_facts(n)
    san = sanitizer(n)
    rv = readme_verdict(n)
    row = dict(module=n, experiment=f'EXP{n}', component=desc, tier=tier,
               readme_verdict=rv,
               in_active_chain=in_chain, witness=wit, witness_seen_in_live_log=live,
               byte_compare=bf, sanitizer=san)
    rows.append(row)

    # A rejected or off-path module must not be executing; an accepted one must be.
    if rv and rv != tier and not (rv == 'OFF_PATH' and tier in ('IN_PROGRESS', 'UNKNOWN')):
        conflicts.append(f'EXP{n}: ledger says {tier}, README says {rv}')

    if tier in ('REJECTED', 'OFF_PATH') and live:
        conflicts.append(f'EXP{n} recorded {tier} but its witness is in the live log')
    if tier == 'ACCEPTED' and not live:
        # An accepted module must execute. EXP182 and EXP188 are recorded OFF_PATH for
        # exactly this reason: they are byte-exact but the profile never reaches them, so
        # calling them ACCEPTED would be a claim the runtime does not support.
        conflicts.append(f'EXP{n} recorded ACCEPTED but its witness is absent from the live log')

reg = dict(generated_from=dict(log=str(log) if log else None,
                               link_command='runtime/exp185/build/commands.json'),
           modules=rows, conflicts=conflicts,
           status='CONSISTENT' if not conflicts else 'INCONSISTENT')
OUT.write_text(json.dumps(reg, indent=2) + '\n')

print(f"{'module':7s} {'component':26s} {'tier':12s} {'chain':5s} {'witness':7s} {'bytes':7s} {'san'}")
for r in rows:
    bc = r['byte_compare']
    b = 'n/a' if not bc else f"{bc['cases']}"
    s = 'n/a' if not r['sanitizer'] else ('ok' if all(v == 0 for v in r['sanitizer'].values() if isinstance(v, int)) else 'FAIL')
    print(f"EXP{r['module']:<4d} {r['component'][:26]:26s} {r['tier']:12s} "
          f"{str(r['in_active_chain']):5s} {str(r['witness_seen_in_live_log']):7s} {b:7s} {s}")

print(f"\nwrote {OUT.relative_to(F)}")
if conflicts:
    print('CONFLICTS:')
    for c in conflicts:
        print(' ', c)
    if '--check' in sys.argv:
        raise SystemExit(1)
else:
    print('COVERAGE_REGISTRY_CONSISTENT')
