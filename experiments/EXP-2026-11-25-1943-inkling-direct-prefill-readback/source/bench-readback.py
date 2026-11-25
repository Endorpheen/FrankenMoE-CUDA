"""Measure synchronous CUDA readback to pageable and bounded pinned buffers."""
from ctypes import byref, CDLL, c_int, c_size_t, c_uint, c_ulonglong, c_void_p, create_string_buffer, memmove, string_at
import hashlib
import json
from statistics import median
from time import perf_counter

cuda = CDLL("libcuda.so.1")
def call(name, *args):
    error = getattr(cuda, name)(*args)
    if error:
        raise RuntimeError(f"{name}: {error}")

device = c_int()
context = c_void_p()
gpu = c_ulonglong()
pinned = c_void_p()
maximum = 36 * 1024 * 1024
call("cuInit", c_uint(0))
call("cuDeviceGet", byref(device), c_int(0))
call("cuCtxCreate_v2", byref(context), c_uint(0), device)
try:
    call("cuMemAlloc_v2", byref(gpu), c_size_t(maximum))
    call("cuMemsetD8_v2", gpu, c_uint(0x5A), c_size_t(maximum))
    call("cuMemHostAlloc", byref(pinned), c_size_t(maximum), c_uint(0))
    output = create_string_buffer(maximum)
    records = []
    for mib in (9, 18, 36):
        size = mib * 1024 * 1024
        def readback(arm):
            dest = output if arm == "pageable" else pinned
            call("cuMemcpyDtoH_v2", dest, gpu, c_size_t(size))
            if arm == "pinned_plus_copy":
                memmove(output, pinned, size)
            return dest if arm != "pinned_plus_copy" else output
        samples = {"pageable": [], "pinned_plus_copy": [], "direct_pinned": []}
        digests = [hashlib.sha256(string_at(readback(arm), size)).hexdigest() for arm in samples]
        assert len(set(digests)) == 1
        for repeat in range(4):
            for arm in (samples if repeat % 2 == 0 else reversed(samples)):
                readback(arm)
                start = perf_counter()
                for _ in range(20):
                    readback(arm)
                samples[arm].append((perf_counter() - start) * 1000 / 20)
        records.append({"mib":mib,"sha256":digests[0],"samples_ms":samples,
                        "median_ms":{key:median(value) for key,value in samples.items()}})
    print(json.dumps(records, indent=2))
finally:
    if pinned:
        call("cuMemFreeHost", pinned)
    if gpu:
        call("cuMemFree_v2", gpu)
    call("cuCtxDestroy_v2", context)
