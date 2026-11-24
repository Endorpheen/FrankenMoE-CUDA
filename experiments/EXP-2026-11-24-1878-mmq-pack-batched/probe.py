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
for name, image, fn, warps in (
    ("control", "control", b"goose_mmq_pack_q8", 1),
    ("warp2", "grouped", b"goose_mmq_pack_q8_warp2", 2),
    ("warp4", "grouped", b"goose_mmq_pack_q8_warp4", 4),
):
    path = Path(__file__).resolve().with_name(f"{image}.cubin")
    module, kernel = ptr(), ptr()
    call("cuModuleLoad", c.byref(module), str(path).encode())
    call("cuModuleGetFunction", c.byref(kernel), module, fn)
    kernels[name] = (kernel, warps)


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
rng = np.random.default_rng(1878)
for extent, rows, layout, words, first in (
    (8, 4096, 4, 2, 0), (16, 3584, 4, 2, 512), (8, 4092, 4, 2, 0),
    (16, 512, 4, 2, 0), (2, 4096, 3, 3, 13), (3, 65, 1, 3, 7), (1, 5, 4, 2, 2),
):
    data = rng.standard_normal((rows+first, extent*256), dtype=np.float32)
    data[0, ::2] = -0.0
    data[1, :] = 0.0
    jobs = np.zeros((rows+first, words), dtype=np.uint64)
    rows_in = rng.permutation(rows+first) if words == 3 else np.arange(rows+first)
    jobs[:, 0] = rows_in.astype(np.uint64) * extent * 1024
    size = rows * 288 * extent
    args = (upload(data), upload(jobs), allocate(size), i32(extent), i32(layout), i32(first), i32(words))
    params = (ptr * len(args))(*(c.addressof(a) for a in args))

    def launch(name):
        fn, warps = kernels[name]
        call("cuLaunchKernel", fn, (2*extent+warps-1)//warps, rows, 1,
             warps*32, 1, 1, 0, ptr(), params, ptr())

    reference = None
    for name in kernels:
        call("cuMemsetD8_v2", args[2], 165, c.c_size_t(size))
        launch(name)
        host = np.empty(size, dtype=np.uint8)
        call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), args[2], c.c_size_t(size))
        if reference is None: reference = host
        mismatches = int(np.count_nonzero(host != reference))
        print(f"extent={extent} rows={rows} layout={layout} words={words} first={first} {name} bytes={size} mismatches={mismatches}", flush=True)
        assert mismatches == 0
    samples = {name: [] for name in kernels}
    for order in (tuple(kernels), tuple(reversed(kernels))) * 3:
        for name in order:
            call("cuEventRecord", start, ptr())
            for _ in range(32): launch(name)
            call("cuEventRecord", end, ptr())
            call("cuEventSynchronize", end)
            ms = c.c_float()
            call("cuEventElapsedTime", c.byref(ms), start, end)
            samples[name].append(ms.value*1000/32)
    for name, values in samples.items():
        print(extent, rows, layout, words, first, name, "us/launch", values, "median", median(values), flush=True)
    for address in args[:3]: call("cuMemFree_v2", address)
