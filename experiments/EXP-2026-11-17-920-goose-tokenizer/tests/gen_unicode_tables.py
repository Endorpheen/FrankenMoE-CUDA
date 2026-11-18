#!/usr/bin/env python3
"""Generate goose/misc/unicode-tables.goose from the pinned native Unicode data.

The native file is read, never modified. Its sha256 is written into the module
header and the experiment evidence so the table can be reproduced.

The emitted flag table is the DERIVED array native builds in
unicode_cpt_flags_array(), not the raw range list: range flags first, then the
whitespace set, the lowercase and uppercase maps and the NFD ranges OR their
bits in. Emitting the raw ranges would silently drop is_whitespace for a space,
which is exactly the bit every pre-tokenizer tests.

    python3 gen_unicode_tables.py [native-unicode-data.cpp] [out.goose]
"""
import hashlib
import re
import sys
from pathlib import Path

DEFAULT_NATIVE = Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/src/unicode-data.cpp')
DEFAULT_OUT = Path(__file__).resolve().parents[3] / 'goose/misc/unicode-tables.goose'

MAX_CPT = 0x110000
HEX = r'(0x[0-9A-Fa-f]+|\d+)'


def block(src, name):
    i = src.index(name)
    j = src.index('};', i)
    return src[i:j]


def pairs(body):
    return [(int(a, 0), int(b, 0)) for a, b in
            re.findall(r'\{\s*' + HEX + r'\s*,\s*' + HEX + r'\s*\}', body)]


def main():
    native = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_NATIVE
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUT
    src = native.read_text()
    digest = hashlib.sha256(native.read_bytes()).hexdigest()

    raw = pairs(block(src, 'unicode_ranges_flags = {'))
    lower = pairs(block(src, 'unicode_map_lowercase'))
    upper = pairs(block(src, 'unicode_map_uppercase'))
    whitespace = [int(v, 0) for v in re.findall(HEX, block(src, 'unicode_set_whitespace = {'))]
    nfd = pairs(block(src, 'unicode_ranges_nfd = {'))

    assert raw[0][0] == 0 and raw[-1][0] == MAX_CPT, 'flag table edges'
    assert all(raw[i][0] < raw[i + 1][0] for i in range(len(raw) - 1)), 'flag starts sorted'
    assert all(lower[i][0] < lower[i + 1][0] for i in range(len(lower) - 1)), 'lower sorted'

    WHITESPACE, LOWERCASE, UPPERCASE, NFD = 0x0100, 0x0200, 0x0400, 0x0800
    flags = [0] * MAX_CPT
    for i in range(1, len(raw)):
        start, value = raw[i - 1]
        end = raw[i][0]
        for cpt in range(start, end):
            flags[cpt] = value
    for cpt in whitespace:
        flags[cpt] |= WHITESPACE
    for _, mapped in lower:
        flags[mapped] |= LOWERCASE
    for _, mapped in upper:
        flags[mapped] |= UPPERCASE
    for start, last, _ in nfd:
        for cpt in range(start, last + 1):
            flags[cpt] |= NFD
    for cpt in range(MAX_CPT):
        assert flags[cpt] != 0, hex(cpt)

    # run-length encode the derived array back into (start, flags) ranges
    ranges = []
    for cpt in range(MAX_CPT):
        if not ranges or ranges[-1][1] != flags[cpt]:
            ranges.append((cpt, flags[cpt]))
    assert len(ranges) < 6000, len(ranges)

    def arr(name, values):
        rows = []
        for i in range(0, len(values), 16):
            rows.append(','.join(str(v) for v in values[i:i + 16]))
        return 'const %s: i64[] = [\n%s\n];\n' % (name, ',\n'.join(rows))

    body = '''// Unicode codepoint tables, generated from the pinned native data file.
//
// Source: src/unicode-data.cpp sha256 %s (llama.cpp-flint-cuda, commit 1b44e307).
// Regenerate with experiments/EXP-2026-11-17-920-goose-tokenizer/tests/gen_unicode_tables.py;
// do not edit by hand. The native file is only read.
//
// uf_flags is unicode_cpt_flags_from_cpt: the DERIVED flag array of
// unicode_cpt_flags_array, run-length encoded as (inclusive start, flag word).
// START holds each range's first codepoint plus the 0x110000 sentinel; FLAG[i]
// is range i's word. A codepoint at or past the sentinel is UNDEFINED (0x1),
// which is the native answer for cpt >= table size.
// Flag bits: 1 undefined, 2 number, 4 letter, 8 separator, 16 accent mark,
// 32 punctuation, 64 symbol, 128 control, 256 whitespace, 512 lowercase,
// 1024 uppercase, 2048 NFD.
//
// ul_lower is unicode_tolower: CPT is sorted, a codepoint without an entry maps
// to itself (native lower_bound miss).

%s
%s
%s
%s
const UF_N: i64 = %d;
const UL_N: i64 = %d;

// Largest index with START[i] <= cpt, restricted to the real ranges.
fn uf_range(cpt: i64) -> i64 {
    var lo: i64 = 0;
    var hi: i64 = UF_N;
    while lo + 1 < hi {
        let mid: i64 = (lo + hi) / 2;
        if UFL_START[mid] <= cpt {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    if lo >= UF_N - 1 {
        return 0 - 1;
    }
    return lo;
}

export fn uf_flags(cpt: i64) -> i64 {
    if cpt < 0 {
        return 1;
    }
    let i: i64 = uf_range(cpt);
    if i < 0 {
        return 1;
    }
    return UFL_FLAG[i];
}

export fn ul_lower(cpt: i64) -> i64 {
    var lo: i64 = 0;
    var hi: i64 = UL_N;
    while lo < hi {
        let mid: i64 = (lo + hi) / 2;
        if ULL_CPT[mid] < cpt {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    if lo < UL_N && ULL_CPT[lo] == cpt {
        return ULL_LOW[lo];
    }
    return cpt;
}
''' % (digest, arr('UFL_START', [s for s, _ in ranges] + [MAX_CPT]),
       arr('UFL_FLAG', [f for _, f in ranges]),
       arr('ULL_CPT', [c for c, _ in lower]),
       arr('ULL_LOW', [l for _, l in lower]),
       len(ranges) + 1, len(lower))

    out.write_text(body)
    print('native', native, digest)
    print('ranges', len(ranges), 'lower', len(lower), '->', out)


if __name__ == '__main__':
    main()
