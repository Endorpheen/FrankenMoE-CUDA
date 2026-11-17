#!/usr/bin/env python3
"""Negative gates for EXP921: each mutant must make the differential gate fail.

    python3 mutations.py --out <new LlamaServer directory> [--name X ...]
"""
import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]
MODULE = 'goose/sampling/sampler-chain-full.goose'

MUTATIONS = {
    'heap-top-root': (
        '    push_heap(lv, id, first, hole, hole_in, v_l, v_id);',
        '    push_heap(lv, id, first, hole, 0, v_l, v_id);'),
    'canonical-single-draw': (
        '    return (r1 + r2 * 4294967296.0) / 18446744073709551616.0;',
        '    return r1 / 4294967296.0;'),
    'top-p-scale': (
        '            if cum >= top_p && i + 1 >= min_keep {',
        '            if cum >= top_p * 0.9 && i + 1 >= min_keep {'),
    'skip-temperature': (
        '        lv[i] = lv[i] / temp;',
        '        lv[i] = lv[i];'),
    'dist-first-candidate': (
        '            if run >= tgt {\n                sel = i;\n                found = true;\n            }',
        '            if run >= tgt {\n                sel = 0;\n                found = true;\n            }'),
}


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--support', type=Path,
                    default=Path('/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run'))
    ap.add_argument('--name', action='append')
    a = ap.parse_args()
    out = a.out.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):
        raise SystemExit('outputs only in LlamaServer')
    pin = json.loads((ROOT / 'runtime/compiler/current.json').read_text())
    goose = Path(pin['compiler'])
    assert sha(goose) == pin['sha256']
    results = {}
    for name in (a.name or list(MUTATIONS)):
        old, new = MUTATIONS[name]
        dst = out / name
        if dst.exists():
            raise SystemExit('refusing to overwrite ' + str(dst))
        (dst / 'src').mkdir(parents=True)
        text = (ROOT / MODULE).read_text()
        if old not in text:
            raise SystemExit(f'{name}: mutation text not found')
        (dst / 'src' / 'sampler.goose').write_text(text.replace(old, new, 1))
        log = (dst / 'mutation.log').open('w')

        def run(cmd):
            cmd = [str(c) for c in cmd]
            log.write('$ ' + ' '.join(cmd) + '\n')
            log.flush()
            r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=300)
            log.write(r.stdout + f'\nexit_code={r.returncode}\n')
            log.flush()
            return r

        assert run([goose, '--standalone', '-o', dst / 'runtime.c', '--header', dst / 'runtime.h',
                    dst / 'src' / 'sampler.goose']).returncode == 0, name + ': mutant must build'
        assert run(['gcc-13', '-O2', '-fPIC', '-shared', '-Wl,-z,defs', dst / 'runtime.c', '-lm',
                    '-o', dst / 'sampler.so']).returncode == 0, name + ': mutant must link'
        assert run(['g++-13', '-std=c++17', '-O2', '-I', a.support, '-I', ROOT / 'vendor/flint/include',
                    '-I', ROOT / 'vendor/flint/ggml/include', '-I', dst, E / 'tests/gate.cpp',
                    dst / 'sampler.so', a.support / 'libllama.so.0', a.support / 'libggml.so.0',
                    a.support / 'libggml-base.so.0', a.support / 'libggml-cpu.so.0',
                    '-Wl,-rpath,' + str(a.support), '-lm', '-o', dst / 'gate']).returncode == 0, name + ': fixture must build'
        r = run([dst / 'gate'])
        results[name] = 'REJECTED' if r.returncode != 0 else 'NOT DETECTED'
        print(f'{name}: {results[name]}')
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    bad = [k for k, v in results.items() if v != 'REJECTED']
    if bad:
        raise SystemExit('mutations not detected: ' + ', '.join(bad))
    print('all mutants rejected')


if __name__ == '__main__':
    main()
