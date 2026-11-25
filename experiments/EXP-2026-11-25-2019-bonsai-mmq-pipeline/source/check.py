"""Measure pipelined K32 and K64 with actual Bonsai GGUF projections."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
from statistics import median
import sys
import numpy as np

parent = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "probe", parent / "EXP-2026-11-25-2000-bonsai-mmq-k64/source/check-k64.py")
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)
gpu = probe.gpu
component = probe.component
I, P, U = gpu.I, gpu.P, gpu.U
variants = {
    "64x64_k64": "bonsai_pq2_pipe_64x64_k64",
    "64x128_k64": "bonsai_pq2_pipe_64x128_k64",
    "64x64_k32": "bonsai_pq2_pipe_64x64_k32",
    "64x128_k32": "bonsai_pq2_pipe_64x128_k32",
    "64x64_cached": "bonsai_pq2_cached_64x64_k64",
    "64x128_cached": "bonsai_pq2_cached_64x128_k64",
}

device, ctx = I(), P()
gpu.call("cuInit",U(0))
gpu.call("cuDeviceGet",ct.byref(device),I(0))
gpu.call("cuCtxCreate_v2",ct.byref(ctx),U(0),device)
try:
    control,trial=(component.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])

    def bind(module,name):
        fn=P()
        gpu.call("cuModuleGetFunction",ct.byref(fn),module,name.encode())
        return fn

    common={"pack":bind(control,"bonsai_pack_half"),
            **{name:bind(trial,symbol) for name,symbol in variants.items()}}
    t={x["name"]:x for x in gpu.inventory["tensors"]}
    rng=np.random.default_rng(2019)
    ev=component.cuda_event(),component.cuda_event()
    results=[]
    with gpu.model.open("rb") as file,mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as mm:
        for name,count,trim in (("blk.0.attn_qkv.weight",128,1),("blk.0.attn_qkv.weight",257,0),
            ("blk.0.attn_qkv.weight",1024,0),("blk.0.ffn_gate.weight",1024,0),
            ("blk.0.ffn_up.weight",1024,0),("blk.0.ffn_down.weight",1023,0),
            ("blk.42.ffn_gate.weight",1024,0),("blk.42.ffn_down.weight",1024,0),
            ("blk.3.attn_output.weight",1024,0)):
            w,r=t[name]["shape"]
            r-=trim
            start=gpu.inventory["data_offset"]+t[name]["offset"]
            weight=mm[start:start+r*(w//128)*34]
            baseline=128 if count>=512 else 64
            probe.SHAPES={"control":(64,baseline,256),
                          **{v:(64,int(v.split("_")[0][3:]),256) for v in variants}}
            kernels={**common,"control":bind(control,f"bonsai_pq2_mmq_64x{baseline}_k64")}
            stat=probe.test(kernels,ev,weight,w,r,count,rng)
            stat["medians_ms"]={v:median(x for key,x in stat["ordered_gpu_ms"] if key==v)
                                for v in probe.SHAPES}
            results.append(dict(tensor=name,**stat))
    print(json.dumps(results,indent=2))
finally:
    gpu.call("cuCtxDestroy_v2",ctx)
