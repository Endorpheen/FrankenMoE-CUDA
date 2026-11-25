"""Compare Inkling grouped IQ4 MMA tile sizes."""
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

rng = random.Random(1946)
rows = 2304
experts = 16
shapes = (7, 31, 64, 128, 257)
try:
    begin, end = event(), event()
    results = {}
    for kind, width, block_values, block_bytes in (("xs", 2304, 256, 136), ("nl", 1152, 32, 18)):
        stride = (width // block_values) * block_bytes
        plane = stride * rows
        encoded = bytearray(rng.randbytes(plane * experts))
        scale = struct.pack("<e", 0.009)
        for pos in range(0, len(encoded), block_bytes):
            encoded[pos:pos + 2] = scale
        weights = alloc(len(encoded), bytes(encoded))
        del encoded
        maximum = max(shapes) * experts
        values = b"".join(struct.pack("<e", rng.uniform(-0.5,0.5)) for _ in range(8192))
        input_f16 = alloc(2 * maximum * width, (values * ((2 * maximum * width + len(values) - 1) // len(values)))[:2 * maximum * width])
        output = alloc(4 * maximum * rows)
        entries = {tile: module_kernel(base/"grouped-moe.cubin",f"goose_i2_{kind}_mmq{tile}")
                   for tile in (32,48,64)}
        results[kind] = {}
        for jobs_per_expert in shapes:
            count = jobs_per_expert * experts
            jobs = alloc(24*count,b"".join(struct.pack("<QQQ",j*width*4,j*rows*4,(j//jobs_per_expert)*plane)
                                           for j in range(count)))
            configs = {}
            for tile in entries:
                tiles = [(e*jobs_per_expert+j,min(tile,jobs_per_expert-j),e*plane)
                         for e in range(experts) for j in range(0,jobs_per_expert,tile)]
                dev = alloc(len(tiles)*24,b"".join(struct.pack("<QQQ",*x) for x in tiles))
                cfg = [weights,input_f16,output,jobs,dev,D(stride),I(width),I(rows)]
                configs[tile] = (len(tiles),cfg,(P * len(cfg))(*(addressof(a) for a in cfg)))
            def launch(tile):
                n, _, args = configs[tile]
                call("cuLaunchKernel",entries[tile],U(rows//64),U(n),U(1),U(128),U(1),U(1),U(0),P(),args,P())
            digests = {}
            for tile in entries:
                launch(tile)
                call("cuCtxSynchronize")
                digests[tile] = hashlib.sha256(retrieve(output, count*rows*4)).hexdigest()
            assert len(set(digests.values())) == 1, (kind,jobs_per_expert,digests)
            samples = {tile:[] for tile in entries}
            for order in (entries,reversed(entries),entries,reversed(entries)):
                for tile in order:
                    launch(tile)
                    call("cuEventRecord",begin,P())
                    for _ in range(20):
                        launch(tile)
                    call("cuEventRecord",end,P())
                    call("cuEventSynchronize",end)
                    timing = F()
                    call("cuEventElapsedTime",byref(timing),begin,end)
                    samples[tile].append(timing.value*1000/20)
            results[kind][jobs_per_expert]={"output_sha256":digests[32],"samples_us":samples,
                "median_us":{tile:statistics.median(v) for tile,v in samples.items()}}
    print(json.dumps(results,indent=2))
finally:
    for handle in events:
        call("cuEventDestroy_v2", handle)
    for module in modules:
        call("cuModuleUnload", module)
    for ptr in allocations:
        call("cuMemFree_v2", ptr)
    call("cuCtxDestroy_v2", context)
