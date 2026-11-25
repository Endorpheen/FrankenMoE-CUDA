"""Check Q8 packing, DP4A products and one-token projection times."""
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
    "component", parent / "EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
gpu, I, P, S, U = c.gpu, c.gpu.I, c.gpu.P, c.gpu.S, c.gpu.U


def read(ptr, length):
    out=(ct.c_ubyte*length)()
    gpu.call("cuMemcpyDtoH_v2",out,ptr,S(length))
    return bytes(out)


def test(name, weight, width, rows, kernels, events, rng):
    x = rng.normal(0,.7,width).astype(np.float32)
    x[:32]=0
    x[32:40]=[127,-127,.5,-.5,1.5,-1.5,63.5,-63.5]
    values=x.reshape(-1,32)
    scales=np.max(np.abs(values),axis=1)/np.float32(127)
    divided=np.divide(values,scales[:,None],out=np.zeros_like(values),where=scales[:,None]!=0)
    signed=np.copysign(np.floor(np.abs(divided.astype(np.float64))+.5),divided).astype(np.int8)
    w,act=map(c.rec.Layer.stage,(weight,x.tobytes()))
    packet,out=gpu.allocate(width//32*36),gpu.allocate(rows*4)
    try:
        def run(mode):
            if mode=="fp32":
                c.dispatch(kernels[mode],((rows+7)//8,1),128,(w,act,out,U(width),U(rows)))
                return
            c.dispatch(kernels["pack"],((width//32+3)//4,1),128,(act,packet,U(width//32)))
            per=4*int(mode[-1])
            c.dispatch(kernels[mode],((rows+per-1)//per,1),128,(w,packet,out,U(width),U(rows)))
        idx=np.unique([0,1,7,8,31,63,rows//2,rows-1])
        raw=np.frombuffer(weight,dtype=np.uint8).reshape(rows,-1,34)[idx]
        digit=((raw[...,2:,None]>>np.array([0,2,4,6],dtype=np.uint8))&3).reshape(len(idx),-1,32).astype(np.int32)-1
        scale_w=np.repeat(raw[...,:2].copy().view("<f2").reshape(len(idx),-1),4,axis=1).astype(np.float32)
        scale_x=scales.astype(np.float16).astype(np.float32)
        dot=np.einsum("rgk,gk->rg",digit,signed.astype(np.int32),dtype=np.int32)
        expected=(dot.astype(np.float32)*(scale_w*scale_x)).sum(axis=1,dtype=np.float64)
        run("fp32")
        baseline=np.frombuffer(read(out,rows*4),dtype="<f4")
        diagnostics={}
        q8_reference=None
        for mode in ("q8_1","q8_2"):
            run(mode)
            buf=np.frombuffer(read(packet,width//32*36),dtype=np.uint8).reshape(-1,36)
            assert buf[:,4:].tobytes()==signed.tobytes()
            assert buf[:,:2].tobytes()==scales.astype("<f2").tobytes()
            raw_output=read(out,rows*4)
            if q8_reference is None: q8_reference=raw_output
            else: assert q8_reference==raw_output
            data=np.frombuffer(raw_output,dtype="<f4")
            assert np.isfinite(data).all()
            np.testing.assert_allclose(data[idx],expected,atol=2e-5,rtol=3e-5)
            diagnostics[mode]=dict(oracle_max_absolute=float(np.max(np.abs(data[idx]-expected))),
                                   l2_relative_to_fp32=float(np.linalg.norm(data-baseline)/np.linalg.norm(baseline)))
        modes=("fp32","q8_1","q8_2")
        times={mode:[] for mode in modes}
        for mode in (modes+modes[::-1])*3:
            def repeat():
                for _ in range(10): run(mode)
            times[mode].append(c.elapsed(repeat,*events)/10)
        return dict(tensor=name,width=width,rows=rows,quality=diagnostics,
                    median_ms={m:median(v) for m,v in times.items()},samples_ms=times)
    finally:
        for ptr in (out,packet,act,w): gpu.call("cuMemFree_v2",ptr)


device,ctx=I(),P()
gpu.call("cuInit",U(0))
gpu.call("cuDeviceGet",ct.byref(device),I(0))
gpu.call("cuCtxCreate_v2",ct.byref(ctx),U(0),device)
try:
    base,new=(c.rec.Layer.load(Path(p).read_bytes()) for p in sys.argv[3:5])
    functions={}
    for mode,name in {"fp32":"bonsai_pq2_matvec_rows2","pack":"bonsai_mv_pack_q8",
                      "q8_1":"bonsai_pq2_mvq_q8_1","q8_2":"bonsai_pq2_mvq_q8_2"}.items():
        f=P()
        gpu.call("cuModuleGetFunction",ct.byref(f),base if mode=="fp32" else new,name.encode())
        functions[mode]=f
    events=c.cuda_event(),c.cuda_event()
    t={a["name"]:a for a in gpu.inventory["tensors"]}
    rng=np.random.default_rng(2012)
    results=[]
    with gpu.model.open("rb") as fd,mmap.mmap(fd.fileno(),0,access=mmap.ACCESS_READ) as data:
        for name,trim in (("blk.0.attn_qkv.weight",1),("blk.0.ssm_out.weight",0),
                          ("blk.0.ffn_gate.weight",0),("blk.0.ffn_up.weight",0),
                          ("blk.0.ffn_down.weight",0),("blk.3.attn_k.weight",0),("output.weight",0)):
            w,r=t[name]["shape"]
            r-=trim
            at=gpu.inventory["data_offset"]+t[name]["offset"]
            results.append(test(name,data[at:at+r*(w//128)*34],w,r,functions,events,rng))
    print(json.dumps(results,indent=2))
finally:
    gpu.call("cuCtxDestroy_v2",ctx)
