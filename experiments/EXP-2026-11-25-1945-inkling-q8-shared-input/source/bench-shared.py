"""Compare Inkling Q8_0 input reuse across CTA warps on fixed data."""
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

def f32_values(count, scale):
    return b"".join(struct.pack("<f", rng.uniform(-scale, scale)) for _ in range(count))

rng = random.Random(1938)
rows = 3072
max_jobs = 512
try:
    entries = {
        "original": ("goose_i2_matrix_q8_0", 4),
        **{f"shared{n}": (f"goose_i2_matrix_q8_shared{n}", n) for n in (4, 8, 16)},
        **{f"wide{n}": (f"goose_i2_matrix_q8_shared{n}_wide", n) for n in (8, 16)},
    }
    kernels = {name: module_kernel(base / "matrix.cubin", symbol)
               for name, (symbol, _) in entries.items()}
    start, end = event(), event()
    results = {}
    for width in (2304, 3072):
        stride = width // 32 * 34
        data = bytearray(rng.randbytes(rows * stride))
        scale = struct.pack("<e", 0.007)
        for offset in range(0, len(data), 34):
            data[offset:offset + 2] = scale
        weight = alloc(len(data), bytes(data))
        inputs = alloc(width * max_jobs * 4, f32_values(width * max_jobs, 0.5))
        output = alloc(rows * max_jobs * 4)
        jobs = alloc(max_jobs * 16, b"".join(struct.pack("<QQ", i * width * 4, i * rows * 4)
                                             for i in range(max_jobs)))
        results[width] = {}
        for count in (16, 128, 512):
            tile = 1 if count == 1 else 8
            cfg = [weight, inputs, output, jobs, D(stride), I(width), I(rows), I(count), I(0), I(2), I(tile)]
            args = (P * 11)(*(addressof(a) for a in cfg))
            def launch(arm):
                warps = entries[arm][1]
                call("cuLaunchKernel", kernels[arm], U((rows + warps - 1) // warps), U(count // tile), U(1),
                     U(32 * warps), U(1), U(1), U(0), P(), args, P())
            active = kernels
            exact = {}
            for arm, kernel in active.items():
                launch(arm)
                call("cuCtxSynchronize")
                exact[arm] = hashlib.sha256(retrieve(output, rows * count * 4)).hexdigest()
            assert len(set(exact.values())) == 1, (width, count, exact)
            samples = {arm: [] for arm in active}
            iterations = 100 if count < 128 else 25
            for arm_order in (active, reversed(active), active, reversed(active)):
                for arm in arm_order:
                    for _ in range(4):
                        launch(arm)
                    call("cuEventRecord", start, P())
                    for _ in range(iterations):
                        launch(arm)
                    call("cuEventRecord", end, P())
                    call("cuEventSynchronize", end)
                    timing = F()
                    call("cuEventElapsedTime", byref(timing), start, end)
                    samples[arm].append(timing.value * 1000 / iterations)
            results[width][count] = {"output_sha256": exact["original"],
                "samples_us": samples, "median_us": {arm: statistics.median(t) for arm, t in samples.items()}}
    print(json.dumps(results, indent=2))
finally:
    for handle in events:
        call("cuEventDestroy_v2", handle)
    for module in modules:
        call("cuModuleUnload", module)
    for ptr in allocations:
        call("cuMemFree_v2", ptr)
    call("cuCtxDestroy_v2", context)
