#!/usr/bin/env python3
"""EXP950: btl_load reproduces the manual load sequence on the same GGUF.

The manual sequence is the one the gated EXP920 gate performs: find the three
keys, index the token and merge arrays, split the (off, len) pairs, call
bt_build, read token_type, then bt_attrs and bt_special_order. btl_load must
produce the same tables and the same attributes from the same bytes, and a
fragment tokenised through its tables must equal the manual call.

No model is loaded: one synthetic vocabulary in memory.
"""
import argparse
import ctypes as C
import struct
import sys
from pathlib import Path

I, U = C.c_int64, C.c_uint8


class S(C.Structure):
    _fields_ = [('data', C.POINTER(I)), ('len', I)]


class B(C.Structure):
    _fields_ = [('data', C.POINTER(U)), ('len', I)]


FORMATS = {0: 'B', 1: 'b', 2: 'H', 3: 'h', 4: 'I', 5: 'i', 6: 'f', 7: 'B',
           10: 'Q', 11: 'q', 12: 'd'}


def encode(ty, value):
    if ty == 8:
        return struct.pack('<Q', len(value)) + value
    if ty == 9:
        element, items = value
        return struct.pack('<IQ', element, len(items)) + b''.join(encode(element, x) for x in items)
    return struct.pack('<' + FORMATS[ty], value)


def wire_file(kv, tensors=(), alignment=32, version=3):
    data = b'GGUF' + struct.pack('<IQQ', version, len(tensors), len(kv))
    for key, ty, value in kv:
        data += struct.pack('<Q', len(key)) + key + struct.pack('<I', ty) + encode(ty, value)
    offset = 0
    for name, ne, ty, nbytes in tensors:
        data += (struct.pack('<Q', len(name)) + name + struct.pack('<I', len(ne)) +
                 struct.pack('<' + 'q' * len(ne), *ne) + struct.pack('<IQ', ty, offset))
        offset += (nbytes + alignment - 1) & -alignment
    end = len(data)
    start = (end + alignment - 1) & -alignment if tensors else end
    return data + bytes(start - end + offset)


class Slice:
    """Keeps every temporary C object alive for the process lifetime."""

    def __init__(self):
        self.alive = []

    def i64(self, array, n=None):
        self.alive.append(array)
        return S(array, len(array) if n is None else n)

    def u8(self, payload):
        buf = C.create_string_buffer(bytes(payload), len(payload))
        self.alive.append(buf)
        return B(C.cast(buf, C.POINTER(U)), len(payload))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('library', type=Path)
    args = parser.parse_args()
    lib = C.CDLL(str(args.library))
    slice_ = Slice()
    checks = 0
    failures = 0

    def check(cond, what):
        nonlocal checks, failures
        checks += 1
        if not cond:
            failures += 1
            print('FAIL', what)

    lib.goose_init.argtypes = [C.c_int, C.c_void_p]
    lib.goose_init(0, None)
    sig = {
        'gr_parse': ([B, I, S, S, S, S, S], I),
        'btl_load': ([B, S, S, S, S, S, S, S, S, S, S, S, S, S, S, I, I, I, S], I),
        'bt_build': ([B, S, S, S, S, S, S, S], I),
        'bt_attrs': ([B, S, S, S, S], I),
        'bt_special_order': ([B, S, S, S, S], I),
        'gr_find_kv': ([B, S, S, S, B], I),
        'gr_arr_n': ([S, S, I], I),
        'gr_string_array_index': ([B, S, S, I, S], I),
        'gr_array_int': ([B, S, S, I, I, S], I),
        'bt_bpe': ([B, B, S, S, S, S, S, S, S, I, S, S, B, S, S, S, S], I),
    }
    for name, (argtypes, res) in sig.items():
        fn = getattr(lib, name)
        fn.argtypes = argtypes
        fn.restype = res

    # GPT-2 byte-level alphabet: every byte has its own token text, which is what
    # the BPE path expects (a space is the two bytes of U+0120, not 0x20).
    keep = list(range(ord('!'), ord('~') + 1)) + list(range(0xA1, 0xAC + 1)) + list(range(0xAE, 0xFF + 1))
    codes = list(keep)
    extra = 0
    for b in range(256):
        if b not in keep:
            keep.append(b)
            codes.append(256 + extra)
            extra += 1
    byte_text = [chr(codes[keep.index(b)]).encode('utf-8') for b in range(256)]
    tokens = byte_text + [b'ab', b'abc', b'<|end|>']
    merges = [b'a b', b'ab c']
    types = [1] * 256 + [1, 1, 3]  # 3 = control: the special order sees one entry
    entries = [(b'general.alignment', 4, 32),
               (b'tokenizer.ggml.tokens', 9, (8, tokens)),
               (b'tokenizer.ggml.merges', 9, (8, merges)),
               (b'tokenizer.ggml.token_type', 9, (5, types))]
    blob = wire_file(entries)
    nk = len(entries)
    n_tok, n_mrg = len(tokens), len(merges)

    state, kv, tensors = (I * 8)(), (I * (nk * 8))(), (I * 12)()
    kslots, tslots = (I * 16)(), (I * 16)()
    buffer = (U * len(blob)).from_buffer_copy(blob)
    slice_.alive.append(buffer)
    data = B(C.cast(buffer, C.POINTER(U)), len(blob))
    kv_sl = slice_.i64(kv, nk * 8)

    check(lib.gr_parse(data, len(blob), slice_.i64(state), kv_sl, slice_.i64(tensors, 12),
                       slice_.i64(kslots, 16), slice_.i64(tslots, 16)) == 0, 'synthetic parse')

    def find(name):
        return lib.gr_find_kv(data, slice_.i64(state), kv_sl, slice_.i64(kslots, 16), slice_.u8(name))

    ti, mi, tyi = find(b'tokenizer.ggml.tokens'), find(b'tokenizer.ggml.merges'), find(b'tokenizer.ggml.token_type')
    check(ti >= 0 and mi >= 0 and tyi >= 0, 'three tokenizer keys found')
    check(lib.gr_arr_n(slice_.i64(state), kv_sl, ti) == n_tok, 'token count')
    check(lib.gr_arr_n(slice_.i64(state), kv_sl, mi) == n_mrg, 'merge count')

    # ---- manual sequence, exactly as the EXP920 gate does it ----
    m_tok_off, m_tok_len = (I * n_tok)(), (I * n_tok)()
    m_mrg_off, m_mrg_len = (I * n_mrg)(), (I * n_mrg)()
    pairs = (I * (2 * n_tok))()
    check(lib.gr_string_array_index(data, slice_.i64(state), kv_sl, ti, slice_.i64(pairs, 2 * n_tok)) == n_tok,
          'manual token index')
    for i in range(n_tok):
        m_tok_off[i], m_tok_len[i] = pairs[2 * i], pairs[2 * i + 1]
    mpairs = (I * (2 * n_mrg))()
    check(lib.gr_string_array_index(data, slice_.i64(state), kv_sl, mi, slice_.i64(mpairs, 2 * n_mrg)) == n_mrg,
          'manual merge index')
    for i in range(n_mrg):
        m_mrg_off[i], m_mrg_len[i] = mpairs[2 * i], mpairs[2 * i + 1]

    m_tok_slots, m_mrg_slots, m_stats = (I * 1024)(), (I * 1024)(), (I * 8)()
    check(lib.bt_build(data, slice_.i64(m_tok_off), slice_.i64(m_tok_len), slice_.i64(m_tok_slots),
                       slice_.i64(m_mrg_off), slice_.i64(m_mrg_len), slice_.i64(m_mrg_slots),
                       slice_.i64(m_stats)) == 0, 'manual bt_build')
    m_type, m_attr, m_order = (I * n_tok)(), (I * n_tok)(), (I * n_tok)()
    for i in range(n_tok):
        v = (I * 1)()
        check(lib.gr_array_int(data, slice_.i64(state), kv_sl, tyi, i, slice_.i64(v, 1)) == 0,
              f'manual type {i} readable')
        m_type[i] = v[0]
    check(list(m_type) == types, 'manual types match the file')
    check(lib.bt_attrs(data, slice_.i64(m_tok_off), slice_.i64(m_tok_len), slice_.i64(m_type),
                       slice_.i64(m_attr)) == 0, 'manual bt_attrs')
    m_spl = lib.bt_special_order(data, slice_.i64(m_tok_off), slice_.i64(m_tok_len),
                                 slice_.i64(m_attr), slice_.i64(m_order))

    # ---- btl_load on the same bytes ----
    g_tok_off, g_tok_len = (I * n_tok)(), (I * n_tok)()
    g_mrg_off, g_mrg_len = (I * n_mrg)(), (I * n_mrg)()
    g_tok_slots, g_mrg_slots = (I * 1024)(), (I * 1024)()
    g_pairs = (I * (2 * max(n_tok, n_mrg)))()
    g_type, g_attr, g_order = (I * n_tok)(), (I * n_tok)(), (I * n_tok)()
    g_eog, g_stats = (I * 8)(), (I * 8)()
    rc = lib.btl_load(data, slice_.i64(state), kv_sl, slice_.i64(kslots, 16),
                      slice_.i64(g_tok_off), slice_.i64(g_tok_len), slice_.i64(g_tok_slots),
                      slice_.i64(g_mrg_off), slice_.i64(g_mrg_len), slice_.i64(g_mrg_slots),
                      slice_.i64(g_pairs), slice_.i64(g_type), slice_.i64(g_attr),
                      slice_.i64(g_order), slice_.i64(g_eog), 5, -1, -1, slice_.i64(g_stats))
    check(rc == 0, f'btl_load rc={rc}')
    check(g_stats[0] == n_tok and g_stats[1] == n_mrg, 'btl_load counts')
    check(list(g_tok_off) == list(m_tok_off) and list(g_tok_len) == list(m_tok_len), 'token table identical')
    check(list(g_mrg_off) == list(m_mrg_off) and list(g_mrg_len) == list(m_mrg_len), 'merge table identical')
    check(list(g_tok_slots) == list(m_tok_slots), 'token hash slots identical')
    check(list(g_mrg_slots) == list(m_mrg_slots), 'merge hash slots identical')
    check(list(g_type) == list(m_type), 'token types identical')
    check(list(g_attr) == list(m_attr), 'attributes identical')
    check(g_stats[4] == m_spl, 'special count identical')
    check(list(g_order[:m_spl]) == list(m_order[:m_spl]), 'special order identical')
    check(g_stats[5] >= 0, 'eog count reported')

    # A fragment through the loaded tables must equal the manual call.
    def bpe(text, tok_off, tok_len, tok_slots, mrg_off, mrg_len, mrg_slots):
        n = len(text)
        ids = (I * 1024)()
        cpts = (I * (n + 1))()
        seg = (I * (4 * (n + 1)))()
        sym = (I * (24 * (n + 1)))()
        heap = (I * (15 * (n + 1) + 40))()
        hc = (I * 2)()
        info = (I * 8)()
        rc = lib.bt_bpe(slice_.u8(text), data,
                        slice_.i64(tok_off), slice_.i64(tok_len), slice_.i64(tok_slots),
                        slice_.i64(mrg_off), slice_.i64(mrg_len), slice_.i64(mrg_slots),
                        slice_.i64(ids), 0,
                        slice_.i64(cpts), slice_.i64(seg), slice_.u8(bytes(8 * (n + 1))),
                        slice_.i64(sym), slice_.i64(heap), slice_.i64(hc), slice_.i64(info))
        return rc, list(ids[:max(rc, 0)])

    text = b'abc ab a'
    ra, ids_a = bpe(text, g_tok_off, g_tok_len, g_tok_slots, g_mrg_off, g_mrg_len, g_mrg_slots)
    rb, ids_b = bpe(text, m_tok_off, m_tok_len, m_tok_slots, m_mrg_off, m_mrg_len, m_mrg_slots)
    check(ra == rb and ra > 0, f'tokenised length {ra} == {rb}')
    check(ids_a == ids_b, 'tokenised ids identical')
    check(ra > 0 and ids_a[0] == 257, f'merge applied: first id {ids_a[0] if ra > 0 else None} == 257')

    # Guards: a short buffer is refused with the stage named, not silently cut.
    short = (I * (n_tok - 1))()
    g_stats2 = (I * 8)()
    rc = lib.btl_load(data, slice_.i64(state), kv_sl, slice_.i64(kslots, 16),
                      slice_.i64(short), slice_.i64(g_tok_len), slice_.i64(g_tok_slots),
                      slice_.i64(g_mrg_off), slice_.i64(g_mrg_len), slice_.i64(g_mrg_slots),
                      slice_.i64(g_pairs), slice_.i64(g_type), slice_.i64(g_attr),
                      slice_.i64(g_order), slice_.i64(g_eog), 5, -1, -1, slice_.i64(g_stats2))
    check(rc == -2 and g_stats2[6] == 1, 'short token offsets refused, stage 1')
    rc = lib.btl_load(data, slice_.i64(state), kv_sl, slice_.i64(kslots, 16),
                      slice_.i64(g_tok_off), slice_.i64(g_tok_len), slice_.i64(g_tok_slots),
                      slice_.i64(g_mrg_off), slice_.i64(g_mrg_len), slice_.i64(g_mrg_slots),
                      slice_.i64(g_pairs), slice_.i64(g_type), slice_.i64(g_attr),
                      slice_.i64(g_order), slice_.i64(g_eog), 5, -1, -1, slice_.i64((I * 6)()))
    check(rc == -1, 'short stats refused')

    print(f'{"PASS" if failures == 0 else "BAD"} checks={checks} failures={failures}')
    return 1 if failures else 0


if __name__ == '__main__':
    sys.exit(main())
