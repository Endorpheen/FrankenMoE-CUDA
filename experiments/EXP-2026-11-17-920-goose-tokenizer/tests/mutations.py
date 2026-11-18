#!/usr/bin/env python3
"""Negative gates for EXP920: each mutation must make the differential gate fail.

A compile error, a crash or a resource stop is not a semantic rejection, so the
mutant must build successfully and then disagree with the native reference.

    python3 mutations.py --out <new LlamaServer directory> --support <apex run dir>
"""
import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]

MODULES = ['goose/misc/gguf-headers.goose', 'goose/model/gguf-tensors.goose',
           'goose/model/gguf-reader.goose', 'goose/model/special-partition.goose',
           'goose/misc/unicode-tables.goose', 'goose/misc/unicode-text.goose',
           'goose/misc/pretokenizer-qwen35.goose', 'goose/model/bpe-tokenize.goose',
           'goose/model/bpe-detokenize.goose']

# name -> (module, exact text to replace, replacement)
MUTATIONS = {
    'split-punct-newline': (
        'goose/misc/pretokenizer-qwen35.goose',
        '                while c2 == 13 || c2 == 10 {',
        '                while c2 == 13 {'),
    'split-last-rn': (
        'goose/misc/pretokenizer-qwen35.goose',
        '            if last_rn > 0 {',
        '            if last_rn < 0 {'),
    'merge-stale': (
        'goose/model/bpe-tokenize.goose',
        '''        if sym[l * 4 + 1] != ln || sym[r * 4 + 1] != rn {
            continue;
        }''',
        '''        if sym[l * 4 + 1] != ln {
            continue;
        }'''),
    'detok-no-byte-map': (
        'goose/model/bpe-detokenize.goose',
        '            let byte: i64 = uc_cpt_to_byte(cpt);\n            if byte >= 0 {\n                out[at] = byte as u8;',
        '            let byte: i64 = cpt;\n            if byte >= 0 {\n                out[at] = byte as u8;'),
    'eog-drop-endoftext': (
        'goose/model/bpe-tokenize.goose',
        '`        || t == "<end_of_turn>" || t == "<|endoftext|>" || t == "</s>"`'.strip('`'),
        '        || t == "<end_of_turn>" || t == "</s>"'),
    'token-id-offset': (
        'goose/model/bpe-tokenize.goose',
        '        let id: i64 = entry - 1;',
        '        let id: i64 = entry;'),
}


def sha(p):
    import hashlib
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--support', type=Path,
                    default=Path('/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run'))
    ap.add_argument('--name', action='append')
    ap.add_argument('--model', default='/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
    a = ap.parse_args()
    out = a.out.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):
        raise SystemExit('outputs only in LlamaServer')
    pin = json.loads((ROOT / 'runtime/compiler/current.json').read_text())
    goose = Path(pin['compiler'])
    assert sha(goose) == pin['sha256']

    names = a.name or list(MUTATIONS)
    results = {}
    for name in names:
        module, old, new = MUTATIONS[name]
        dst = out / name
        if dst.exists():
            raise SystemExit('refusing to overwrite ' + str(dst))
        (dst / 'src').mkdir(parents=True)
        paths = []
        for rel in MODULES:
            target = dst / 'src' / Path(rel).name
            text = (ROOT / rel).read_text()
            if rel == module:
                if old not in text:
                    raise SystemExit(f'{name}: mutation text not found in {rel}')
                text = text.replace(old, new, 1)
            target.write_text(text)
            paths.append(target)
        log = (dst / 'mutation.log').open('w')

        def run(cmd):
            cmd = [str(c) for c in cmd]
            log.write('$ ' + ' '.join(cmd) + '\n')
            log.flush()
            r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=300)
            log.write(r.stdout + f'\nexit_code={r.returncode}\n')
            log.flush()
            return r

        r = run([sys.executable, ROOT / 'tools/compose-goose.py', dst / 'unit.goose', *paths])
        assert r.returncode == 0, name
        r = run([goose, '--standalone', '-o', dst / 'runtime.c', '--header', dst / 'runtime.h', dst / 'unit.goose'])
        assert r.returncode == 0, name + ': mutant must build'
        r = run(['gcc-13', '-O2', '-fPIC', '-shared', '-Wl,-z,defs', dst / 'runtime.c', '-lm', '-o', dst / 'tokenizer.so'])
        assert r.returncode == 0, name + ': mutant must link'
        r = run(['g++-13', '-std=c++17', '-O2', '-I', a.support, '-I', ROOT / 'vendor/flint/include',
                 '-I', ROOT / 'vendor/flint/ggml/include', '-I', dst, E / 'tests/gate.cpp', dst / 'tokenizer.so',
                 a.support / 'libllama.so.0', a.support / 'libggml.so.0', a.support / 'libggml-base.so.0',
                 a.support / 'libggml-cpu.so.0', '-Wl,-rpath,' + str(a.support), '-lm', '-o', dst / 'gate'])
        assert r.returncode == 0, name + ': fixture must build'
        try:
            r = run([dst / 'gate', a.model])
            results[name] = 'REJECTED' if r.returncode != 0 else 'NOT DETECTED'
        except subprocess.TimeoutExpired:
            results[name] = 'HANG'
        print(f'{name}: {results[name]}')
    (out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
    bad = [k for k, v in results.items() if v != 'REJECTED']
    if bad:
        raise SystemExit('mutations not detected: ' + ', '.join(bad))
    print('all mutants rejected')


if __name__ == '__main__':
    main()
