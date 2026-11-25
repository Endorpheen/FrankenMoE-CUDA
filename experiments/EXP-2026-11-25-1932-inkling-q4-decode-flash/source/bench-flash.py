"""Time and compare Inkling single-query Flash partials on fixed Q4_0 inputs."""
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

MAX_KEYS = 32768
PART = 2048
HEADS = 24
ROW = 576
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

rng = random.Random(1932)
keys_data = b"".join(struct.pack("<e", 0.016) + rng.randbytes(16) for _ in range(MAX_KEYS * 32))
values_data = b"".join(struct.pack("<e", 0.019) + rng.randbytes(16) for _ in range(MAX_KEYS * 32))
try:
    q = alloc(HEADS * 128 * 2, b"".join(struct.pack("<e", rng.uniform(-0.3, 0.3)) for _ in range(HEADS * 128)))
    k = alloc(MAX_KEYS * ROW, keys_data)
    v = alloc(MAX_KEYS * ROW, values_data)
    relative = alloc(HEADS * 16 * 4, f32_values(HEADS * 16, 0.2))
    bias = alloc(1024 * 16 * 4, f32_values(1024 * 16, 0.3))
    sinks = alloc(HEADS * 4, f32_values(HEADS, 0.4))
    positions = alloc(MAX_KEYS * 8, struct.pack(f"<{MAX_KEYS}q", *range(MAX_KEYS)))
    query = alloc(8)
    partial = alloc(HEADS * (MAX_KEYS // PART) * 128 * 4)
    meta = alloc(HEADS * (MAX_KEYS // PART) * 2 * 4)
    common = [q, k, v, relative, bias, sinks, positions, query, partial, meta]
    kernels = {
        "accepted": module_kernel(base / "flash-native.cubin", "goose_i2_flash_mma"),
        "warp232": module_kernel(base.parent / "inkling-decode-warp-softmax/flash-native.cubin", "goose_i2_flash_mma_decode"),
        **{arm: module_kernel(base / f"{arm}.cubin", "goose_i2_flash_mma_decode")
           for arm in ("sum", "shape", "shape-sum", "shape-qk", "shape-pv")},
    }
    begin, end = event(), event()
    results = {}
    for keys in (1024, 2048, 4096, 8192, MAX_KEYS):
        parts = (keys + PART - 1) // PART
        call("cuMemcpyHtoD_v2", query, c_char_p(struct.pack("<q", keys - 1)), B(8))
        cfg = [L(keys), L(1), L(parts), L(1024 if keys == 1024 else 0), F(float.fromhex("0x1.6a09e6p-4")), I(3)]
        args = (P * 16)(*(addressof(a) for a in common + cfg))
        def launch(kernel):
            call("cuLaunchKernel", kernel, U(1), U(HEADS), U(parts), U(128), U(1), U(1), U(0), P(), args, P())
        order = list(kernels)
        hashes = {}
        measurements = {arm: [] for arm in order}
        for arm, kernel in kernels.items():
            launch(kernel)
            call("cuCtxSynchronize")
            hashes[arm] = hashlib.sha256(
                retrieve(partial, HEADS * parts * 128 * 4) + retrieve(meta, HEADS * parts * 2 * 4)
            ).hexdigest()
        assert len(set(hashes.values())) == 1, (keys, hashes)
        for repeat in range(4):
            for arm in (order if repeat % 2 == 0 else reversed(order)):
                kernel = kernels[arm]
                for _ in range(8):
                    launch(kernel)
                call("cuEventRecord", begin, P())
                for _ in range(80):
                    launch(kernel)
                call("cuEventRecord", end, P())
                call("cuEventSynchronize", end)
                elapsed = F()
                call("cuEventElapsedTime", byref(elapsed), begin, end)
                measurements[arm].append(elapsed.value * 1000 / 80)
        results[keys] = {"parts": parts, "partial_and_meta_sha256": hashes["accepted"],
                         "median_us": {k: statistics.median(v) for k, v in measurements.items()},
                         "samples_us": measurements}
    print(json.dumps(results, indent=2))
finally:
    for handle in events:
        call("cuEventDestroy_v2", handle)
    for module in modules:
        call("cuModuleUnload", module)
    for ptr in allocations:
        call("cuMemFree_v2", ptr)
    call("cuCtxDestroy_v2", context)
