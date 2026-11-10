#!/usr/bin/env python3
"""Rebuild and run every module fixture against the committed generated sources.

Rebuilds from what is committed rather than regenerating from .goose, because the committed
artifacts are what the chain ships. Prints one line per module and a total.

This proves the modules are individually correct. It does NOT prove the chain composition --
see check_witnesses.py (with --baseline) and the route audit for that.
"""
import subprocess, glob
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LLAMA_SRC = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/src')
STD = ['-include', 'stdint.h', '-include', 'stdio.h', '-include', 'stdlib.h', '-include', 'string.h']


def build_and_run(E: Path):
    S, tag = E / 'source', E.name[-3:]
    bridge, fixture = S / 'bridge.c', E / 'tests' / 'fixture.cpp'
    if not bridge.exists() or not fixture.exists():
        return None
    gb, gf, ex = f'/tmp/ag_b{tag}.o', f'/tmp/ag_f{tag}.o', f'/tmp/ag_x{tag}'
    r = subprocess.run(['gcc-13', '-O2', '-fPIC', f'-I{S}', *STD, '-c', str(bridge), '-o', gb],
                       capture_output=True, text=True)
    if r.returncode:
        return (E.name, 'bridge-fail', r.stderr.splitlines()[:1])
    compiled = False
    for extra in ([], [f'-I{LLAMA_SRC}'], [f'-I{LLAMA_SRC}', f'-I{E / "integration"}']):
        r = subprocess.run(['g++-13', '-O2', '-std=c++17', *extra, '-c', str(fixture), '-o', gf],
                           capture_output=True, text=True)
        if r.returncode == 0:
            compiled = True
            break
    if not compiled:
        return (E.name, 'fixture-fail', r.stderr.splitlines()[:1])
    libs = []
    if 'unicode-data' in fixture.read_text(errors='replace'):
        ud = '/tmp/ag_unicode_data.o'
        subprocess.run(['g++-13', '-O2', '-std=c++17', f'-I{LLAMA_SRC}',
                        '-c', str(LLAMA_SRC / 'unicode-data.cpp'), '-o', ud], check=True)
        libs = [ud]
    r = subprocess.run(['g++-13', gf, gb, *libs, '-o', ex], capture_output=True, text=True)
    if r.returncode:
        return (E.name, 'link-fail', r.stderr.splitlines()[:1])
    q = subprocess.run([ex], capture_output=True, text=True, timeout=900)
    line = [l for l in (q.stdout + q.stderr).splitlines() if 'FIXTURE' in l]
    return (E.name, f'rc={q.returncode}', line[-1] if line else '(no FIXTURE line)')


def main():
    rows = [r for f in sorted(glob.glob(str(ROOT / 'experiments' / 'EXP-*' / 'tests' / 'fixture.cpp')))
            if (r := build_and_run(Path(f).parent.parent))]
    total = fails = 0
    for name, status, msg in rows:
        print(f'{name[-30:]:<32} {status:<12} {msg}')
        if isinstance(msg, str) and 'checks=' in msg:
            import re
            m = re.search(r'checks=(\d+) failures=(\d+)', msg)
            if m:
                total += int(m.group(1))
                fails += int(m.group(2))
    print(f'\n{len(rows)} fixtures, {total} checks, {fails} failures')
    return 0 if fails == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
