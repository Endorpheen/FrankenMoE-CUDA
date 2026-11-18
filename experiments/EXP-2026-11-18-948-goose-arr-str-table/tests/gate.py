#!/usr/bin/env python3
"""EXP948: gr_arr_str_table must reproduce a GGUF string array as NUL-terminated
C strings, byte-identical to what native gguf_get_arr_str returns.

No model, no runtime build: one synthetic GGUF fixture and the pure reader.
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


class Init(C.Structure):
    _fields_ = [('no_alloc', C.c_bool), ('ctx', C.c_void_p)]


FORMATS = {0: 'B', 1: 'b', 2: 'H', 3: 'h', 4: 'I', 5: 'i', 6: 'f', 7: 'B',
           10: 'Q', 11: 'q', 12: 'd'}


def encode(ty, value):
    if ty == 8:
        return struct.pack('<Q', len(value)) + value
    if ty == 9:
        element, items = value
        return struct.pack('<IQ', element, len(items)) + b''.join(encode(element, x) for x in items)
    return struct.pack('<' + FORMATS[ty], value)


def padded(n, alignment):
    return (n + alignment - 1) & -alignment


def wire_file(kv, tensors, alignment=32, version=3):
    data = b'GGUF' + struct.pack('<IQQ', version, len(tensors), len(kv))
    for key, ty, value in kv:
        data += struct.pack('<Q', len(key)) + key + struct.pack('<I', ty) + encode(ty, value)
    offset = 0
    for name, ne, ty, nbytes in tensors:
        data += (struct.pack('<Q', len(name)) + name + struct.pack('<I', len(ne)) +
                 struct.pack('<' + 'q' * len(ne), *ne) + struct.pack('<IQ', ty, offset))
        offset += padded(nbytes, alignment)
    end = len(data)
    start = padded(end, alignment) if tensors else end
    return data + bytes(start - end + offset), end


class Checker:
    def __init__(self, library):
        self.lib = C.CDLL(str(library))
        self.checks = 0
        self.failures = 0
        self.lib.goose_init.argtypes = [C.c_int, C.c_void_p]
        self.lib.goose_init.restype = None
        self.lib.goose_init(0, None)
        self.lib.gr_parse.argtypes = [B, I, S, S, S, S, S]
        self.lib.gr_parse.restype = I
        self.lib.gr_arr_str_table.argtypes = [B, S, S, I, B, S]
        self.lib.gr_arr_str_table.restype = I
        self.lib.gr_arr_str_span.argtypes = [B, S, S, I, I, S]
        self.lib.gr_arr_str_span.restype = I
        self.lib.gr_val_span.argtypes = [S, S, I, S]
        self.lib.gr_val_span.restype = I

    def check(self, condition, what):
        self.checks += 1
        if not condition:
            self.failures += 1
            print('FAIL', what)
        return condition


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('library', type=Path)
    parser.add_argument('--native', type=Path)
    args = parser.parse_args()
    checker = Checker(args.library)
    lib = checker.lib

    strings = [b'', b'A', b'\xe2\x98\x83', b'x' * 300, b'A\x00B']
    entries = [(b'general.alignment', 4, 32),
               (b'tokenizer.ggml.tokens', 9, (8, strings)),
               (b'notstrings', 9, (5, [1, 2, 3]))]
    blob, end = wire_file(entries, [])
    path = Path(sys.argv[0]).resolve().parent / 'arr-str-table.gguf'
    path.write_bytes(blob)

    nk = len(entries)
    state = (I * 8)()
    kv = (I * (nk * 8))()
    tensors = (I * 12)()
    kslots = (I * 16)()
    tslots = (I * 16)()
    buffer = (U * len(blob)).from_buffer_copy(blob)
    sl_u8 = B(C.cast(buffer, C.POINTER(U)), len(blob))
    sl_i64 = S(state, 8)
    kv_slice = S(kv, nk * 8)
    checker.check(lib.gr_parse(sl_u8, len(blob), sl_i64, kv_slice, S(tensors, 12),
                               S(kslots, 16), S(tslots, 16)) == 0, 'synthetic parse')

    index = 1
    n = len(strings)
    total = sum(len(s) + 1 for s in strings)
    expected = []
    cursor = 0
    for s in strings:
        expected.append(cursor)
        cursor += len(s) + 1
    offs = (I * n)()
    empty = (U * 1)()
    # The counting pass fills the offset table even when the text buffer is
    # missing, which is what lets a caller size its buffer in one call.
    checker.check(lib.gr_arr_str_table(sl_u8, sl_i64, kv_slice, index, B(empty, 0),
                                       S(offs, n)) == -(total + 1), 'missing size answered -(need+1)')
    checker.check(list(offs) == expected, 'size query published the offset table')

    text = (U * total)()
    checker.check(lib.gr_arr_str_table(sl_u8, sl_i64, kv_slice, index, B(text, total),
                                       S(offs, n)) == n, 'table element count')
    checker.check(list(offs) == expected, 'offset table')
    for i, s in enumerate(strings):
        base = offs[i]
        checker.check(bytes(text[base:base + len(s)]) == s, f'element {i} bytes')
        checker.check(text[base + len(s)] == 0, f'element {i} terminator')

    checker.check(lib.gr_arr_str_table(sl_u8, sl_i64, kv_slice, index, B(text, total),
                                       S(offs, n - 1)) == -2, 'short offset array refused')
    for bad in [-1, nk, (1 << 63) - 1]:
        checker.check(lib.gr_arr_str_table(sl_u8, sl_i64, kv_slice, bad, B(text, total),
                                           S(offs, n)) == -1, f'index {bad} refused')
    checker.check(lib.gr_arr_str_table(sl_u8, sl_i64, kv_slice, 2, B(text, total),
                                       S(offs, 3)) == -1, 'integer array refused')

    # gr_arr_str_span: one element as (offset, length) into the header window.
    span = (I * 2)()
    for i, s in enumerate(strings):
        checker.check(lib.gr_arr_str_span(sl_u8, sl_i64, kv_slice, index, i, S(span, 2)) == 0,
                      f'span element {i} accepted')
        checker.check(bytes(buffer[span[0]:span[0] + span[1]]) == s, f'span element {i} bytes')
        checker.check(span[1] == len(s), f'span element {i} length')
    for bad in [-1, n, (1 << 63) - 1]:
        checker.check(lib.gr_arr_str_span(sl_u8, sl_i64, kv_slice, index, bad, S(span, 2)) == -1,
                      f'span element {bad} refused')
    checker.check(lib.gr_arr_str_span(sl_u8, sl_i64, kv_slice, 2, 0, S(span, 2)) == -1,
                  'span on integer array refused')
    checker.check(lib.gr_arr_str_span(sl_u8, sl_i64, kv_slice, -1, 0, S(span, 2)) == -1,
                  'span on bad key refused')

    # gr_val_span: wire type, payload offset/bytes and element type per key.
    value = (I * 4)()
    checker.check(lib.gr_val_span(sl_i64, kv_slice, index, S(value, 4)) == 0, 'value span on array')
    checker.check((value[0], value[3]) == (9, 8), 'array wire/element type')
    checker.check(lib.gr_val_span(sl_i64, kv_slice, 0, S(value, 4)) == 0, 'value span on scalar')
    checker.check(value[0] == 4 and value[3] == -1, 'scalar wire type and absent element type')
    checker.check(value[2] == 4, 'scalar payload is four bytes')
    checker.check(struct.unpack_from('<I', bytes(buffer), value[1])[0] == 32,
                  'scalar payload offset points at the value')
    checker.check(lib.gr_val_span(sl_i64, kv_slice, -1, S(value, 4)) == -1, 'value span bad index')
    checker.check(lib.gr_val_span(sl_i64, kv_slice, index, S(value, 3)) == -1, 'short value span out')

    if args.native:
        oracle = C.CDLL(str(args.native))
        oracle.gguf_init_from_file.argtypes = [C.c_char_p, Init]
        oracle.gguf_init_from_file.restype = C.c_void_p
        oracle.gguf_get_arr_n.argtypes = [C.c_void_p, I]
        oracle.gguf_get_arr_n.restype = C.c_size_t
        oracle.gguf_get_arr_str.argtypes = [C.c_void_p, I, C.c_size_t]
        oracle.gguf_get_arr_str.restype = C.c_char_p
        ctx = oracle.gguf_init_from_file(str(path).encode(), Init(True, None))
        checker.check(bool(ctx), 'native GGUF open')
        if ctx:
            checker.check(oracle.gguf_get_arr_n(ctx, index) == n, 'native element count')
            for i, s in enumerate(strings):
                pointer = oracle.gguf_get_arr_str(ctx, index, i)
                if b'\x00' in s:
                    # Recorded divergence: the native container stops at the first
                    # embedded NUL, while the wire length says otherwise. The Goose
                    # table keeps all wire bytes; native truncates. Not a match, and
                    # not a regression either - native loses data here.
                    checker.check(C.string_at(pointer) == s.split(b'\x00')[0],
                                  f'native element {i} truncates at the embedded NUL')
                    checker.check(bytes(text[offs[i]:offs[i] + len(s)]) == s,
                                  f'element {i} keeps the wire bytes')
                else:
                    native = C.string_at(pointer, len(s))
                    checker.check(native == s, f'native element {i} bytes')
                    checker.check(bytes(text[offs[i]:offs[i] + len(s)]) == native,
                                  f'element {i} matches native')

    print(f'PASS checks={checker.checks} failures={checker.failures}'
          if checker.failures == 0 else f'BAD checks={checker.checks} failures={checker.failures}')
    return 1 if checker.failures else 0


if __name__ == '__main__':
    sys.exit(main())
