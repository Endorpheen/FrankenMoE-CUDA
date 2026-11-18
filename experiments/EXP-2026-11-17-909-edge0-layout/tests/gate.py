#!/usr/bin/env python3
"""Independent format oracle; never loads a model or maps a weight file."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes as C
import json
from pathlib import Path
import struct

I = C.c_int64
U = C.c_uint8


class Slice(C.Structure):
    _fields_ = [("data", C.POINTER(I)), ("len", I)]


class Bytes(C.Structure):
    _fields_ = [("data", C.POINTER(U)), ("len", I)]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    parser.add_argument("--pack", type=Path, required=True)
    args = parser.parse_args()
    lib = C.CDLL(str(args.library.resolve()))
    lib.goose_init.argtypes = [C.c_int, C.c_void_p]
    lib.goose_init(0, None)
    sizes = {"e0_expert_desc": 3, "e0_du_desc": 4, "e0_ple_desc": 1,
             "e0_label_desc": 2, "e0_scale_desc": 1}
    for name, count in sizes.items():
        fn = getattr(lib, name)
        fn.argtypes = [I] * count + [Slice]
        fn.restype = I
    lib.e0_route_group.argtypes = [Bytes, I, I]
    lib.e0_route_group.restype = I
    checks = 0

    def check(condition, label):
        nonlocal checks
        checks += 1
        if not condition:
            raise AssertionError(label)

    def call(name, values, expected=None, length=8):
        storage = (I * 10)(*([-777] * 10))
        pointer = C.cast(C.byref(storage, 8), C.POINTER(I))
        rc = getattr(lib, name)(*values, Slice(pointer, length))
        if expected is None:
            check(rc == -1 and list(storage) == [-777] * 10, (name, values, "invalid input writes"))
        else:
            check(rc == 0 and list(storage)[1:9] == expected and storage[0] == storage[9] == -777,
                  (name, values, list(storage), expected))
        return list(storage)[1:9]

    # The oracle derives sizes from block shapes, not the module's byte constants.
    gate_size = (1280 // 256) * 110 * 640
    down_size = (640 // 32) * 18 * 1280
    record = 2 * gate_size + down_size
    native = [line.split() for line in (args.pack / "native_experts.txt").read_text().splitlines()
              if line and not line.startswith("#")]
    for layer in range(48):
        check(int(native[layer][3]) == layer * 512 * record and int(native[layer][4]) == record, "native table")
        for expert in range(512):
            start = (layer * 512 + expert) * record
            for matrix in range(3):
                n0, n1, kind, offset = ((1280, 640, 11, matrix * gate_size)
                                        if matrix < 2 else (640, 1280, 20, 2 * gate_size))
                row_bytes = n0 // (256 if kind == 11 else 32) * (110 if kind == 11 else 18)
                result = call("e0_expert_desc", (layer, expert, matrix),
                              [0, start + offset, row_bytes * n1, kind, n0, n1, row_bytes, record])
                check(result[1] + result[2] <= (args.pack / "experts.bin").stat().st_size, "expert range")
            call("e0_label_desc", (layer, expert), [4, (layer * 512 + expert) * 4, 4, 26, 1, 1, 4, 512 * 4])
        call("e0_scale_desc", (layer,), [100 + layer, 0, 1280 * 4, 0, 1280, 1, 5120, 5120])
        for bits in (4, 8):
            block_bytes = 2 + (16 if bits == 4 else 32)
            group_bytes = (2560 * 1280 // 32) * block_bytes
            for group in range(8):
                for matrix in range(2):
                    n0, n1 = (2560, 1280) if matrix == 0 else (1280, 2560)
                    expected = [1 if bits == 4 else 2,
                                layer * 16 * group_bytes + matrix * 8 * group_bytes + group * group_bytes,
                                group_bytes, 2 if bits == 4 else 8, n0, n1, n0 // 32 * block_bytes,
                                group_bytes * 16]
                    result = call("e0_du_desc", (layer, group, bits, matrix), expected)
                    check(result[1] + result[2] <= (args.pack / f"latent_dense_q{bits}.bin").stat().st_size, "DU range")
    raw = (args.pack / "labels.bin").read_bytes()
    labels = (U * len(raw)).from_buffer_copy(raw)
    for layer in range(48):
        for expert in range(512):
            expected = struct.unpack_from("<i", raw, (layer * 512 + expert) * 4)[0]
            check(0 <= expected < 8 and lib.e0_route_group(Bytes(labels, len(raw)), layer, expert) == expected, "group labels")
    check(bytes(labels) == raw, "labels mutated")
    for index in range(4):
        labels[index] = 255
        check(lib.e0_route_group(Bytes(labels, len(raw)), 0, 0) == -1, "invalid label")
        labels[index] = raw[index]
    for row in (0, 1, 20000003, 160000374, 320001535):
        result = call("e0_ple_desc", (row,), [3, 352 + row * 160, 160, 24, 160, 1, 160, 160])
        check(result[1] + result[2] <= (args.pack / "ple-fp8.gguf").stat().st_size, "PLE range")
    for name, good in {"e0_expert_desc": [0, 0, 0], "e0_du_desc": [0, 0, 4, 0],
                       "e0_ple_desc": [0], "e0_label_desc": [0, 0], "e0_scale_desc": [0]}.items():
        for length in (0, 1, 7):
            call(name, good, length=length)
        for position in range(len(good)):
            for bad in (-1, -(1 << 63), (1 << 63) - 1):
                values = good.copy()
                values[position] = bad
                call(name, values)
    for name, values in [("e0_expert_desc", [48, 0, 0]), ("e0_expert_desc", [0, 512, 0]),
                         ("e0_expert_desc", [0, 0, 3]), ("e0_du_desc", [0, 8, 4, 0]),
                         ("e0_du_desc", [0, 0, 16, 0]), ("e0_du_desc", [0, 0, 4, 2]),
                         ("e0_ple_desc", [320001536]), ("e0_label_desc", [0, 512]), ("e0_scale_desc", [48])]:
        call(name, values)
    check(lib.e0_route_group(Bytes(labels, len(raw) - 1), 0, 0) == -1, "short labels")

    def threaded(seed):
        for i in range(512):
            layer, expert = (seed + i) % 48, (i * 17) % 512
            out = (I * 8)()
            if lib.e0_expert_desc(layer, expert, 2, Slice(out, 8)) != 0:
                return False
            if out[1] != (layer * 512 + expert) * record + 2 * gate_size:
                return False
        return True

    with ThreadPoolExecutor(max_workers=8) as pool:
        check(all(pool.map(threaded, range(16))), "independent concurrent calls")
    print(json.dumps({"checks": checks, "expert_matrices": 48 * 512 * 3, "DU_matrices": 48 * 8 * 2 * 2,
                      "real_labels": len(raw) // 4, "thread_calls": 16 * 512,
                      "model_loaded": False, "result": "PASS"}))


if __name__ == "__main__":
    main()
