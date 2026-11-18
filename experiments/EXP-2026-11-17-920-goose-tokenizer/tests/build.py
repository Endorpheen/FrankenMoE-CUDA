#!/usr/bin/env python3
"""Selective builds/gates for EXP920 (Goose tokenizer). No server, no GPU."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

E = Path(__file__).resolve().parents[1]
ROOT = E.parents[1]
MODULES = [ROOT / n for n in [
    'goose/misc/gguf-headers.goose',
    'goose/model/gguf-tensors.goose',
    'goose/model/gguf-reader.goose',
    'goose/model/special-partition.goose',
    'goose/misc/unicode-tables.goose',
    'goose/misc/unicode-text.goose',
    'goose/misc/pretokenizer-qwen35.goose',
    'goose/model/bpe-tokenize.goose',
    'goose/model/bpe-detokenize.goose',
]]
SUPPORT = Path('/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run')


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--step', choices=['check', 'build', 'gate'], required=True)
    ap.add_argument('--support', type=Path, default=SUPPORT)
    ap.add_argument('--lib', type=Path, help='build directory holding tokenizer.so (gate step)')
    a = ap.parse_args()
    out = a.out.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):
        raise SystemExit('outputs only in LlamaServer')
    out.mkdir(parents=True, exist_ok=True)
    (out / (a.step + '.pid')).write_text(str(os.getpid()) + '\n')
    pin = json.loads((ROOT / 'runtime/compiler/current.json').read_text())
    goose = Path(pin['compiler'])
    assert sha(goose) == pin['sha256'], 'pinned compiler changed'
    key = {str(p): sha(p) for p in MODULES}
    key['compiler'] = pin['sha256']
    if a.step == 'gate':
        key['lib'] = str(a.lib or out)
        for p in [a.support / 'libllama.so.0', a.support / 'libggml.so.0',
                  a.support / 'libggml-base.so.0', a.support / 'libggml-cpu.so.0',
                  E / 'tests/gate.cpp']:
            key[str(p)] = sha(p)
    kp = out / 'inputs.json'
    if kp.exists() and json.loads(kp.read_text()) != key:
        raise SystemExit('changed source inputs: preserve old artifacts and use a new output')
    kp.write_text(json.dumps(key, indent=2) + '\n')
    with (out / (a.step + '.log')).open('a') as log:
        def run(cmd):
            cmd = list(map(str, cmd))
            line = '$ ' + shlex.join(cmd)
            print(line, flush=True)
            log.write(line + '\n')
            log.flush()
            r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, timeout=600)
            log.write(r.stdout + '\nexit_code=' + str(r.returncode) + '\n')
            log.flush()
            print(r.stdout[-12000:], end='', flush=True)
            if r.returncode:
                raise RuntimeError('command failed: ' + str(r.returncode))
        if a.step == 'check':
            run([sys.executable, ROOT / 'tools/compose-goose.py', out / 'unit.goose', *MODULES])
            run([goose, 'check', out / 'unit.goose'])
        elif a.step == 'build':
            if (out / 'tokenizer.so').exists():
                raise RuntimeError('refusing to overwrite library')
            run([sys.executable, ROOT / 'tools/compose-goose.py', out / 'unit.goose', *MODULES])
            run([goose, '--standalone', '-o', out / 'runtime.c', '--header', out / 'runtime.h', out / 'unit.goose'])
            run(['gcc-13', '-O2', '-g', '-fPIC', '-shared', '-Wl,-z,defs', out / 'runtime.c', '-lm', '-o', out / 'tokenizer.so'])
            run(['ldd', out / 'tokenizer.so'])
            run(['nm', '-D', '--undefined-only', out / 'tokenizer.so'])
        else:
            if (out / 'gate').exists():
                raise RuntimeError('refusing to overwrite fixture')
            lib = (a.lib or out).resolve()
            run(['g++-13', '-std=c++17', '-O2', '-g', '-I', a.support, '-I', ROOT/'vendor/flint/include', '-I', ROOT/'vendor/flint/ggml/include', '-I', lib,
                 E / 'tests/gate.cpp', lib / 'tokenizer.so',
                 a.support / 'libllama.so.0', a.support / 'libggml.so.0',
                 a.support / 'libggml-base.so.0', a.support / 'libggml-cpu.so.0',
                 '-Wl,-rpath,' + str(a.support), '-lm', '-o', out / 'gate'])
            run([out / 'gate', '/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf'])
        log.write('RESULT PASS\n')


if __name__ == '__main__':
    main()
