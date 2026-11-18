#!/usr/bin/env python3
"""Write a one-layer GGUF from the real Apex weights.

Purpose (EXP923): llama.cpp can only be compared at the logits level, so to
isolate a per-layer divergence both implementations must see the same truncated
model. This copies the metadata with block_count=1 / nextn=0 and the tensors of
block 0 plus the embedding, the output norm and the head, byte for byte.

    python3 make_layer_gguf.py <source.gguf> <out.gguf> [--blocks N]

Only the subset llama.cpp needs for a 1-block qwen35moe is written; tensor
payloads are copied verbatim (they are quantised blocks, so nothing is decoded).
"""
import argparse
import struct
import sys
from pathlib import Path

SZ = {0: 1, 1: 1, 2: 2, 3: 2, 4: 4, 5: 4, 6: 4, 7: 1, 10: 8, 11: 8, 12: 8}
# ggml type -> (block size, bytes per block); exact payload sizes come from this
TYPE = {0: (1, 4), 1: (1, 2), 2: (32, 18), 3: (32, 20), 6: (32, 22), 7: (32, 24), 8: (32, 34),
        9: (32, 36), 10: (256, 84), 11: (256, 110), 12: (256, 144), 13: (256, 176), 14: (256, 210),
        15: (256, 292), 16: (256, 66), 17: (256, 74), 18: (256, 98), 19: (256, 50), 20: (32, 18),
        21: (256, 110), 22: (256, 82), 23: (256, 136), 24: (1, 1), 25: (1, 2), 26: (1, 4), 27: (1, 8),
        28: (1, 8), 29: (256, 56), 30: (1, 2), 34: (256, 54), 35: (256, 66), 36: (256, 74), 37: (32, 20),
        38: (32, 18), 39: (256, 110)}
BOOL, U32, I32, U64, F32, STR, ARR = 7, 4, 5, 10, 6, 8, 9


def read_kv(f):
    n = struct.unpack("<Q", f.read(8))[0]
    key = f.read(n).decode()
    t = struct.unpack("<I", f.read(4))[0]
    if t in SZ:
        raw = f.read(SZ[t])
        return key, t, raw
    if t == STR:
        m = struct.unpack("<Q", f.read(8))[0]
        return key, t, f.read(m)
    if t == ARR:
        et = struct.unpack("<I", f.read(4))[0]
        cnt = struct.unpack("<Q", f.read(8))[0]
        vals = []
        for _ in range(cnt):
            if et == STR:
                m = struct.unpack("<Q", f.read(8))[0]
                vals.append(f.read(m))
            else:
                vals.append(f.read(SZ[et]))
        return key, t, (et, vals)
    raise ValueError(t)


def write_kv(out, key, t, val):
    kb = key.encode()
    out.write(struct.pack("<Q", len(kb)))
    out.write(kb)
    out.write(struct.pack("<I", t))
    if t in SZ:
        out.write(val)
    elif t == STR:
        out.write(struct.pack("<Q", len(val)))
        out.write(val)
    else:
        et, vals = val
        out.write(struct.pack("<I", et))
        out.write(struct.pack("<Q", len(vals)))
        for v in vals:
            if et == STR:
                out.write(struct.pack("<Q", len(v)))
                out.write(v)
            else:
                out.write(v)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source", type=Path)
    ap.add_argument("out", type=Path)
    ap.add_argument("--src-blocks", type=int, nargs="+", default=[0],
                    help="source block indices, written as blocks 0..n-1 in order")
    ap.add_argument("--zero-blocks", type=int, nargs="*", default=[],
                    help="output block indices whose tensor payloads are zeroed (isolation)")
    ap.add_argument("--zero-match", nargs="*", default=[],
                    help="written tensor names containing any of these strings are zeroed")
    ap.add_argument("--zero-qgate", type=int, nargs="*", default=[],
                    help="output block indices whose attn_q gate half (second hd rows of each 2*hd) is zeroed")
    a = ap.parse_args()

    f = open(a.source, "rb")
    magic = f.read(4)
    assert magic == b"GGUF", magic
    version, n_tensors, n_kv = struct.unpack("<IQQ", f.read(20))

    kvs = [read_kv(f) for _ in range(n_kv)]
    infos = []
    for _ in range(n_tensors):
        n = struct.unpack("<Q", f.read(8))[0]
        name = f.read(n).decode()
        dims_n = struct.unpack("<I", f.read(4))[0]
        dims = list(struct.unpack("<%dQ" % dims_n, f.read(8 * dims_n)))
        ttype = struct.unpack("<I", f.read(4))[0]
        off = struct.unpack("<Q", f.read(8))[0]
        infos.append((name, dims, ttype, off))

    meta_end = f.tell()
    alignment = 32
    for key, t, val in kvs:
        if key == "general.alignment":
            alignment = struct.unpack("<I", val)[0]
    data_start = meta_end + (-meta_end) % alignment

    # sizes come from the source descriptors: recompute from the offsets
    by_name = {i[0]: i for i in infos}
    ordered = sorted(infos, key=lambda i: i[3])
    sizes = {}
    for idx, (name, dims, ttype, off) in enumerate(ordered):
        end = ordered[idx + 1][3] if idx + 1 < len(ordered) else None
        sizes[name] = None  # filled below from the payload length

    rename = {}
    for k, src_block in enumerate(a.src_blocks):
        for n in by_name:
            if n.startswith("blk.%d." % src_block):
                rename[n] = "blk.%d.%s" % (k, n[len("blk.%d." % src_block):])
    keep = ["token_embd.weight", "output_norm.weight", "output.weight"] + list(rename)
    keep = [n for n in keep if n in by_name]

    # metadata, with the truncated block count
    out_kvs = []
    for key, t, val in kvs:
        if key == "qwen35moe.block_count":
            out_kvs.append((key, t, struct.pack("<I", len(a.src_blocks))))
        elif key == "qwen35moe.nextn_predict_layers":
            out_kvs.append((key, t, struct.pack("<I", 0)))
        elif key == "qwen35moe.attention.recurrent_layers":
            out_kvs.append((key, t, (BOOL, [val[1][b] for b in a.src_blocks])))
        else:
            out_kvs.append((key, t, val))

    chosen = [by_name[n] for n in keep]
    chosen.sort(key=lambda i: i[3])

    # payload lengths come from the descriptor itself: elements / block * bytes
    tt_sizes = {}
    for name, dims, ttype, off in chosen:
        if ttype not in TYPE:
            raise SystemExit(f"unsupported type {ttype} for {name}")
        blk, tb = TYPE[ttype]
        elements = 1
        for d in dims:
            elements *= d
        if elements % blk:
            raise SystemExit(f"{name}: elements {elements} not a multiple of block {blk}")
        tt_sizes[name] = (data_start + off, (elements // blk) * tb)

    out = open(a.out, "wb")
    out.write(b"GGUF")
    out.write(struct.pack("<IQQ", version, len(chosen), len(out_kvs)))
    for key, t, val in out_kvs:
        write_kv(out, key, t, val)

    offsets = {}
    cur = 0
    for name, dims, ttype, off in chosen:
        offsets[name] = cur
        start, length = tt_sizes[name]
        cur += length
        cur += (-cur) % alignment
    data_len = cur

    for name, dims, ttype, off in chosen:
        nb = name.encode()
        nb = rename.get(name, name).encode()
        out.write(struct.pack("<Q", len(nb)))
        out.write(nb)
        out.write(struct.pack("<I", len(dims)))
        out.write(struct.pack("<%dQ" % len(dims), *dims))
        out.write(struct.pack("<I", ttype))
        out.write(struct.pack("<Q", offsets[name]))

    out.write(b"\0" * ((-out.tell()) % alignment))
    zero_prefix = tuple("blk.%d." % b for b in a.zero_blocks)
    # the q projection emits, per head, hd query rows then hd gate rows
    hd = None
    for key, t, val in kvs:
        if key == "qwen35moe.attention.key_length":
            hd = struct.unpack("<I", val)[0]
    qgate_rows = {}
    if a.zero_qgate:
        if hd is None:
            raise SystemExit("--zero-qgate needs attention.key_length")
        for b in a.zero_qgate:
            name = "blk.%d.attn_q.weight" % a.src_blocks[b]
            if name not in by_name:
                raise SystemExit("no " + name)
            n_, dims_, ttype_, _ = by_name[name]
            blk, tb = TYPE[ttype_]
            if dims_[0] % blk:
                raise SystemExit("row not block aligned")
            row_bytes = (dims_[0] // blk) * tb
            qgate_rows[rename.get(name, name)] = (row_bytes, dims_[1], hd)
    for name, dims, ttype, off in chosen:
        start, length = tt_sizes[name]
        f.seek(start)
        payload = f.read(length)
        assert len(payload) == length
        written = rename.get(name, name)
        if written.startswith(zero_prefix) or any(m in written for m in a.zero_match):
            payload = bytes(length)
        elif written in qgate_rows:
            row_bytes, n_rows, hd_v = qgate_rows[written]
            buf = bytearray(payload)
            for r in range(n_rows):
                if (r % (2 * hd_v)) >= hd_v:
                    buf[r * row_bytes:(r + 1) * row_bytes] = bytes(row_bytes)
            payload = bytes(buf)
        out.write(payload)
        out.write(b"\0" * ((-length) % alignment))

    out.close()
    f.close()
    print(f"wrote {a.out} tensors={len(chosen)} kv={len(out_kvs)} data={data_len} "
          f"bytes={a.out.stat().st_size}")
    print("kept:", " ".join(n for n, *_ in chosen))
    print("written as:", " ".join(rename.get(n, n) for n, *_ in chosen))


if __name__ == "__main__":
    main()
