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

import time

module, kernel = ptr(), ptr()
filename = Path(__file__).resolve().with_name("head.cubin")
call("cuModuleLoad", c.byref(module), str(filename).encode())
call("cuModuleGetFunction", c.byref(kernel), module, b"goose_mmvq_dense_matvec")

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

device_weights, device_activations = upload(weights), upload(packed)
out_bytes = c.c_size_t(rows * 4)
out = allocate(out_bytes.value)
host, mapped = ptr(), u64()
call("cuMemHostAlloc", c.byref(host), out_bytes, 2)
call("cuMemHostGetDevicePointer_v2", c.byref(mapped), host, 0)
stream = ptr()
call("cuStreamCreate", c.byref(stream), 0)
shape_args = (u64(210 * extent), i32(extent), i32(14))
args = {name: (device_weights, device_activations, dest) + shape_args
        for name, dest in (("copy", out), ("mapped", mapped))}
params = {name: (ptr * len(a))(*(c.addressof(x) for x in a)) for name, a in args.items()}

def evaluate(name):
    call("cuLaunchKernel", kernel, rows, 1, 1, 128, 1, 1, 0, stream, params[name], ptr())
    if name == "copy":
        call("cuMemcpyDtoHAsync_v2", host, out, out_bytes, stream)

def result():
    array = np.ctypeslib.as_array((c.c_float * rows).from_address(host.value))
    assert np.isfinite(array).all()
    return array.view(np.uint32).copy()

reference = []
for sample in range(3):
    packed[..., :4].view(np.float16)[:] = rng.uniform(-0.1, 0.1, (extent, 8, 2))
    call("cuMemcpyHtoD_v2", device_activations, ptr(packed.ctypes.data), c.c_size_t(packed.nbytes))
    for arm in ("copy", "mapped"):
        evaluate(arm)
        call("cuStreamSynchronize", stream)
        bits = result()
        if arm == "copy":
            reference = bits
        else:
            n = int(np.count_nonzero(reference != bits))
            print(f"sample={sample} compared_logits={rows} mismatches={n}", flush=True)
            assert n == 0

scores = {name: [] for name in args}
for order in (("copy", "mapped"), ("mapped", "copy")) * 4:
    for name in order:
        evaluate(name)
        call("cuStreamSynchronize", stream)
        start = time.perf_counter()
        for _ in range(32):
            evaluate(name)
            call("cuStreamSynchronize", stream)
        scores[name].append(1000 * (time.perf_counter() - start) / 32)
for name, values in scores.items():
    print(name, "device_to_host_ready_ms", values, "median", median(values), flush=True)
