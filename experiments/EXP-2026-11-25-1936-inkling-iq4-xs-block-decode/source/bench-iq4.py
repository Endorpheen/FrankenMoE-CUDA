"""Compare Inkling IQ4_XS block and group kernels on fixed data."""
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

rng = random.Random(1936)
width, rows, routes, banks = 2304, 2304, 8, 24
stride = width // 256 * 136
plane = stride * rows
try:
    data = bytearray(rng.randbytes(plane * banks))
    scale = struct.pack("<e", 0.009)
    for offset in range(0, len(data), 136):
        data[offset:offset + 2] = scale
    weights = alloc(len(data), bytes(data))
    del data
    inputs = alloc(width * 4, f32_values(width, 0.5))
    output = alloc(rows * routes * 4)
    job_sets = [
        alloc(routes * 24, b"".join(struct.pack("<QQQ", 0, j * rows * 4, (base + j) * plane)
                                   for j in range(routes)))
        for base in range(0, banks, routes)
    ]
    kernels = {arm: module_kernel(base / "matrix.cubin", entry) for arm, entry in
               (("original", "goose_i2_matrix_iq4_xs"), ("block_header", "goose_i2_matrix_iq4_xs_blocks"))}
    start, end = event(), event()
    results = {}
    for count in (1, 2, 4, 8):
        configurations = [[weights, inputs, output, jobs, D(stride), I(width), I(rows), I(count), I(0), I(3), I(1)]
                          for jobs in job_sets]
        packets = [(P * 11)(*(addressof(a) for a in cfg)) for cfg in configurations]
        def launch(kernel, visit):
            packet = packets[visit % len(packets)]
            call("cuLaunchKernel", kernel, U(rows // 4), U(count), U(1), U(128), U(1), U(1), U(0), P(), packet, P())
        hashes = {}
        for name, kernel in kernels.items():
            digest = hashlib.sha256()
            for visit in range(len(packets)):
                launch(kernel, visit)
                call("cuCtxSynchronize")
                digest.update(retrieve(output, rows * count * 4))
            hashes[name] = digest.hexdigest()
        assert len(set(hashes.values())) == 1, (count, hashes)
        samples = {k: [] for k in kernels}
        for pair in range(4):
            for name in (kernels if pair % 2 == 0 else reversed(kernels)):
                kernel = kernels[name]
                for visit in range(10):
                    launch(kernel, visit)
                call("cuEventRecord", start, P())
                for visit in range(120):
                    launch(kernel, visit)
                call("cuEventRecord", end, P())
                call("cuEventSynchronize", end)
                elapsed = F()
                call("cuEventElapsedTime", byref(elapsed), start, end)
                samples[name].append(elapsed.value * 1000 / 120)
        results[count] = {"sha256": hashes["original"], "median_us": {k: statistics.median(v) for k, v in samples.items()},
                          "samples_us": samples}
    print(json.dumps(results, indent=2))
finally:
    for handle in events:
        call("cuEventDestroy_v2", handle)
    for module in modules:
        call("cuModuleUnload", module)
    for ptr in allocations:
        call("cuMemFree_v2", ptr)
    call("cuCtxDestroy_v2", context)
