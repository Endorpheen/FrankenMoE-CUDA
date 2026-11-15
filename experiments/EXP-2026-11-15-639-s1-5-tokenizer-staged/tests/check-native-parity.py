#!/usr/bin/env python3
"""Prove the staged native bodies are the pristine ones, moved not rewritten.

The stage moves three native bodies out of their functions:

  src/llama-vocab.cpp   the "determine vocab type" block -> determine_vocab_type_native()
  src/llama-vocab.cpp   tokenizer_st_partition()          -> tokenizer_st_partition_native()
  common/common.cpp     string_strip()                    -> string_strip_native()

The move is allowed to change two things inside the first body and nothing else:
the block is dedented by one level, `return;` becomes `return true;` (the early
exit is now reported to the caller) and `__func__` becomes "load" (the log
prefix stays the one users saw). This script checks exactly that, byte for byte.

Usage: check-native-parity.py <reference-dir> <staged-root>
"""
import re
import sys
from pathlib import Path

DISPATCH_START = 'if (tokenizer_model == "no_vocab" || tokenizer_model == "none") {'
DISPATCH_END = 'ml.get_key(LLM_KV_TOKENIZER_ADD_PREFIX,'
PARTITION_SIGS = ('void llama_vocab::impl::tokenizer_st_partition_native(',
                  'void llama_vocab::impl::tokenizer_st_partition(')
STRIP_SIGS = ('static std::string string_strip_native(const std::string & str) {',
              'std::string string_strip(const std::string & str) {')


def cut_dispatch(text):
    # Start at the beginning of the line so the block keeps its own indentation.
    start = text.rindex('\n', 0, text.index(DISPATCH_START)) + 1
    end = text.index(DISPATCH_END, start)
    return text[start:end]


def cut_body(text, sigs):
    for sig in sigs:
        if sig in text:
            start = text.index(sig)
            break
    else:
        raise SystemExit(f'signature not found: {sigs}')
    brace = text.index('{', start)
    depth = 0
    for i in range(brace, len(text)):
        c = text[i]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return text[brace + 1:i]
    raise SystemExit('unbalanced braces')


def dedent(block):
    # Preprocessor lines sit at column 0 in both copies (native style) and would
    # pin the common prefix to zero, so they are left alone. The prefix is taken
    # from the remaining lines: the pristine block is nested one level deeper
    # than the extracted body, and this makes both compare at the same base.
    lines = block.split('\n')
    widths = [len(l) - len(l.lstrip()) for l in lines
              if l.strip() and not l.lstrip().startswith('#')]
    prefix = min(widths) if widths else 0
    out = []
    for line in lines:
        if not line.strip():
            out.append('')
        elif line.lstrip().startswith('#'):
            out.append(line.lstrip())
        else:
            out.append(line[prefix:])
    return '\n'.join(out)


def normalize(block):
    block = dedent(block)
    block = re.sub(r'(?m)^(\s*)return(?: true)?;\s*$', r'\1return true;', block)
    block = block.replace('__func__', '"load"')
    return block


def main():
    refdir = Path(sys.argv[1])
    staged = Path(sys.argv[2])

    checks = []

    base_vocab = (refdir / 'llama-vocab.cpp.orig').read_text()
    stag_vocab = (staged / 'llama.cpp/src/llama-vocab.cpp').read_text()

    a = normalize(cut_dispatch(base_vocab))
    b = normalize(cut_dispatch(stag_vocab))
    checks.append(('src/llama-vocab.cpp: determine vocab type', a, b))

    a = cut_body(base_vocab, PARTITION_SIGS).strip('\n')
    b = cut_body(stag_vocab, PARTITION_SIGS).strip('\n')
    checks.append(('src/llama-vocab.cpp: tokenizer_st_partition', a, b))

    base_common = (refdir / 'common.cpp.orig').read_text()
    stag_common = (staged / 'llama.cpp/common/common.cpp').read_text()
    a = cut_body(base_common, STRIP_SIGS).strip('\n')
    b = cut_body(stag_common, STRIP_SIGS).strip('\n')
    checks.append(('common/common.cpp: string_strip', a, b))

    bad = 0
    for name, x, y in checks:
        if x == y:
            print(f'PARITY ok    {name} ({len(x.splitlines())} native lines)')
        else:
            bad += 1
            print(f'PARITY FAIL  {name}')
            for i, (lx, ly) in enumerate(zip(x.splitlines(), y.splitlines())):
                if lx != ly:
                    print(f'  line {i + 1}\n    ref {lx!r}\n    new {ly!r}')
                    break
    print(f'PARITY {"FAIL" if bad else "PASS"} {len(checks) - bad}/{len(checks)}')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
