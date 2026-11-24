import ctypes as c
from pathlib import Path
from statistics import median

import numpy as np

cuda = c.CDLL("libcuda.so.1")
ptr = c.c_void_p
u64 = c.c_uint64

def call(name, *args):
    status = getattr(cuda, name)(*args)
    if status:
        raise RuntimeError(f"{name}: {status}")

call("cuInit", 0)
device, ctx = c.c_int(), ptr()
call("cuDeviceGet", c.byref(device), 0)
call("cuDevicePrimaryCtxRetain", c.byref(ctx), device)
call("cuCtxSetCurrent", ctx)
kernels = {}
for name in ("control", "candidate"):
    module, kernel = ptr(), ptr()
    path = Path(__file__).resolve().with_name(f"{name}.cubin")
    call("cuModuleLoad", c.byref(module), str(path).encode())
    call("cuModuleGetFunction", c.byref(kernel), module, b"goose_gdn_mma_v32")
    kernels[name] = kernel

def allocate(nbytes):
    address = u64()
    call("cuMemAlloc_v2", c.byref(address), c.c_size_t(nbytes))
    return address

def upload(array):
    address = allocate(array.nbytes)
    call("cuMemcpyHtoD_v2", address, ptr(array.ctypes.data), c.c_size_t(array.nbytes))
    return address

start, end = ptr(), ptr()
call("cuEventCreate", c.byref(start), 0)
call("cuEventCreate", c.byref(end), 0)
rng = np.random.default_rng(1868)
heads, key_heads, width = 32, 16, 128
for nt in (64, 65, 79, 128, 4096):
    def data(low, high, shape):
        return rng.uniform(low, high, shape).astype(np.float32)

    q = data(-0.07, 0.07, (nt, key_heads, width))
    k = data(-0.07, 0.07, q.shape)
    v = data(-0.1, 0.1, (nt, heads, width))
    gate = data(-0.3, -0.01, (nt, heads))
    beta = data(0.05, 0.5, gate.shape)
    state = data(-0.04, 0.04, (heads, width, width))
    inputs = [upload(arr) for arr in (q, k, v, gate, beta, state)]
    buffers = inputs + [allocate(v.nbytes), allocate(state.nbytes)]
    packet = (u64 * 57)()
    packet[:8] = [a.value for a in buffers]
    packet[9:12] = (width, key_heads * width, nt * key_heads * width)
    packet[17:20] = (width, heads * width, nt * heads * width)
    packet[21:24] = (1, heads, nt * heads)
    packet[33], packet[34], packet[52], packet[55] = heads, nt, key_heads, 1
    packet[49] = int(np.float32(width ** -0.5).view(np.uint32))
    args = (ptr * 1)(c.addressof(packet))

    def launch(name):
        call("cuLaunchKernel", kernels[name], heads * 4, 1, 1, 256, 1, 1, 38016 if name == "control" else 38144, ptr(), args, ptr())

    reference = []
    for name in kernels:
        for address, array in zip(buffers[-2:], (v, state), strict=True):
            call("cuMemsetD8_v2", address, 255, c.c_size_t(array.nbytes))
        launch(name)
        outputs = []
        for address, array in zip(buffers[-2:], (v, state), strict=True):
            host = np.empty_like(array)
            call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), address, c.c_size_t(host.nbytes))
            assert np.isfinite(host).all(), (nt, name, "non-finite output")
            outputs.append(host.view(np.uint32))
        if name == "control":
            reference = outputs
        mismatches = [int(np.count_nonzero(a != b)) for a, b in zip(reference, outputs, strict=True)]
        print(f"nt={nt} {name} score_mismatches={mismatches[0]} state_mismatches={mismatches[1]}", flush=True)
        assert mismatches == [0, 0]

    if nt == 4096:
        timings = {name: [] for name in kernels}
        for order in (("control", "candidate"), ("candidate", "control")) * 3:
            for name in order:
                call("cuEventRecord", start, ptr())
                for _ in range(20):
                    launch(name)
                call("cuEventRecord", end, ptr())
                call("cuEventSynchronize", end)
                ms = c.c_float()
                call("cuEventElapsedTime", c.byref(ms), start, end)
                timings[name].append(ms.value / 20)
        for name, samples in timings.items():
            print(name, "ms/launch", samples, "median", median(samples), flush=True)
    for address in buffers:
        call("cuMemFree_v2", address)
