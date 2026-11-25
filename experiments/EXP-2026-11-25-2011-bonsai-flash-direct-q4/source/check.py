"""Measure complete compact Flash chains with resident Q4 K/V."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
from statistics import median
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
path = experiments / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py"
spec = importlib.util.spec_from_file_location("component", path)
component = importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu = component.gpu
D, I, P, S, U = gpu.D, gpu.I, gpu.P, gpu.S, gpu.U

MODES = ("stage", "k", "v", "kv", "group2", "group4")


def run(k, n, functions, events, rng):
    chunks, tiles, groups = (k+31)//32, (k+127)//128, 4*((n*6+15)//16)
    capacity = (32<<20)//(tiles*16*258*4)
    first, out_count = k-n+1, n*24*256
    storage = []

    def alloc(size):
        ptr = gpu.allocate(size)
        storage.append(ptr)
        return ptr

    try:
        q0 = rng.standard_normal(out_count, dtype=np.float32)
        q = alloc(q0.nbytes)
        gpu.upload(q, (ct.c_ubyte*q0.nbytes).from_buffer_copy(q0.tobytes()))
        key, value = (alloc(k*576) for _ in range(2))
        packed_k, packed_v = (alloc(chunks*4*16384) for _ in range(2))
        x, scratch, output = alloc(k*4096), alloc(32<<20), alloc(out_count*4)
        for target in (key,value):
            values = (rng.standard_normal(k*1024, dtype=np.float32) * .6).tobytes()
            gpu.upload(x, (ct.c_ubyte*len(values)).from_buffer_copy(values))
            component.dispatch(functions["row"], ((k*32+127)//128,1), 128, (x, target, D(4096), U(32), D(k*32)))

        def chain(mode):
            if mode != "kv":
                component.dispatch(functions["pack"], (chunks,4), 128, (key,value,packed_k,packed_v,U(k),U(chunks)))
            kk = key if "k" in mode else packed_k
            vv = value if "v" in mode else packed_v
            for start in range(0, groups, capacity):
                count = min(capacity, groups-start)
                params = U(n),U(first),U(tiles),U(start)
                group = int(mode[-1]) if mode.startswith("group") else 1
                component.dispatch(functions[mode], (count,(tiles+group-1)//group), 128,
                                   (q,kk,vv,scratch,params[0],U(k),*params[1:],U(chunks)))
                component.dispatch(functions["merge"], (count*16,1), 128,(scratch,output,*params))

        ref = None
        for mode in MODES:
            gpu.call("cuMemsetD8_v2", scratch, U(0), S(32<<20))
            chain(mode)
            dst = bytes(gpu.download(output,out_count))
            partial = bytes(gpu.download(scratch, (32<<20)//4))
            if ref is None:
                ref = dst, partial
                assert np.isfinite(np.frombuffer(dst,dtype=np.float32)).all()
            else:
                assert (dst,partial) == ref, (mode,k,n)
        samples = {mode:[] for mode in MODES}
        for mode in (MODES + MODES[::-1])*3:
            samples[mode].append(component.elapsed(lambda: chain(mode), *events))
        return dict(keys=k, queries=n, exact=True,
                    median_ms={mode:median(values) for mode,values in samples.items()}, samples_ms=samples)
    finally:
        for ptr in reversed(storage):
            gpu.call("cuMemFree_v2",ptr)


device,ctx=I(),P()
gpu.call("cuInit",U(0))
gpu.call("cuDeviceGet",ct.byref(device),I(0))
gpu.call("cuCtxCreate_v2",ct.byref(ctx),U(0),device)
try:
    frozen,base,trial=(component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:6])
    functions = {}
    names = dict(row="goose_set_q4_rows",pack="bonsai_flash_stage_kv",stage="bonsai_flash_async_tile",
                 merge="bonsai_flash_compact_ordered_merge",
                 **{m:f"bonsai_flash_direct_{m}" for m in ("k","v","kv")},
                 group2="bonsai_flash_tile_group2",group4="bonsai_flash_tile_group4")
    for label,name in names.items():
        f=P()
        gpu.call("cuModuleGetFunction",ct.byref(f),frozen if label=="row" else trial if label in MODES[1:] else base,name.encode())
        functions[label]=f
    rng=np.random.default_rng(2011)
    events=component.cuda_event(),component.cuda_event()
    print(json.dumps([run(k,n,functions,events,rng) for k,n in
                      ((257,33),(4096,1024),(8192,1024),(16384,1024),(32768,1024),(32767,127))],indent=2))
finally:
    gpu.call("cuCtxDestroy_v2",ctx)
