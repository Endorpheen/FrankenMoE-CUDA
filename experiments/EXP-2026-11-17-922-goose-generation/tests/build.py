#!/usr/bin/env python3
"""EXP922: one composed Goose unit drives a real generation loop. No GPU, no server."""
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
    'goose/cpu/goose-graph-builders.goose', 'goose/model/qgate-split.goose',
    'goose/model/qwen35-attention.goose', 'goose/model/gdn-ops.goose', 'goose/model/qwen35-gdn.goose',
    'goose/model/qwen35-kv.goose', 'goose/model/qwen35-trunk.goose',
    'goose/model/gguf-tensors.goose', 'goose/misc/gguf-headers.goose',
    'goose/model/gguf-reader.goose', 'goose/model/special-partition.goose',
    'goose/misc/unicode-tables.goose', 'goose/misc/unicode-text.goose',
    'goose/misc/pretokenizer-qwen35.goose', 'goose/model/bpe-tokenize.goose',
    'goose/model/bpe-detokenize.goose', 'goose/sampling/sampler-chain-full.goose',
]]
SUPPORT = Path('/home/end0/AI/LlamaServer/apex-independent-fast-20261116/run')


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--step', choices=['check', 'build', 'gate'], required=True)
    ap.add_argument('--support', type=Path, default=SUPPORT)
    ap.add_argument('--model', default='/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
    ap.add_argument('--prompt', default='The capital of France is')
    ap.add_argument('--max-tokens', default='8')
    ap.add_argument('--seed', default='42')
    ap.add_argument('--mode', default='greedy')
    ap.add_argument('--timeout', default='7200')
    ap.add_argument('--lib', type=Path, help='build directory holding unit.so')
    a = ap.parse_args()
    out = a.out.resolve()
    if not out.is_relative_to(Path('/home/end0/AI/LlamaServer')):
        raise SystemExit('outputs only in LlamaServer')
    out.mkdir(parents=True, exist_ok=True)
    pin = json.loads((ROOT / 'runtime/compiler/current.json').read_text())
    goose = Path(pin['compiler'])
    assert sha(goose) == pin['sha256']
    key = {str(p): sha(p) for p in MODULES}
    key['compiler'] = pin['sha256']
    if a.step in ('build', 'gate'):
        for p in [ROOT / 'integration/goose-standalone/graph-bindings.c',
                  ROOT / 'integration/m1/host/goose_host.c',
                  a.support / 'libggml-base.so.0', a.support / 'libggml-cpu.so.0']:
            key[str(p)] = sha(p)
    if a.step == 'gate':
        key['lib'] = str(a.lib or out)
        key[str(E / 'tests/gate.cpp')] = sha(E / 'tests/gate.cpp')
    kp = out / 'inputs.json'
    if kp.exists() and json.loads(kp.read_text()) != key:
        raise SystemExit('changed source inputs: preserve old artifacts and use a new output')
    kp.write_text(json.dumps(key, indent=2) + '\n')
    with (out / (a.step + '.log')).open('a') as log:
        def run(cmd, timeout=600):
            cmd = [str(c) for c in cmd]
            print('$ ' + shlex.join(cmd), flush=True)
            log.write('$ ' + shlex.join(cmd) + '\n')
            log.flush()
            r = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=timeout)
            log.write(r.stdout + '\nexit_code=' + str(r.returncode) + '\n')
            log.flush()
            print(r.stdout[-12000:], end='', flush=True)
            if r.returncode:
                raise RuntimeError('command failed: ' + str(r.returncode))
        if a.step == 'check':
            run([sys.executable, ROOT / 'tools/compose-goose.py', out / 'unit.goose', *MODULES])
            run([goose, 'check', out / 'unit.goose'])
        elif a.step == 'build':
            if (out / 'unit.so').exists():
                raise RuntimeError('refusing to overwrite library')
            run([sys.executable, ROOT / 'tools/compose-goose.py', out / 'unit.goose', *MODULES])
            run([goose, '--standalone', '-o', out / 'runtime.c', '--header', out / 'runtime.h', out / 'unit.goose'])
            include = ROOT / 'vendor/flint/ggml/include'
            run(['gcc-13', '-O1', '-g', '-fPIC', '-std=c11', '-I', include, '-c',
                 ROOT / 'integration/goose-standalone/graph-bindings.c', '-o', out / 'bindings.o'])
            import re
            host = (ROOT / 'integration/m1/host/goose_host.c').read_text()
            parts = [re.search(p, host, re.S).group(0) for p in [r'static void host_st_u8\([^}]+\}', r'void qz_st_u8\([^}]+\}']]
            (out / 'existing-byte-store.c').write_text('#include <stdint.h>\n#include <string.h>\n' + '\n'.join(parts) + '\n')
            run(['gcc-13', '-O1', '-g', '-fPIC', '-std=c11', '-c', out / 'existing-byte-store.c',
                 '-o', out / 'existing-byte-store.o'])
            run(['gcc-13', '-O1', '-g', '-fPIC', '-shared', '-Wl,-z,defs', out / 'runtime.c', out / 'bindings.o',
                 out / 'existing-byte-store.o', a.support / 'libggml-base.so.0',
                 '-Wl,-rpath,' + str(a.support), '-lm', '-o', out / 'unit.so'])
            run(['ldd', out / 'unit.so'])
        else:
            if (out / 'gate').exists():
                raise RuntimeError('refusing to overwrite fixture')
            include = ROOT / 'vendor/flint/ggml/include'
            lib = (a.lib or out).resolve()
            run(['g++-13', '-std=c++17', '-O1', '-g', '-I', include, '-I', lib, E / 'tests/gate.cpp',
                 lib / 'unit.so', a.support / 'libggml-cpu.so.0', a.support / 'libggml-base.so.0',
                 '-Wl,-rpath,' + str(a.support), '-lm', '-o', out / 'gate'])
            run([out / 'gate', a.model, a.prompt, a.max_tokens, a.seed, a.mode], timeout=int(a.timeout))
        log.write('RESULT PASS\n')


if __name__ == '__main__':
    main()
