import ctypes as c
from pathlib import Path
from statistics import median
import numpy as np

root = Path(__file__).resolve().parent
cuda = c.CDLL("libcuda.so.1")
ptr, addr = c.c_void_p, c.c_uint64

def call(name, *args):
    status = getattr(cuda, name)(*args)
    if status:
        raise RuntimeError(f"{name}: {status}")

call("cuInit", 0)
device, context = c.c_int(), ptr()
call("cuDeviceGet", c.byref(device), 0)
call("cuDevicePrimaryCtxRetain", c.byref(context), device)
call("cuCtxSetCurrent", context)
kernels = {}
for name in ("control", "i32", "full"):
    module, fn = ptr(), ptr()
    call("cuModuleLoad", c.byref(module), str(root / f"{name}.cubin").encode())
    call("cuModuleGetFunction", c.byref(fn), module, b"goose_flash_causal_mma")
    kernels[name] = fn

def allocate(nbytes):
    address = addr()
    call("cuMemAlloc_v2", c.byref(address), c.c_size_t(nbytes))
    return address

def upload(host):
    device = allocate(host.nbytes)
    call("cuMemcpyHtoD_v2", device, ptr(host.ctypes.data), c.c_size_t(host.nbytes))
    return device

def launch(fn, args, queries):
    params = (ptr * len(args))(*(c.addressof(a) for a in args))
    tiles = (queries + 7) // 8
    call("cuLaunchKernel", fn, tiles, 2, 1, 128, 1, 1, 33792, ptr(), params, ptr())

start, end = ptr(), ptr()
call("cuEventCreate", c.byref(start), 0)
call("cuEventCreate", c.byref(end), 0)
rng = np.random.default_rng(1859)
for queries, keys, prefix in [(8, 8, 1), (16, 46, 31), (17, 48, 32), (513, 573, 61), (4096, 32768, 28673)]:
    shape = (queries, 16, 256)
    padded = (keys + 31) & ~31
    q = rng.uniform(-1, 1, shape).astype(np.float32)
    k = rng.uniform(-1, 1, (2, padded, 256)).astype(np.float16)
    v = rng.uniform(-1, 1, (2, padded, 256)).astype(np.float16)
    cfg = np.zeros(22, dtype=np.int64)
    cfg[:9] = (256, 256, queries, 16, 2, 2, keys, q.strides[0], q.strides[1])
    cfg[21] = prefix
    out = allocate(q.nbytes)
    args = (upload(q), upload(k), upload(v), out, upload(cfg), c.c_float(0.0625))
    results = {}
    for name, fn in kernels.items():
        call("cuMemsetD8_v2", out, 255, c.c_size_t(q.nbytes))
        launch(fn, args, queries)
        host = np.empty(shape, dtype=np.float32)
        call("cuMemcpyDtoH_v2", ptr(host.ctypes.data), out, c.c_size_t(host.nbytes))
        if queries % 8 == 0:
            assert np.isfinite(host).all(), (name, queries, "unwritten or non-finite")
        assert np.isfinite(host[: queries // 8 * 8]).all(), (name, queries)
        results[name] = host.view(np.uint32)
    matches = {name: np.count_nonzero(results[name] != results["control"]) for name in ("i32", "full")}
    print(f"q={queries} keys={keys} prefix={prefix} mismatches={matches}", flush=True)
    assert all(n == 0 for n in matches.values())
    if queries == 4096:
        scores = {name: [] for name in kernels}
        for order in (("control", "i32", "full"), ("full", "i32", "control")) * 3:
            for name in order:
                call("cuEventRecord", start, ptr())
                for _ in range(3):
                    launch(kernels[name], args, queries)
                call("cuEventRecord", end, ptr())
                call("cuEventSynchronize", end)
                ms = c.c_float()
                call("cuEventElapsedTime", c.byref(ms), start, end)
                scores[name].append(ms.value / 3)
        for name, values in scores.items():
            print(name, "ms per launch", values, "median", median(values), flush=True)
    for address in args[:-1]:
        call("cuMemFree_v2", address)
