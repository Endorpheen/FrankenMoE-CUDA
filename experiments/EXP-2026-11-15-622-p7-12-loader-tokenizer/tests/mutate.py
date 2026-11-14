#!/usr/bin/env python3
"""Mutation run for the P7-12 slices.

Each mutation edits one port, rebuilds it with tests/build.sh at -O0, runs its
gate and compares with the native oracle. A gate failure, a build failure or a
missing output counts as caught.

Twelve mutations, run in two halves:
    mutate.py 0 6     S1 x4, S2 x2
    mutate.py 6 12    S2 x2, S3 x4
"""
import subprocess
import sys
from pathlib import Path

S = Path(__file__).resolve().parent.parent
OUT = S / 'build' / 'mut'
ENV = {
    'PATH': '/usr/local/bin:/usr/bin:/bin',
    'OPT': '-O0',
    'LLAMA_DIR': '/home/end0/AI/LlamaServer/llama.cpp-flint-cuda',
    'GOOSE_BIN': '/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose',
    'CC': 'gcc-13',
    'CXX': 'g++-13',
}

# slice, label, anchor, replacement
M = [
    # ---- half 1: S1 dispatch, then S2 -----------------------------------
    ('dispatch', 'no_vocab reports the SPM type name',
     '        tname = "NONE";', '        tname = "SPM";'),
    ('dispatch', 'bert mask id becomes sep',
     '        mask = 103;', '        mask = 102;'),
    ('dispatch', 'llama-bpe loses its llama3 rule',
     'vt_eq(pre, "llama-bpe") || ', ''),
    ('dispatch', 'BPE default keeps clean_spaces off',
     '''            escape_whitespaces = 0;
            clean_spaces = 1;''',
     '''            escape_whitespaces = 0;
            clean_spaces = 0;'''),
    ('partition', 'unknown tokens are split out too',
     'if parse_special == 0 && (attr & 9) != 0 {',
     'if parse_special == 0 && (attr & 8) != 0 {'),
    ('partition', 'LSTRIP reads the RSTRIP bit',
     'if (attr & 128) != 0 {', 'if (attr & 256) != 0 {'),
    # ---- half 2: S2, then S3 -------------------------------------------
    ('partition', 'the left remainder is kept even when empty',
     'if ll > 0 {', 'if ll >= 0 {'),
    ('partition', 'the partition whitespace set loses the carriage return',
     'return c == 32 || c == 9 || c == 10 || c == 11 || c == 12 || c == 13;',
     'return c == 32 || c == 9 || c == 10 || c == 11 || c == 12;'),
    ('strings', 'the strip set loses the vertical tab',
     'return c == 32 || c == 9 || c == 10 || c == 11 || c == 12 || c == 13;',
     'return c == 32 || c == 9 || c == 10 || c == 12 || c == 13;'),
    ('strings', 'split advances one byte past the delimiter',
     'start = m + delim.len;', 'start = m + 1;'),
    ('strings', 'from(vector<int>) opens without the blank',
     'var at: i64 = cs_copy("[ ", 0, 2, out, 0);',
     'var at: i64 = cs_copy("[", 0, 1, out, 0);'),
    ('strings', 'only uppercase X starts an escape',
     '} else if c == 120 && i + 2 < input.len',
     '} else if c == 88 && i + 2 < input.len'),
]

SRC = {
    'dispatch': S / 'source' / 'vocab-type.goose',
    'partition': S / 'source' / 'special-partition.goose',
    'strings': S / 'source' / 'common-strings.goose',
}


def run(slice_name):
    out = OUT / slice_name
    out.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(['bash', str(S / 'tests' / 'build.sh'), slice_name],
                       capture_output=True, text=True, timeout=600, errors='replace',
                       env=dict(ENV, OUT_DIR=str(out)))
    text = (r.stdout or '') + (r.stderr or '')
    if r.returncode != 0:
        return 1, text.strip().splitlines()[-1][:120]
    if 'failures 0' not in text:
        return 1, 'gate reported no clean line'
    return 0, 'identical'


def gate_failed(text):
    """A mutation counts only when the gate itself reported failures; a
    compile error or a missing corpus would hide a broken mutation."""
    for line in text.splitlines():
        if 'failures ' in line:
            try:
                return int(line.rsplit('failures ', 1)[1].split()[0]) > 0
            except ValueError:
                return False
    return False


lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
caught = 0
missed = []
for idx, (slice_name, label, old, new) in enumerate(M[lo:hi], start=lo):
    path = SRC[slice_name]
    orig = path.read_text()
    if orig.count(old) != 1:
        print(f'[skip] {idx} {label}: anchor count {orig.count(old)}')
        continue
    path.write_text(orig.replace(old, new, 1))
    try:
        rc, detail = run(slice_name)
    except subprocess.TimeoutExpired:
        rc, detail = 1, 'timeout'
    finally:
        path.write_text(orig)
    ok = rc != 0 and gate_failed(detail if 'failures ' in detail else '')
    if rc != 0 and not ok:
        detail = 'no gate verdict: ' + detail
    caught += ok
    if not ok:
        missed.append(idx)
    print(f'[{"caught" if ok else "MISSED"}] {idx} {slice_name}: {label}: {detail}')

print(f'mutations {caught}/{hi - lo} caught (range {lo}:{hi}) missed={missed}')
