#!/usr/bin/env python3
"""Cross-architecture compile coverage for P3-18.

This host has one GPU (sm_89), so no arm here can execute another configuration.
The requirement allows exactly that: "sm89 executed; alternate hardware
configurations have compile/emulation evidence and explicit hardware-unverified
ledger".

What this does: replays each experiment's recorded gpu-fixture-build command with
--generate-code pointed at every architecture nvcc supports, and records compile
success per module per architecture. A module that compiles for compute_75 but not
compute_90 is a real portability finding; a module that compiles everywhere but has
never run everywhere is recorded as hardware-unverified, not as covered.

Nothing is executed for the non-native architectures -- compiling is the claim, and
the ledger says so.
"""
from pathlib import Path
import json, re, shlex, subprocess, sys

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
OUT = F / 'runtime/arch-compile-coverage'
OUT.mkdir(parents=True, exist_ok=True)
SCRATCH = Path('/tmp/cc-arch-coverage')
SCRATCH.mkdir(parents=True, exist_ok=True)

NATIVE_ARCH = 'sm_89'
ARCHS = ['compute_50', 'compute_52', 'compute_53', 'compute_60', 'compute_61',
         'compute_62', 'compute_70', 'compute_72', 'compute_75', 'compute_80',
         'compute_86', 'compute_87', 'compute_89', 'compute_90']

GEN_RE = re.compile(r'^--generate-code=arch=compute_\d+,code=\[.*\]$')
SRC_RE = re.compile(r'.*/tests/gpu-fixture\.cu$')


def build_command(exp_dir):
    cmds = json.loads((exp_dir / 'local-test-commands.json').read_text())
    build = [c for c in cmds if c['label'] == 'gpu-fixture-build']
    if not build:
        return None
    return build[0]


rows, failed = [], []
experiments = sorted(p.parent.parent for p in F.glob('experiments/*/tests/gpu-fixture.cu'))

for exp_dir in experiments:
    cmd = build_command(exp_dir)
    if cmd is None:
        continue
    src_idx = next((i for i, a in enumerate(cmd['argv']) if SRC_RE.match(a)), None)
    gen_idx = next((i for i, a in enumerate(cmd['argv']) if GEN_RE.match(a)), None)
    if src_idx is None or gen_idx is None:
        failed.append((exp_dir.name, 'recorded command lacks a source or --generate-code argument'))
        continue

    entry = dict(experiment=exp_dir.name, source=cmd['argv'][src_idx], archs={})
    for arch in ARCHS:
        argv = list(cmd['argv'])
        argv[gen_idx] = f'--generate-code=arch={arch},code=[{arch}]'
        obj = SCRATCH / f'{exp_dir.name}-{arch}.o'
        argv[-1] = str(obj)
        r = subprocess.run(argv, cwd=cmd['cwd'], text=True, capture_output=True, timeout=1800)
        entry['archs'][arch] = 'ok' if r.returncode == 0 else 'fail'
        if r.returncode != 0:
            err = (r.stderr or '').strip().splitlines()
            entry.setdefault('errors', {})[arch] = err[-1][:300] if err else f'rc={r.returncode}'
        if obj.exists():
            obj.unlink()
    entry['native_arch'] = NATIVE_ARCH
    entry['native_executed'] = True
    entry['other_archs_executed'] = False
    rows.append(entry)
    ok = sorted(a for a, v in entry['archs'].items() if v == 'ok')
    bad = sorted(a for a, v in entry['archs'].items() if v != 'ok')
    print(f"{exp_dir.name}: ok={len(ok)}/{len(ARCHS)}"
          + (f" FAIL={bad}" if bad else ''), flush=True)

summary = dict(
    native_arch=NATIVE_ARCH,
    archs=ARCHS,
    modules=len(rows),
    all_archs_ok=[r['experiment'] for r in rows if all(v == 'ok' for v in r['archs'].values())],
    partial=sorted(r['experiment'] for r in rows if not all(v == 'ok' for v in r['archs'].values())),
    failed=failed,
    note=('Compilation is the only evidence for non-native architectures on this host; '
          'no kernel was executed on them. Recorded as hardware-unverified, not covered.'),
)
(OUT / 'compile-coverage.json').write_text(json.dumps(summary | {'detail': rows}, indent=2) + '\n')

print(f"modules={len(rows)} all_archs_ok={len(summary['all_archs_ok'])} "
      f"partial={len(summary['partial'])} malformed={len(failed)}")
