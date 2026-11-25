"""Time and compare Inkling batched Flash partials on fixed Q4_0 inputs."""
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

rng = random.Random(1940)
MAX_QUERIES = 128
keys_data = b"".join(struct.pack("<e", 0.016) + rng.randbytes(16) for _ in range(MAX_KEYS * 32))
values_data = b"".join(struct.pack("<e", 0.019) + rng.randbytes(16) for _ in range(MAX_KEYS * 32))
try:
    q = alloc(MAX_QUERIES * HEADS * 128 * 2, b"".join(
        struct.pack("<e", rng.uniform(-0.3, 0.3)) for _ in range(MAX_QUERIES * HEADS * 128)))
    k = alloc(MAX_KEYS * ROW, keys_data)
    v = alloc(MAX_KEYS * ROW, values_data)
    relative = alloc(MAX_QUERIES * HEADS * 16 * 4, f32_values(MAX_QUERIES * HEADS * 16, 0.2))
    bias = alloc(1024 * 16 * 4, f32_values(1024 * 16, 0.3))
    sinks = alloc(MAX_QUERIES * HEADS * 4, f32_values(MAX_QUERIES * HEADS, 0.4))
    positions = alloc(MAX_KEYS * 8, struct.pack(f"<{MAX_KEYS}q", *range(MAX_KEYS)))
    query = alloc(MAX_QUERIES * 8)
    partial = alloc(MAX_QUERIES * HEADS * (MAX_KEYS // PART) * 128 * 4)
    meta = alloc(MAX_QUERIES * HEADS * (MAX_KEYS // PART) * 2 * 4)
    staged_k = alloc(MAX_KEYS * 1024 * 2)
    staged_v = alloc(MAX_KEYS * 1024 * 2)
    common = [q, k, v, relative, bias, sinks, positions, query, partial, meta]
    kernels = {
        "original": module_kernel(base / "flash-native.cubin", "goose_i2_flash_mma_prefill"),
        "staged": module_kernel(base / "flash-native.cubin", "goose_i2_flash_mma_prefill_f16"),
    }
    unpack = module_kernel(base / "flash-native.cubin", "goose_i2_prefill_unpack_kv")
    begin, end = event(), event()
    rows = []
    for keys in (1024, 4096, MAX_KEYS):
        parts = (keys + PART - 1) // PART
        tasks = U(keys * 512)
        unpack_args = (P * 5)(*(addressof(a) for a in (k, v, staged_k, staged_v, tasks)))
        def unpack_once():
            call("cuLaunchKernel", unpack, U((tasks.value + 255) // 256), U(1), U(1),
                 U(256), U(1), U(1), U(0), P(), unpack_args, P())
        unpack_once()
        call("cuEventRecord", begin, P())
        for _ in range(32):
            unpack_once()
        call("cuEventRecord", end, P())
        call("cuEventSynchronize", end)
        timing = F()
        call("cuEventElapsedTime", byref(timing), begin, end)
        unpack_us = timing.value * 1000 / 32
        for queries in (16, MAX_QUERIES):
            pos = struct.pack(f"<{queries}q", *range(keys-queries,keys))
            call("cuMemcpyHtoD_v2", query, c_char_p(pos), B(len(pos)))
            cfg = [L(keys), L(queries), L(parts), L(1024 if keys == 1024 else 0),
                   F(float.fromhex("0x1.6a09e6p-4")), I(1)]
            args = (P * 16)(*(addressof(a) for a in common + cfg))
            staged_args = (P * 16)(*(addressof(a) for a in [q, staged_k, staged_v, *common[3:], *cfg]))
            def launch(kernel):
                packet = staged_args if kernel == kernels["staged"] else args
                call("cuLaunchKernel", kernel, U((queries+15)//16), U(HEADS), U(parts),
                     U(128), U(1), U(1), U(0), P(), packet, P())
            digests = {}
            for name, kernel in kernels.items():
                launch(kernel)
                call("cuCtxSynchronize")
                digests[name] = hashlib.sha256(
                    retrieve(partial, queries * HEADS * parts * 128 * 4) +
                    retrieve(meta, queries * HEADS * parts * 2 * 4)
                ).hexdigest()
            assert len(set(digests.values())) == 1, (keys, queries, digests)
            data = {name: [] for name in kernels}
            for order in (kernels, reversed(kernels), kernels, reversed(kernels)):
                for name in order:
                    launch(kernels[name])
                    call("cuEventRecord", begin, P())
                    for _ in range(12):
                        launch(kernels[name])
                    call("cuEventRecord", end, P())
                    call("cuEventSynchronize", end)
                    timing = F()
                    call("cuEventElapsedTime", byref(timing), begin, end)
                    data[name].append(timing.value * 1000 / 12)
            rows.append({"keys": keys, "queries": queries, "unpack_us":unpack_us, "exact_sha256":digests["original"],
                         "samples_us":data,"median_us":{name:statistics.median(samples)
                                                        for name,samples in data.items()}})
    print(json.dumps(rows, indent=2))
finally:
    for handle in events:
        call("cuEventDestroy_v2", handle)
    for module in modules:
        call("cuModuleUnload", module)
    for ptr in allocations:
        call("cuMemFree_v2", ptr)
    call("cuCtxDestroy_v2", context)
