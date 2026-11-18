#!/usr/bin/env python3
"""Bitwise independent FP32 oracle and bounded real PLE row reads."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes as C
import io
import json
import math
import os
from pathlib import Path
import random
import struct


class Bytes(C.Structure):
    _fields_ = [("data", C.POINTER(C.c_uint8)), ("len", C.c_int64)]


class Floats(C.Structure):
    _fields_ = [("data", C.POINTER(C.c_float)), ("len", C.c_int64)]


def header_info(header):
    stream = io.BytesIO(header)

    def read(fmt):
        size = struct.calcsize("<" + fmt)
        return struct.unpack("<" + fmt, stream.read(size))[0]

    def string():
        size = read("Q")
        assert size < len(header), "unreasonable metadata string"
        return stream.read(size).decode()

    assert stream.read(4) == b"GGUF" and read("I") == 3
    tensors, keys = read("Q"), read("Q")
    assert tensors == 1 and keys == 5
    meta = {}
    for _ in range(keys):
        key = string()
        kind = read("I")
        assert kind in (6, 8)
        meta[key] = read("f") if kind == 6 else string()
    assert meta["general.architecture"] == "strata-ple" and meta["strata.ple.format"] == "f8_e4m3"
    assert string() == "per_layer_token_embd.weight" and read("I") == 2
    width, rows, kind, offset = read("Q"), read("Q"), read("I"), read("Q")
    assert (width, rows, kind, offset) == (160, 320001536, 24, 0)
    return (stream.tell() + 31) // 32 * 32, rows, meta["strata.ple.scale"]


def reference(raw, scale):
    values = []
    for byte in raw:
        exponent, mantissa = (byte >> 3) & 15, byte & 7
        if exponent == 15 and mantissa == 7:
            value = 0.0
        elif exponent == 0:
            value = math.ldexp(mantissa / 8.0, -6)
        else:
            value = math.ldexp(1.0 + mantissa / 8.0, exponent - 7)
        value = math.copysign(value, -1.0 if byte & 128 else 1.0)
        values.append(value * scale)
    return struct.pack("<160f", *values)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    parser.add_argument("--pack", type=Path, required=True)
    args = parser.parse_args()
    lib = C.CDLL(str(args.library.resolve()))
    lib.goose_init.argtypes = [C.c_int, C.c_void_p]
    lib.goose_init(0, None)
    lib.e0_ple_supported_scale.argtypes = []
    lib.e0_ple_supported_scale.restype = C.c_float
    lib.e0_ple_decode_row.argtypes = [Bytes, C.c_float, Floats]
    lib.e0_ple_decode_row.restype = C.c_int64
    path = args.pack / "ple-fp8.gguf"
    fd = os.open(path, os.O_RDONLY)
    try:
        data_offset, n_rows, scale = header_info(os.pread(fd, 352, 0))
        assert (data_offset, os.fstat(fd).st_size) == (352, 352 + n_rows * 160)
        assert lib.e0_ple_supported_scale() == scale, "unsupported table scale"
        checks = 0

        def check_row(raw, input_length=160, output_length=160, input_scale=scale, valid=True):
            raw_array = (C.c_uint8 * 160).from_buffer_copy(raw)
            output = (C.c_float * 162)(*([-12345.0] * 162))
            pointer = C.cast(C.byref(output, 4), C.POINTER(C.c_float))
            before = bytes(output)
            rc = lib.e0_ple_decode_row(Bytes(raw_array, input_length), input_scale, Floats(pointer, output_length))
            assert bytes(raw_array) == raw, "input mutated"
            if valid:
                assert rc == 0 and bytes(output)[4:-4] == reference(raw, scale), "FP32 bits differ"
                assert bytes(output)[:4] == before[:4] and bytes(output)[-4:] == before[-4:], "output guard overwritten"
            else:
                assert rc == -1 and bytes(output) == before, "invalid call wrote output"
            return True

        raw_cases = [bytes(range(160)), bytes(range(96, 256))]
        raw_cases += [bytes([b]) * 160 for b in (0, 128, 1, 129, 7, 135, 126, 254, 127, 255)]
        rng = random.Random(910)
        raw_cases += [rng.randbytes(160) for _ in range(32)]
        for raw in raw_cases:
            check_row(raw)
            checks += 1
        for length in (-1, 0, 159, 161, (1 << 63) - 1):
            check_row(raw_cases[0], input_length=length, valid=False)
            checks += 1
        for length in (-1, 0, 159):
            check_row(raw_cases[0], output_length=length, valid=False)
            checks += 1
        for bad in (0.0, -scale, 1.0, float("nan"), float("inf")):
            check_row(raw_cases[0], input_scale=bad, valid=False)
            checks += 1
        rows = [0, 1, n_rows - 1] + [rng.randrange(n_rows) for _ in range(29)]
        for row in rows:
            raw = os.pread(fd, 160, data_offset + row * 160)
            assert len(raw) == 160, "truncated PLE row"
            check_row(raw)
            checks += 1

        def threaded(seed):
            for i in range(256):
                check_row(raw_cases[(i + seed) % len(raw_cases)])
            return True

        with ThreadPoolExecutor(max_workers=8) as pool:
            assert all(pool.map(threaded, range(16)))
        print(json.dumps({"result": "PASS", "checks": checks, "all_FP8_codes": 256,
                          "synthetic_rows": len(raw_cases), "real_rows": len(rows),
                          "real_payload_bytes_read": len(rows) * 160, "concurrent_calls": 4096,
                          "table_scale": scale, "model_loaded": False}))
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
