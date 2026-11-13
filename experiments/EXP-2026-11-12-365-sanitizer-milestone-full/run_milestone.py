#!/usr/bin/env python3
"""Whole-matrix compute-sanitizer milestone for P3-19.

EXP191 ran thirteen modules under three tools. Three things were left outside its
criterion:

  * synccheck, which the requirement names explicitly and EXP191 never ran;
  * the 28 modules added after EXP191 (EXP248-288), which have self-contained GPU
    fixtures on disk but were never sanitized;
  * a module list derived from the artifact tree rather than hand-written, so a
    new fixture cannot be silently skipped.

This runner discovers every runtime/exp*/build/gpu-fixture, requires the fixture
to pass on its own first (a sanitizer finding on a fixture that already reports
failures would blame the tool for the fixture), then runs the four tools.

Any missing summary line, any non-zero error count, and any failing plain run
fails the milestone. Nothing is exempt and nothing is skipped silently.
"""
from pathlib import Path
import json, os, re, subprocess

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
SAN = '/usr/local/cuda-12.6/bin/compute-sanitizer'
OUT = F / 'runtime/sanitizer-milestone-full'
OUT.mkdir(parents=True, exist_ok=True)

PATS = {
    'memcheck':  r'ERROR SUMMARY: (\d+) errors',
    'racecheck': r'RACECHECK SUMMARY: \d+ hazards displayed \((\d+) errors',
    'initcheck': r'ERROR SUMMARY: (\d+) errors',
    'synccheck': r'ERROR SUMMARY: (\d+) errors',
}
TOOLS = list(PATS)
PLAIN = r'failures=(\d+)'


def discover():
    """Module -> fixture binary. A '*gpu-fixture-fixed' supersedes the shipped fixture
    for the two modules whose fixture fed F32 mask strides into the F16 arm; taking it
    automatically keeps the corrected binary from being silently ignored."""
    mods = {}
    for exe in sorted(F.glob('runtime/exp*/build/gpu-fixture')):
        n = exe.parent.parent.name.replace('exp', '')
        if n.isdigit():
            mods[int(n)] = 'gpu-fixture'
    for exe in sorted(F.glob('runtime/exp*/build/gpu-fixture-fixed')):
        n = exe.parent.parent.name.replace('exp', '')
        if n.isdigit():
            mods[int(n)] = 'gpu-fixture-fixed'
    return sorted(mods.items())


rows, failed = [], []
for n, binary in discover():
    build = F / f'runtime/exp{n}/build'
    env = {**os.environ}
    entry = dict(module=n, fixture=binary, status='PASS')

    q = subprocess.run([f'./{binary}'], cwd=build, text=True,
                       capture_output=True, env=env, timeout=900)
    m = re.search(PLAIN, q.stdout + q.stderr)
    entry['plain_rc'] = q.returncode
    entry['plain_failures'] = int(m.group(1)) if m else None
    if q.returncode != 0 or entry['plain_failures'] != 0:
        entry['status'] = 'FAIL'
        failed.append((n, f'plain run rc={q.returncode} failures={entry["plain_failures"]}'))
        rows.append(entry)
        print(f'EXP{n}: FAIL plain rc={q.returncode}', flush=True)
        continue

    for tool, pat in PATS.items():
        r = subprocess.run([SAN, '--tool', tool, f'./{binary}'], cwd=build,
                           text=True, capture_output=True, env=env, timeout=1800)
        text = r.stdout + r.stderr
        (OUT / f'exp{n}-{tool}.log').write_text(text)
        mm = re.search(pat, text)
        errs = int(mm.group(1)) if mm else None
        entry[tool] = errs
        if errs is None:
            entry['status'] = 'NO-SUMMARY'
            failed.append((n, f'{tool}: no summary line'))
        elif errs != 0:
            entry['status'] = 'FAIL'
            failed.append((n, f'{tool}: {errs} errors'))

    rows.append(entry)
    print(f"EXP{n}: {entry['status']}  "
          + ' '.join(f'{t}={entry[t]}' for t in TOOLS), flush=True)

(OUT / 'milestone.json').write_text(json.dumps(
    dict(modules=rows, failed=[list(f) for f in failed], tools=TOOLS,
         module_count=len(rows),
         status='PASS' if not failed else 'FAIL'), indent=2) + '\n')

if failed:
    print('FAILED:', failed)
    raise SystemExit(1)
print(f'P3-19_SANITIZER_MILESTONE_PASS modules={len(rows)} tools={len(TOOLS)}')
