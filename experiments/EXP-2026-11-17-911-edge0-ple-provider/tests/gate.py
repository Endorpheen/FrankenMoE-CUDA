#!/usr/bin/env python3
"""Provider I/O, atomic output, syscall-fault and concurrency gate."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import ctypes as C
import importlib.util
import json
import os
from pathlib import Path

E = Path(__file__).resolve().parents[1]
oracle_path = E.parent / "EXP-2026-11-17-910-edge0-ple-fp8/tests/gate.py"
spec = importlib.util.spec_from_file_location("fp8_oracle", oracle_path)
oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)


class Indices(C.Structure):
    _fields_ = [("data", C.POINTER(C.c_int64)), ("len", C.c_int64)]


def setup(path):
    lib = C.CDLL(str(path.resolve()))
    lib.goose_init.argtypes = [C.c_int, C.c_void_p]
    lib.goose_init(0, None)
    lib.e0_ple_read_rows.argtypes = [C.c_int64, Indices, C.c_float, oracle.Bytes, oracle.Floats, Indices]
    lib.e0_ple_read_rows.restype = C.c_int64
    return lib


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("library", type=Path)
    parser.add_argument("--pack", type=Path, required=True)
    parser.add_argument("--fault-library", type=Path)
    args = parser.parse_args()
    normal = setup(args.library)
    faults = setup(args.fault_library or args.library.with_name("faults.so"))
    faults.e0_io_mode.argtypes = [C.c_int]
    faults.e0_io_calls.restype = C.c_int
    path = args.pack / "ple-fp8.gguf"
    fd = os.open(path, os.O_RDONLY)
    try:
        offset, n_rows, scale = oracle.header_info(os.pread(fd, 352, 0))
        os.lseek(fd, 123, os.SEEK_SET)
        cases = 0

        def call(lib, rows, expected=None, row_length=None, scratch_length=None,
                 output_length=None, descriptor_length=8, supplied_fd=fd, supplied_scale=scale):
            indices = (C.c_int64 * max(len(rows), 1))(*rows)
            scratch = (C.c_uint8 * (16 * 160 + 2))(*([0xA5] * (16 * 160 + 2)))
            output = (C.c_float * (16 * 160 + 2))(*([-12345.0] * (16 * 160 + 2)))
            descriptor = (C.c_int64 * 10)(*([-777] * 10))
            before = bytes(output)
            raw_ptr = C.cast(C.byref(scratch, 1), C.POINTER(C.c_uint8))
            out_ptr = C.cast(C.byref(output, 4), C.POINTER(C.c_float))
            desc_ptr = C.cast(C.byref(descriptor, 8), C.POINTER(C.c_int64))
            rc = lib.e0_ple_read_rows(supplied_fd, Indices(indices, len(rows) if row_length is None else row_length),
                                      supplied_scale,
                                      oracle.Bytes(raw_ptr, 16 * 160 if scratch_length is None else scratch_length),
                                      oracle.Floats(out_ptr, 16 * 160 if output_length is None else output_length),
                                      Indices(desc_ptr, descriptor_length))
            if expected is None:
                assert rc == len(rows), ("return", rc, rows)
                want = b"".join(oracle.reference(os.pread(fd, 160, offset + row * 160), scale) for row in rows)
                assert bytes(output)[4:4 + len(want)] == want, "row output mismatch"
                assert bytes(output)[4 + len(want):] == before[4 + len(want):], "unused output written"
            else:
                assert rc == expected, ("wrong failure", rc, expected)
                assert bytes(output) == before, "failure committed partial output"
            assert scratch[0] == scratch[-1] == 0xA5, "scratch guard overwritten"
            assert descriptor[0] == descriptor[-1] == -777, "descriptor guard overwritten"
            assert bytes(output)[:4] == before[:4] and bytes(output)[-4:] == before[-4:], "output guard overwritten"
            assert list(indices)[:len(rows)] == rows, "indices mutated"
            assert os.lseek(fd, 0, os.SEEK_CUR) == 123, "shared file cursor changed"

        batches = [[], [0], [n_rows - 1], [1, 0, 1, n_rows - 1],
                   [n_rows - 1 - i * 7919 for i in range(16)]]
        for rows in batches:
            call(normal, rows)
            cases += 1
        for params in [dict(rows=[-1]), dict(rows=[n_rows]), dict(rows=[(1 << 63) - 1]), dict(rows=[0, -1])]:
            faults.e0_io_mode(0)
            call(faults, expected=-2, **params)
            assert faults.e0_io_calls() == 0, "invalid row reached I/O"
            cases += 1
        invalid = [dict(row_length=-1), dict(row_length=17), dict(scratch_length=159),
                   dict(output_length=159), dict(supplied_scale=1.0), dict(supplied_scale=float("nan")),
                   dict(supplied_fd=-1), dict(supplied_fd=1 << 31), dict(descriptor_length=0), dict(descriptor_length=7)]
        for params in invalid:
            faults.e0_io_mode(0)
            call(faults, [0], expected=-1, **params)
            assert faults.e0_io_calls() == 0, "invalid arguments reached I/O"
            cases += 1
        for mode in (1, 2):
            faults.e0_io_mode(mode)
            call(faults, batches[-1])
            assert faults.e0_io_calls() > 16, "fault injection was not exercised"
            cases += 1
        for mode, expected in ((3, -5), (4, -4), (5, -3), (6, -3)):
            faults.e0_io_mode(mode)
            call(faults, [0, 1], expected=expected)
            if mode == 3:
                assert faults.e0_io_calls() == 192, "unbounded or changed retry policy"
            cases += 1
        bad_fd = os.open(path, os.O_RDONLY)
        os.close(bad_fd)
        call(normal, [0], supplied_fd=bad_fd, expected=-3)
        cases += 1

        def thread(seed):
            for i in range(32):
                call(normal, [(seed * 997 + i) % n_rows, (seed + i * 17) % n_rows])
            return True

        with ThreadPoolExecutor(max_workers=8) as pool:
            assert all(pool.map(thread, range(16)))
        print(json.dumps({"result": "PASS", "cases": cases, "concurrent_calls": 512,
                          "max_batch_rows": 16, "caller_raw_bytes": 2560,
                          "caller_output_bytes": 10240, "model_loaded": False,
                          "caller_descriptor_bytes": 64,
                          "native_adapter_debt_files": 1}))
    finally:
        os.close(fd)


if __name__ == "__main__":
    main()
