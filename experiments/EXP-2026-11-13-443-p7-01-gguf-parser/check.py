#!/usr/bin/env python3
"""Independent GGUF structure parser and record comparison for EXP443."""
import struct, sys
from pathlib import Path

F = Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108')
REC = F / 'runtime/exp443/build/records.bin'
MODEL = Path('/home/end0/AI/LlamaServer/LlamaServer')  # replaced below
MODEL = Path('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
FNV_OFF = 14695981039346656037
FNV_PRIME = 1099511628211


def fnv(b):
    h = FNV_OFF
    for c in b:
        h = ((h ^ c) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return h


class R:
    def __init__(self, data):
        self.d = data; self.p = 0
    def u32(self):
        v = struct.unpack_from('<I', self.d, self.p)[0]; self.p += 4; return v
    def u64(self):
        v = struct.unpack_from('<Q', self.d, self.p)[0]; self.p += 8; return v
    def take(self, n):
        if self.p + n > len(self.d): raise ValueError('eof')
        v = self.d[self.p:self.p+n]; self.p += n; return v
    def string(self):
        n = self.u64()
        if n > len(self.d) - self.p: raise ValueError('string')
        return self.take(n)


def skip_one(r, t):
    if t in (0, 1, 7): r.take(1)
    elif t in (2, 3): r.take(2)
    elif t in (4, 5, 6): r.take(4)
    elif t in (10, 11, 12): r.take(8)
    elif t == 8: r.string()
    else: raise ValueError(f'type {t}')


def parse(data):
    r = R(data)
    if len(data) < 24: raise ValueError('short')
    magic = r.u32()
    if magic != int.from_bytes(b'GGUF', 'little'): raise ValueError('magic')
    version = r.u32()
    if version not in (2, 3): raise ValueError('version')
    tc = r.u64(); kc = r.u64()
    if tc > 10_000_000 or kc > 10_000_000: raise ValueError('counts')
    w = [(version << 32) | magic, tc, kc]
    for _ in range(kc):
        key = r.string()
        klen = len(key); kh = fnv(key)
        vt = r.u32()
        a0 = a1 = 0
        if vt == 9:
            et = r.u32(); n = r.u64()
            a0, a1 = et, n
            for _ in range(n): skip_one(r, et)
        else:
            skip_one(r, vt)
        w += [klen, kh, vt, a0, a1]
    for _ in range(tc):
        name = r.string()
        nl = len(name); nh = fnv(name)
        nd = r.u32()
        if nd < 1 or nd > 4: raise ValueError('ndims')
        dims = [r.u64() for _ in range(nd)]
        dims += [0] * (4 - nd)
        ty = r.u32(); off = r.u64()
        w += [nl, nh, ty, nd] + dims + [off]
    return w


def main():
    data = MODEL.read_bytes()
    want = parse(data)
    got = list(struct.unpack('<%dQ' % (REC.stat().st_size // 8), REC.read_bytes()))
    print('python words', len(want), 'goose words', len(got))
    if want != got:
        for i, (a, b) in enumerate(zip(want, got)):
            if a != b:
                print('FIRST DIFF', i, hex(a), hex(b)); break
        return 1
    print('FG443_GGUF_CHECK PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
