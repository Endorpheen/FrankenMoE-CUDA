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
host = ptr()
call("cuMemHostAlloc", c.byref(host), out_bytes, 0)
stream, copy_stream = ptr(), ptr()
call("cuStreamCreate", c.byref(stream), 1)
call("cuStreamCreate", c.byref(copy_stream), 1)
events = [ptr(), ptr()]
for event in events:
    call("cuEventCreate", c.byref(event), 2)
shape_args = (u64(210 * extent), i32(extent), i32(14))
half_rows = rows // 2
half_bytes = c.c_size_t(half_rows * 4)
args = [(device_weights, device_activations, out) + shape_args,
        (u64(device_weights.value + half_rows * 210 * extent),
         device_activations, u64(out.value + half_bytes.value)) + shape_args]
params = [(ptr * len(a))(*(c.addressof(x) for x in a)) for a in args]

def launch(packet, count):
    call("cuLaunchKernel", kernel, count, 1, 1, 128, 1, 1, 0, stream, packet, ptr())

def evaluate(name):
    if name == "copy":
        launch(params[0], rows)
        call("cuMemcpyDtoHAsync_v2", host, out, out_bytes, stream)
    else:
        for half, event in enumerate(events):
            launch(params[half], half_rows)
            call("cuEventRecord", event, stream)
            call("cuStreamWaitEvent", copy_stream, event, 0)
            offset = half * half_bytes.value
            call("cuMemcpyDtoHAsync_v2", ptr(host.value + offset),
                 u64(out.value + offset), half_bytes, copy_stream)

def drain(name):
    call("cuStreamSynchronize", stream if name == "copy" else copy_stream)

def result():
    array = np.ctypeslib.as_array((c.c_float * rows).from_address(host.value))
    assert np.isfinite(array).all()
    return array.view(np.uint32).copy()

reference = []
for sample in range(3):
    packed[..., :4].view(np.float16)[:] = rng.uniform(-0.1, 0.1, (extent, 8, 2))
    call("cuMemcpyHtoD_v2", device_activations, ptr(packed.ctypes.data), c.c_size_t(packed.nbytes))
    for arm in ("copy", "split"):
        evaluate(arm)
        drain(arm)
        bits = result()
        if arm == "copy":
            reference = bits
        else:
            n = int(np.count_nonzero(reference != bits))
            print(f"sample={sample} compared_logits={rows} mismatches={n}", flush=True)
            assert n == 0

scores = {name: [] for name in ("copy", "split")}
for order in (("copy", "split"), ("split", "copy")) * 4:
    for name in order:
        evaluate(name)
        drain(name)
        start = time.perf_counter()
        for _ in range(32):
            evaluate(name)
            drain(name)
        scores[name].append(1000 * (time.perf_counter() - start) / 32)
for name, values in scores.items():
    print(name, "device_to_host_ready_ms", values, "median", median(values), flush=True)
