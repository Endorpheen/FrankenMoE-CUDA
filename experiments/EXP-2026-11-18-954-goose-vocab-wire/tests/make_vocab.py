#!/usr/bin/env python3
"""Build a tiny valid vocabulary GGUF for the EXP954 gate.

Same wire encoder as EXP948/950: no dependency on the runtime, no model.
"""
import struct
import sys
from pathlib import Path

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


def byte_alphabet():
    keep = list(range(ord('!'), ord('~') + 1)) + list(range(0xA1, 0xAC + 1)) + list(range(0xAE, 0xFF + 1))
    codes = list(keep)
    extra = 0
    for b in range(256):
        if b not in keep:
            keep.append(b)
            codes.append(256 + extra)
            extra += 1
    return [chr(codes[keep.index(b)]).encode('utf-8') for b in range(256)]


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else 'vocab.gguf')
    tokens = byte_alphabet() + [b'ab', b'abc', b'<|end|>', b'zzz']
    merges = [b'a b', b'ab c']
    types = [1] * 256 + [1, 1, 3, 1]
    kv = [
        (b'general.architecture', 8, b'llama'),
        (b'general.name', 8, b'exp954-synthetic'),
        (b'llama.vocab_size', 4, len(tokens)),
        (b'tokenizer.ggml.model', 8, b'gpt2'),
        (b'tokenizer.ggml.pre', 8, b'default'),
        (b'tokenizer.ggml.tokens', 9, (8, tokens)),
        (b'tokenizer.ggml.merges', 9, (8, merges)),
        (b'tokenizer.ggml.token_type', 9, (5, types)),
        (b'tokenizer.ggml.bos_token_id', 4, 1),
        (b'tokenizer.ggml.eos_token_id', 4, 2),
        (b'tokenizer.ggml.unknown_token_id', 4, 0),
        (b'tokenizer.ggml.add_bos_token', 7, 0),
        (b'tokenizer.ggml.add_eos_token', 7, 0),
    ]
    blob = wire_file(kv)
    out.write_bytes(blob)
    print(f'wrote {out} {len(blob)} bytes, {len(tokens)} tokens, {len(merges)} merges')


if __name__ == '__main__':
    main()
