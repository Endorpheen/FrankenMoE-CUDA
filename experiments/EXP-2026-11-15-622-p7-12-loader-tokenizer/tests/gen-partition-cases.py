#!/usr/bin/env python3
"""Corpus for the special-token partitioning gate (S2).

Every case is one line: parse_special, the input text in hex, and the special
tokens as id:attr:text-hex separated by commas. Hex keeps whitespace and bytes
above 127 unambiguous, which is exactly what this slice is about.

Attributes are the native llama_token_attr bits: UNKNOWN 1, UNUSED 2, NORMAL 4,
CONTROL 8, USER_DEFINED 16, BYTE 32, NORMALIZED 64, LSTRIP 128, RSTRIP 256.

The loader's order (special tokens by text length, longest first) is derived
here the same way the fixture derives it: length descending, then id.
"""
import sys
from pathlib import Path

OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else (
    Path(__file__).resolve().parent.parent / 'build' / 'partition-cases.tsv')

UNKNOWN, NORMAL, CONTROL, USER_DEFINED = 1, 4, 8, 16
LSTRIP, RSTRIP = 128, 256

cases = []


def case(text, tokens, parse_special=1):
    """tokens: list of (id, text, attr)."""
    h = text.encode('utf-8').hex() or ''
    ts = ','.join(f'{i}:{a}:{t.encode("utf-8").hex()}' for i, t, a in tokens)
    cases.append((parse_special, h, ts))


# --- one token, the four positions ------------------------------------------
case('a<|s|>b', [(1, '<|s|>', CONTROL)])
case('a<|s|>b', [(1, '<|s|>', CONTROL | USER_DEFINED)])
case('<|s|>b', [(1, '<|s|>', CONTROL)])
case('a<|s|>', [(1, '<|s|>', CONTROL)])
case('<|s|>', [(1, '<|s|>', CONTROL)])
case('a<|s|><|s|>b', [(1, '<|s|>', CONTROL)])
case('<|s|><|s|><|s|>', [(1, '<|s|>', CONTROL)])
case('aaaa', [(1, 'aa', CONTROL)])
case('abab', [(1, 'ab', CONTROL)])

# --- longer candidate wins because the loader order is longest first --------
case('xabcx', [(1, 'ab', CONTROL), (2, 'abc', CONTROL)])
case('xabcx', [(1, 'ab', CONTROL), (2, 'bc', CONTROL)])
case('abcabc', [(1, 'abc', CONTROL), (2, 'bca', CONTROL)])

# --- strip attributes -------------------------------------------------------
case('a <|s|> b', [(1, '<|s|>', CONTROL | LSTRIP)])
case('a <|s|> b', [(1, '<|s|>', CONTROL | RSTRIP)])
case('a \t <|s|> \n b', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])
case('    <|s|>    ', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])
case('   <|s|>x', [(1, '<|s|>', CONTROL | LSTRIP)])
case('x<|s|>   ', [(1, '<|s|>', CONTROL | RSTRIP)])
case('\n\t<|s|>\r\v\f', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])
case('a<|s|>b', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])
case('<|s|>\n<|s|>', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])
case('a  <|s|>  b  <|s|>  c', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])
case('a<|s|> <|s|>b', [(1, '<|s|>', CONTROL | LSTRIP | RSTRIP)])

# --- parse_special = 0: control and unknown are skipped ---------------------
case('a<|s|>b', [(1, '<|s|>', CONTROL)], parse_special=0)
case('a<|s|>b', [(1, '<|s|>', UNKNOWN)], parse_special=0)
case('a<|s|>b', [(1, '<|s|>', USER_DEFINED)], parse_special=0)
case('a<|s|>b', [(1, '<|s|>', CONTROL | USER_DEFINED)], parse_special=0)
case('a<|s|>b', [(1, '<|s|>', NORMAL)], parse_special=0)
case('a<|s|>b', [(1, '<|s|>', CONTROL | NORMAL)], parse_special=0)

# --- literal text, not a pattern -------------------------------------------
case('a.b*c', [(1, '.', CONTROL)])
case('a.b*c', [(1, '*', CONTROL)])
case('a\\b', [(1, '\\', CONTROL)])

# --- bytes above 127 --------------------------------------------------------
case('\u00e9<|s|>\u00e8', [(1, '<|s|>', CONTROL)])
case('\u00e9<|s|>\u00e8', [(1, '\u00e9', CONTROL)])
case('\u00ff\u00fe', [(1, '\u00fe', CONTROL | USER_DEFINED)])

# --- several specials at once ----------------------------------------------
case('a<|a|>b<|bb|>c<|ccc|>', [(1, '<|ccc|>', CONTROL), (2, '<|bb|>', CONTROL),
                               (3, '<|a|>', CONTROL)])
case('[INST] x [/INST] y', [(1, '[INST]', CONTROL), (2, '[/INST]', CONTROL)])
case('<|im_start|>u<|im_end|>', [(1, '<|im_start|>', CONTROL),
                                (2, '<|im_end|>', CONTROL)])
case('a\nb', [(1, '\n', CONTROL)])
case('  ', [(1, ' ', CONTROL | LSTRIP | RSTRIP)])
case('x', [(1, 'x', CONTROL | LSTRIP | RSTRIP)])
case('', [(1, 'x', CONTROL)])
case('abc', [(1, 'zz', CONTROL)])


# --- deterministic fuzz: the index arithmetic of the split/erase path --------
import random

rng = random.Random(20261115)
POOL = ['<|a|>', '<|b|>', 'a', 'ab', 'b', ' ', '\n', '<', '|>', 'aa']
ATTRS = [CONTROL, CONTROL | LSTRIP, CONTROL | RSTRIP, CONTROL | LSTRIP | RSTRIP,
         USER_DEFINED, USER_DEFINED | RSTRIP, NORMAL, UNKNOWN, CONTROL | USER_DEFINED]
ALPHA = ['a', 'b', ' ', '\n', '<', '>', '|', '\t']

for _ in range(400):
    n = rng.randint(0, 24)
    text = ''.join(rng.choice(ALPHA) for _ in range(n))
    k = rng.randint(1, 5)
    picks = rng.sample(POOL, k)
    toks = [(i + 1, picks[i], rng.choice(ATTRS)) for i in range(k)]
    case(text, toks, parse_special=rng.randint(0, 1))


def order(tokens):
    return sorted(tokens, key=lambda t: (-len(t[1].encode('utf-8')), t[0]))


with OUT.open('w') as f:
    for ps, h, ts in cases:
        f.write(f'{ps}\t{h}\t{ts}\n')
print(f'{len(cases)} cases -> {OUT}')
