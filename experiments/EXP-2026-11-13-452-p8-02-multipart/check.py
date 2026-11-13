#!/usr/bin/env python3
"""Independent cross-check of the Goose path-component decoder.

decoded.txt holds "input hexbytes" lines produced by the compiled Goose module.
Python's urllib.parse.unquote_to_bytes is an independent implementation of the
same rule: decode %HH when both digits are hex, copy everything else literally.
"""
import sys
from pathlib import Path
from urllib.parse import unquote_to_bytes

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
E = F / 'experiments/EXP-2026-11-13-452-p8-02-multipart'


def main():
    path = F / 'runtime/exp452/build/decoded.txt'
    n = 0
    for line in path.read_text().splitlines():
        inhex, _, outhex = line.partition('\t')
        src = bytes.fromhex(inhex)
        got = bytes.fromhex(outhex) if outhex else b''
        want = unquote_to_bytes(src)
        if got != want:
            print(f'DECODE MISMATCH {src!r}: goose={got!r} python={want!r}')
            return 1
        n += 1
    print(f'FG452_DECODE_CHECK entries={n} python_unquote_match=yes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
