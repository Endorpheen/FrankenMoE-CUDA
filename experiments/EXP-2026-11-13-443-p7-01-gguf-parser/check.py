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



def find(data, key):
    r = R(data); r.u32(); r.u32(); r.u64(); kc = r.u64()
    for _ in range(kc):
        k = r.string(); vt = r.u32()
        if k == key.encode():
            if vt == 9:
                et = r.u32(); cnt = r.u64()
                return (vt, r.p, cnt, et, cnt)
            if vt == 8:
                off = r.p; n = r.u64()
                return (vt, off + 8, n, 0, 0)
            size = {0:1,1:1,7:1,2:2,3:2,4:4,5:4,6:4,10:8,11:8,12:8}[vt]
            return (vt, r.p, size, 0, 0)
        if vt == 9:
            et = r.u32(); n = r.u64()
            for _ in range(n): skip_one(r, et)
        else:
            skip_one(r, vt)
    return None

def vhash(data, t, off, ln, et, cnt):
    if t != 9: return fnv(data[off:off+ln])
    if et == 8:
        h = FNV_OFF; p = off
        for _ in range(cnt):
            n = struct.unpack_from('<Q', data, p)[0]
            eh = fnv(data[p+8:p+8+n])
            for _ in range(8):
                h = ((h ^ (eh & 0xff)) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF; eh >>= 8
            p += 8 + n
        return h
    size = {0:1,1:1,7:1,2:2,3:2,4:4,5:4,6:4,10:8,11:8,12:8}[et]
    return fnv(data[off:off+cnt*size])

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
    keys = ['general.architecture','llama.context_length','tokenizer.ggml.model',
            'tokenizer.ggml.tokens','tokenizer.ggml.scores','general.alignment']
    exp = []
    for key in keys:
        v = find(data, key)
        if v is None:
            exp.append(f'{key} absent rc=0')
        else:
            t, off, ln, et, cnt = v
            exp.append(f'{key} type={t} off={off} len={ln} et={et} cnt={cnt} hash={vhash(data,t,off,ln,et,cnt):016x}')
    got = (REC.parent/'lookup.txt').read_text().splitlines()
    if exp != got:
        for i,(a,b) in enumerate(zip(exp,got)):
            if a != b: print('LOOKUP DIFF',i); print(' py ',a); print(' go ',b); break
        return 1
    print('FG443_GGUF_CHECK PASS')
    return 0


if __name__ == '__main__':
    sys.exit(main())
