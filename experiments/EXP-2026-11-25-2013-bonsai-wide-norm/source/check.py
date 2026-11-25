"""Check bitwise RMSNorm for actual Bonsai weights and timing by batch size."""
import ctypes as ct
import importlib.util
import json
from pathlib import Path
from statistics import median
import sys

import numpy as np

experiments = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("component",
    experiments / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
gpu, D, F, I, P, U = c.gpu,c.gpu.D,c.gpu.F,c.gpu.I,c.gpu.P,c.gpu.U
WIDTH = 5120


def check_case(rows, weight, fn, events, rng, eps):
    x = rng.standard_normal((rows, WIDTH),dtype=np.float32)
    if rows>1:
        x[0]=0
        x[0,0]=-0.0
        x[1]=x[1]*1e-9
    data = c.rec.Layer.stage(x.tobytes())
    out = gpu.allocate(rows*WIDTH*4)
    try:
        def run(mode):
            if mode == "base":
                args = data, weight, D(0), out, U(WIDTH), F(eps), U(0), U(1), U(WIDTH)
                c.dispatch(fn[mode], (rows,1), 32, args)
            else:
                c.dispatch(fn[mode], (rows,1), int(mode.split("_")[0]), (data,weight,out,F(eps)))
        run("base")
        reference=bytes(gpu.download(out,rows*WIDTH))
        assert np.isfinite(np.frombuffer(reference,dtype=np.float32)).all()
        for mode in fn:
            run(mode)
            observed=bytes(gpu.download(out,rows*WIDTH))
            assert observed==reference,(rows,mode,float(np.max(np.abs(
                np.frombuffer(observed,dtype=np.float32)-np.frombuffer(reference,dtype=np.float32)))))
        times={mode:[] for mode in fn}
        for mode in (*fn,*reversed(fn))*4:
            def repeat():
                for _ in range(20): run(mode)
            times[mode].append(c.elapsed(repeat,*events)/20)
        return dict(rows=rows,bit_exact=True,median_ms={m:median(v) for m,v in times.items()},samples_ms=times)
    finally:
        for ptr in (out,data): gpu.call("cuMemFree_v2",ptr)


device,ctx=I(),P()
gpu.call("cuInit",U(0))
gpu.call("cuDeviceGet",ct.byref(device),I(0))
gpu.call("cuCtxCreate_v2",ct.byref(ctx),U(0),device)
try:
    base,trial=(c.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
    fn={}
    for mode in ("base","128_0","256_0","128_1","256_1","512_1"):
        f=P()
        name="bonsai_norm" if mode=="base" else "bonsai_norm_wide_"+mode
        gpu.call("cuModuleGetFunction",ct.byref(f),base if mode=="base" else trial,name.encode())
        fn[mode]=f
    weight=next(t for t in gpu.inventory["tensors"] if t["name"]=="blk.0.attn_norm.weight")
    assert weight["type"]==0 and weight["shape"]==[WIDTH]
    with gpu.model.open("rb") as file:
        file.seek(gpu.inventory["data_offset"]+weight["offset"])
        w=c.rec.Layer.stage(file.read(WIDTH*4))
    try:
        rng=np.random.default_rng(2013)
        eps=gpu.inventory["metadata"]["qwen35.attention.layer_norm_rms_epsilon"]
        events=c.cuda_event(),c.cuda_event()
        print(json.dumps([check_case(rows,w,fn,events,rng,eps) for rows in (1,17,64,128,256,512,1024)],indent=2))
    finally:
        gpu.call("cuMemFree_v2",w)
finally:
    gpu.call("cuCtxDestroy_v2",ctx)
