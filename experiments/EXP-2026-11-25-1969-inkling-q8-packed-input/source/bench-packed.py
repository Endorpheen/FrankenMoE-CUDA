"""Measure bit-exact Q8_0 projection with and without an interleaved activation tile."""
from array import array
from ctypes import addressof, byref, CDLL, c_char_p, c_float, c_int, c_longlong, c_size_t, c_uint, c_ulonglong, c_void_p, string_at
import hashlib
import json
from pathlib import Path
import random
import statistics
import struct

CU = CDLL("libcuda.so.1")
U, P, I, L, D, B, F = c_uint, c_void_p, c_int, c_longlong, c_ulonglong, c_size_t, c_float
def call(name, *args):
    result = getattr(CU, name)(*args)
    if result != 0:
        raise RuntimeError(f"{name} returned {result}")

base = Path(__file__).resolve().parent
call("cuInit", U(0))
device = I()
call("cuDeviceGet", byref(device), I(0))
context = P()
call("cuCtxCreate_v2", byref(context), U(0), device)
allocations = []
modules = []
events = []

def alloc(size, data=None):
    ptr = D()
    call("cuMemAlloc_v2", byref(ptr), B(size))
    allocations.append(ptr)
    if data is not None:
        assert len(data) == size
        call("cuMemcpyHtoD_v2", ptr, c_char_p(data), B(size))
    return ptr

def retrieve(ptr, size):
    from ctypes import create_string_buffer
    buf = create_string_buffer(size)
    call("cuMemcpyDtoH_v2", buf, ptr, B(size))
    return string_at(buf, size)

def event():
    handle = P()
    call("cuEventCreate", byref(handle), U(0))
    events.append(handle)
    return handle

def module_kernel(path, entry):
    module = P()
    call("cuModuleLoad", byref(module), c_char_p(str(path).encode()))
    modules.append(module)
    kernel = P()
    call("cuModuleGetFunction", byref(kernel), module, c_char_p(entry.encode()))
    return kernel


COUNT = 4096
TILE = 8
rng = random.Random(1969)


def arguments(values):
    return (P * len(values))(*(addressof(value) for value in values))


def check_call(kernel, x, y, block, values):
    call("cuLaunchKernel", kernel, U(x), U(y), U(1), U(block), U(1), U(1), U(0), P(), values, P())


def benchmark(width, rows, kernels, start, end):
    first_allocation = len(allocations)
    try:
        stride = width // 32 * 34
        data = bytearray(rng.randbytes(rows * stride))
        scales = [struct.pack("<e", rng.uniform(0.001, 0.019)) for _ in range(128)]
        for index, offset in enumerate(range(0, len(data), 34)):
            data[offset:offset + 2] = scales[index % len(scales)]
        weights = alloc(len(data), bytes(data))
        inputs = alloc(width * COUNT * 4, array("f", (rng.uniform(-1, 1) for _ in range(width * COUNT))).tobytes())
        output = alloc(rows * COUNT * 4)
        packed = alloc(width * COUNT * 4)
        jobs = alloc(COUNT * 16, b"".join(struct.pack("<QQ", i * width * 4, i * rows * 4) for i in range(COUNT)))
        dense_values = [weights, inputs, output, jobs, D(stride), I(width), I(rows), I(COUNT), I(0), I(2), I(TILE)]
        pack_values = [inputs, jobs, packed, U(width)]
        packed_values = [weights, packed, output, jobs, D(stride), U(width)]
        dense_args, pack_args, packed_args = map(arguments,(dense_values,pack_values,packed_values))

        def pack():
            check_call(kernels["pack"], (width + 127) // 128, COUNT // TILE, 128, pack_args)

        def dense():
            check_call(kernels["original"], rows // 4, COUNT // TILE, 128, dense_args)

        def packed_proj(warps):
            check_call(kernels[f"packed{warps}"], rows // warps, COUNT // TILE, warps * 32, packed_args)

        def chain(warps):
            pack()
            packed_proj(warps)

        arms = {"original": dense, "packed4_total": lambda: chain(4),
                "packed8_total": lambda: chain(8), "pack_only": pack,
                "packed4_projection": lambda: packed_proj(4), "packed8_projection": lambda: packed_proj(8)}
        fingerprints = {}
        for name in ("original","packed4_total","packed8_total"):
            arms[name]()
            call("cuCtxSynchronize")
            fingerprints[name] = hashlib.sha256(retrieve(output, rows * COUNT * 4)).hexdigest()
        assert len(set(fingerprints.values())) == 1, (width, rows, fingerprints)
        timings = {name: [] for name in arms}
        for ordering in (list(arms),list(reversed(arms)),list(arms),list(reversed(arms))):
            for name in ordering:
                launch = arms[name]
                launch()
                call("cuEventRecord", start, P())
                for _ in range(8):
                    launch()
                call("cuEventRecord", end, P())
                call("cuEventSynchronize", end)
                elapsed = F()
                call("cuEventElapsedTime", byref(elapsed), start, end)
                timings[name].append(elapsed.value / 8)
        return {"channels": width, "output_rows": rows, "input_rows": COUNT, "exact_sha256": fingerprints["original"],
                "samples_ms": timings,"median_ms": {name: statistics.median(values) for name, values in timings.items()}}
    finally:
        while len(allocations) > first_allocation:
            call("cuMemFree_v2", allocations.pop())


try:
    kernels = {"original": module_kernel(base/"control.cubin", "goose_i2_matrix_q8_0")}
    for name, entry in (("pack", "i2_pack_q8_input8"), ("packed4", "i2_q8_packed4"),
                        ("packed8", "i2_q8_packed8")):
        kernels[name] = module_kernel(base/"q8-packed.cubin", entry)
    start, end = event(), event()
    results = [benchmark(width, rows, kernels, start, end)
               for width, rows in ((2304,3072),(2304,2304),(3072,2304),(2304,1024))]
    print(json.dumps(results, indent=2))
finally:
    for handle in events:
        call("cuEventDestroy_v2", handle)
    for module in modules:
        call("cuModuleUnload", module)
    for ptr in allocations:
        call("cuMemFree_v2", ptr)
    call("cuCtxDestroy_v2", context)
