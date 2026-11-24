import ctypes as c
from pathlib import Path
from statistics import median

import numpy as np

cuda = c.CDLL("libcuda.so.1")
ptr, u64, i32 = c.c_void_p, c.c_uint64, c.c_int

def call(name, *args):
    status = getattr(cuda, name)(*args)
    if status:
        raise RuntimeError(f"{name}: {status}")

call("cuInit", 0)
device, ctx = i32(), ptr()
call("cuDeviceGet", c.byref(device), 0)
call("cuDevicePrimaryCtxRetain", c.byref(ctx), device)
call("cuCtxSetCurrent", ctx)
kernels = {}
for name, threads, rows_per_cta in (("control", 128, 1), ("candidate", 256, 2)):
    module, function = ptr(), ptr()
    image = Path(__file__).resolve().with_name(f"{name}.cubin")
    call("cuModuleLoad", c.byref(module), str(image).encode())
    call("cuModuleGetFunction", c.byref(function), module, b"goose_mmvq_dense_matvec")
    kernels[name] = (function, threads, rows_per_cta)


def allocate(size):
    address = u64()
    call("cuMemAlloc_v2", c.byref(address), c.c_size_t(size))
    return address

def upload(array):
    address = allocate(array.nbytes)
    call("cuMemcpyHtoD_v2", address, ptr(array.ctypes.data), c.c_size_t(array.nbytes))
    return address

start, end = ptr(), ptr()
call("cuEventCreate", c.byref(start), 0)
call("cuEventCreate", c.byref(end), 0)
rng = np.random.default_rng(1865)
rows, extent = 248320, 8
weights = rng.integers(0, 256, (rows, extent, 210), dtype=np.uint8)
weights[..., 208:].view(np.float16)[:] = rng.uniform(0.0, 0.02, (rows, extent, 1))
packed = rng.integers(0, 256, (extent, 8, 36), dtype=np.uint8)
packed[..., :4].view(np.float16)[:] = rng.uniform(-0.1, 0.1, (extent, 8, 2))
out = allocate(rows * 4)
args = (upload(weights), upload(packed), out, u64(210 * extent), i32(extent), i32(14))
params = (ptr * len(args))(*(c.addressof(a) for a in args))

def launch(name):
    function, threads, rows_per_cta = kernels[name]
    call("cuLaunchKernel", function, rows // rows_per_cta, 1, 1, threads, 1, 1, 0, ptr(), params, ptr())

replies = []
for name in kernels:
    call("cuMemsetD8_v2", out, 255, c.c_size_t(rows * 4))
    launch(name)
    values = np.empty(rows, dtype=np.float32)
    call("cuMemcpyDtoH_v2", ptr(values.ctypes.data), out, c.c_size_t(values.nbytes))
    assert np.isfinite(values).all(), name
    replies.append(values.view(np.uint32))
mismatches = int(np.count_nonzero(replies[0] != replies[1]))
print(f"rows={rows} extent={extent} mismatches={mismatches}", flush=True)
assert mismatches == 0
samples = {name: [] for name in kernels}
for order in (("control", "candidate"), ("candidate", "control")) * 4:
    for name in order:
        call("cuEventRecord", start, ptr())
        for _ in range(32):
            launch(name)
        call("cuEventRecord", end, ptr())
        call("cuEventSynchronize", end)
        elapsed = c.c_float()
        call("cuEventElapsedTime", c.byref(elapsed), start, end)
        samples[name].append(elapsed.value / 32)
for name, values in samples.items():
    print(name, "ms/launch", values, "median", median(values), flush=True)
