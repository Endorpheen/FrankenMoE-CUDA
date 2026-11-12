#!/usr/bin/env python3
"""Whole-backend functional and sanitizer milestone (P3-19).

Runs every qualified module's differential fixture under three compute-sanitizer tools
and records the results. The fixtures already prove byte-level equality against native in
both arms; this adds the memory and concurrency checks that a passing byte comparison
does not cover.

The three tools are chosen because each catches something the others cannot:
  memcheck   -- out-of-bounds and misaligned access (this caught the misaligned 4-byte
                read in EXP186 and the stale-ybase out-of-bounds in EXP185)
  racecheck  -- shared-memory hazards between the cross-warp staging writes and reads
  initcheck  -- reads of uninitialised shared staging, which is exactly the failure mode
                of a reduction that forgets to zero a slot above the warp count

Any module whose fixture is missing or whose sanitizer summary is not recorded as clean
fails the milestone; nothing is skipped silently.
"""
from pathlib import Path
import json, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
SAN = '/usr/local/cuda-12.6/bin/compute-sanitizer'
OUT = F / 'runtime/sanitizer-milestone'
OUT.mkdir(parents=True, exist_ok=True)

MODULES = [174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 188]

# No module is exempt. An earlier version declared EXP181 an "inherent" hazard because the
# native arm showed more of them; that was wrong. The hazards came from the fixture feeding
# duplicate experts for one token, which a router never emits, and once the fixture
# generated distinct experts per token both arms reported zero. Any non-zero result now
# fails the milestone, as it should.
NATIVE_PARITY = {}

PATS = {
    'memcheck':  r'ERROR SUMMARY: (\d+) errors',
    'racecheck': r'RACECHECK SUMMARY: \d+ hazards displayed \((\d+) errors',
    'initcheck': r'ERROR SUMMARY: (\d+) errors',
}

rows = []
failed = []
for n in MODULES:
    b = F / f'runtime/exp{n}/build'
    run = F / f'runtime/exp{n}/run'
    exe = b / 'gpu-fixture'
    if not exe.exists():
        failed.append((n, 'missing fixture binary'))
        rows.append(dict(module=n, status='MISSING'))
        continue
    entry = dict(module=n, status='PASS')
    for tool, pat in PATS.items():
        q = subprocess.run([SAN, '--tool', tool, './gpu-fixture'],
                           cwd=b, text=True, capture_output=True,
                           env={**__import__('os').environ, 'LD_LIBRARY_PATH': str(run)},
                           timeout=900)
        (OUT / f'exp{n}-{tool}.log').write_text(q.stdout + q.stderr)
        m = re.search(pat, q.stdout + q.stderr)
        errs = int(m.group(1)) if m else None
        entry[tool] = errs
        if errs is None:
            entry['status'] = 'NO-SUMMARY'
            failed.append((n, f'{tool}: no summary line'))
        elif errs != 0:
            if n in NATIVE_PARITY:
                var, val = NATIVE_PARITY[n]
                import os as _os
                qn = subprocess.run([SAN, '--tool', tool, './gpu-fixture'],
                                    cwd=b, text=True, capture_output=True,
                                    env={**_os.environ, 'LD_LIBRARY_PATH': str(run), var: val},
                                    timeout=900)
                (OUT / f'exp{n}-{tool}-native.log').write_text(qn.stdout + qn.stderr)
                mn = re.search(pat, qn.stdout + qn.stderr)
                native_errs = int(mn.group(1)) if mn else None
                entry[tool + '_native'] = native_errs
                if native_errs is not None and native_errs >= errs:
                    entry['status'] = 'PASS_WITH_NATIVE_PARITY'
                    entry.setdefault('notes', []).append(
                        f'{tool}: {errs} vs native {native_errs} -- hazard is inherent to the algorithm')
                else:
                    entry['status'] = 'FAIL'
                    failed.append((n, f'{tool}: {errs} errors, native only {native_errs}'))
            else:
                entry['status'] = 'FAIL'
                failed.append((n, f'{tool}: {errs} errors'))
    rows.append(entry)
    print(f"EXP{n}: {entry['status']}  "
          + ' '.join(f'{t}={entry[t]}' for t in PATS), flush=True)

(OUT / 'milestone.json').write_text(json.dumps(
    dict(modules=rows, failed=[list(f) for f in failed],
         tools=list(PATS), status='PASS' if not failed else 'FAIL'), indent=2) + '\n')

if failed:
    print('FAILED:', failed)
    raise SystemExit(1)
print('P3-19_SANITIZER_MILESTONE_PASS')
