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
for name, threads, tile, shared in (("control", 256, 256, 61440), ("narrow", 128, 128, 40960), ("n64", 128, 64, 30720)):
    module, function = ptr(), ptr()
    image = Path(__file__).resolve().with_name(f"{name}.cubin")
    call("cuModuleLoad", c.byref(module), str(image).encode())
    call("cuModuleGetFunction", c.byref(function), module, b"goose_mmq_q4_dense_m128n256_pipeline")
    call("cuFuncSetAttribute", function, 8, shared)
    active = i32()
    call("cuOccupancyMaxActiveBlocksPerMultiprocessor", c.byref(active), function, threads, c.c_size_t(shared))
    print(name, f"max_active_ctas_per_sm={active.value}", flush=True)
    kernels[name] = (function, threads, tile, shared)

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
rng = np.random.default_rng(1864)
cases = [(128, 257, 3, 16), (512, 4096, 8, 0), (2048, 3584, 8, 0),
         (2048, 4096, 8, 0), (4096, 4096, 8, 0), (8192, 4096, 8, 0), (2048, 3584, 16, 128)]
for rows, routes, extent, first in cases:
    weights = rng.integers(0, 16, (rows // 16, extent, 4, 1280), dtype=np.uint8)
    scales = weights[..., 1024:].view(np.float32)
    scales[:] = rng.uniform(-0.1, 0.1, scales.shape)
    activations = rng.integers(0, 256, (routes, extent, 2, 144), dtype=np.uint8)
    coeff = activations[..., :16].view(np.float16)
    coeff[:] = rng.uniform(-0.1, 0.1, coeff.shape)
    jobs = np.zeros((first + routes, 2), np.uint64)
    jobs[first:, 1] = np.arange(routes, dtype=np.uint64) * rows * 4
    size = rows * routes * 4
    addresses = (upload(weights), upload(activations), allocate(size), upload(jobs))
    args = addresses + (u64(extent * 144), i32(extent), i32(first), i32(routes))
    packet = (ptr * len(args))(*(c.addressof(a) for a in args))

    def launch(name):
        function, threads, tile, shared = kernels[name]
        call("cuLaunchKernel", function, rows // 128, (routes + tile - 1) // tile, 1,
             threads, 1, 1, shared, ptr(), packet, ptr())

    output = []
    for name in kernels:
        call("cuMemsetD8_v2", addresses[2], 255, c.c_size_t(size))
        launch(name)
        result = np.empty((routes, rows), dtype=np.float32)
        call("cuMemcpyDtoH_v2", ptr(result.ctypes.data), addresses[2], c.c_size_t(size))
        assert np.isfinite(result).all(), (name, rows, routes, "non-finite or unwritten")
        output.append(result.view(np.uint32))
    differences = sum(int(np.count_nonzero(output[0] != data)) for data in output[1:])
    print(f"rows={rows} routes={routes} extent={extent} first={first} mismatches={differences}", flush=True)
    assert differences == 0
    if rows >= 512:
        samples = {name: [] for name in kernels}
        for order in (tuple(kernels), tuple(reversed(kernels))) * 3:
            for name in order:
                call("cuEventRecord", start, ptr())
                for _ in range(20):
                    launch(name)
                call("cuEventRecord", end, ptr())
                call("cuEventSynchronize", end)
                ms = c.c_float()
                call("cuEventElapsedTime", c.byref(ms), start, end)
                samples[name].append(ms.value / 20)
        for name, values in samples.items():
            print(f"{rows} {routes} {extent} {name} ms/launch {values} median {median(values)}", flush=True)
    for address in addresses:
        call("cuMemFree_v2", address)
