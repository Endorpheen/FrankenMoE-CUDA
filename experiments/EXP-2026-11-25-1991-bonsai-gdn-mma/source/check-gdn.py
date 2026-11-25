"""Qualify frozen Goose GDN kernels on the Bonsai 16/48 head geometry."""
import ctypes as ct
import importlib.util
import json
import math
from pathlib import Path
import struct
import sys

import numpy as np

prior=Path(__file__).resolve().parents[2]/'EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py'
spec=importlib.util.spec_from_file_location('fixture',prior)
component=importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu=component.gpu
D,F,I,P,U=gpu.D,gpu.F,gpu.I,gpu.P,gpu.U
STATE=48*128*128
SCALE=struct.unpack('<I',struct.pack('<f',1/math.sqrt(128)))[0]


def config(tokens):
    return (
        1,128,4096,4096*tokens, 1,128,4096,4096*tokens, 1,128,10240,10240*tokens,
        1,1,48,48*tokens, 1,1,48,48*tokens, 1,128,16384,STATE,
        128,48,tokens,1,0,0,1,0,0,0,8,32,4,32,int(tokens==1),
        STATE,0,SCALE,1,4,16,1,0,1,6144*tokens,
    )


def check(kernels,events,tokens,rng):
    allocations=[]

    def stage(values):
        ptr=component.rec.Layer.stage(np.asarray(values,dtype=np.float32).tobytes())
        allocations.append(ptr)
        return ptr

    def alloc(n):
        ptr=gpu.allocate(n*4)
        allocations.append(ptr)
        return ptr

    def offset(ptr,n):
        return D(ptr.value+n*4)

    def launch(mode,key_count,ptrs):
        kernel,x,z,threads,shared=kernels[mode]
        values=tuple(p.value for p in ptrs)
        packet=(D*57)(*values,*config(key_count))
        gpu.call('cuLaunchKernel',kernel,U(x),U(1),U(z),U(threads),U(1),U(1),
                 U(shared),P(),(P*1)(ct.addressof(packet)),P())

    def relative(a,b):
        return dict(relative_l2=float(np.linalg.norm(a-b)/np.linalg.norm(a)),
                    max_abs=float(np.max(np.abs(a-b))))

    qk=rng.normal(0,1,(tokens+1,32,128))
    qk/=np.linalg.norm(qk,axis=-1,keepdims=True)
    v=np.zeros((tokens+1,10240),dtype=np.float32)
    v[:,4096:]=rng.normal(0,.1,(tokens+1,6144))
    gate=-rng.uniform(.003,.20,(tokens+1,48))
    beta=rng.uniform(.05,.95,(tokens+1,48))
    try:
        q=stage(qk)
        source=[q,offset(q,2048),offset(stage(v),4096),stage(gate),stage(beta)]
        tail=[offset(p,tokens*s) for p,s in zip(source,(4096,4096,10240,48,48))]
        initial=stage(rng.normal(0,.008,STATE))
        y,s=alloc(tokens*6144),alloc(STATE)
        next_y,next_state=alloc(6144),alloc(STATE)

        def get(mode):
            launch(mode,tokens,(*source,initial,y,s))
            launch('decode',1,(*tail,s,next_y,next_state))
            return tuple(np.ctypeslib.as_array(gpu.download(p,n)) for p,n in
                        ((y,tokens*6144),(s,STATE),(next_y,6144),(next_state,STATE)))

        control=get('scalar')
        values={mode:get(mode) for mode in ('mma','v32')}
        stats={}
        for mode,data in values.items():
            assert all(np.isfinite(arr).all() for arr in data)
            stats[mode]={name:relative(a,b) for name,a,b in zip(
                ('scores','carry','decode_scores','decode_carry'),control,data)}
        scores={mode:[] for mode in ('scalar','mma','v32')}
        for mode in ('scalar','mma','v32','v32','mma','scalar')*2:
            scores[mode].append(component.elapsed(lambda:launch(mode,tokens,(*source,initial,y,s)),*events))
        return dict(tokens=tokens,measured_gpu_ms=scores,errors_vs_scalar=stats,
                    l2_limit=.001,passes={mode:all(v['relative_l2']<=.001 for v in details.values())
                         for mode,details in stats.items()})
    finally:
        for ptr in reversed(allocations):
            gpu.call('cuMemFree_v2',ptr)


def main():
    frozen=Path(sys.argv[3]).read_bytes()
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        mod=component.rec.Layer.load(frozen)
        functions={}
        for mode,name,x,z,threads,shared in (
            ('scalar','goose_gated_delta_net',48,8,128,0),
            ('decode','goose_gdn_decode',48,8,128,0),
            ('mma','goose_gdn_mma',48,1,256,93312),
            ('v32','goose_gdn_mma_v32',192,1,256,38016),
        ):
            kernel=P()
            gpu.call('cuModuleGetFunction',ct.byref(kernel),mod,name.encode())
            if shared:
                gpu.call('cuFuncSetAttribute',kernel,I(8),I(shared))
            functions[mode]=(kernel,x,z,threads,shared)
        events=component.cuda_event(),component.cuda_event()
        rng=np.random.default_rng(1991)
        print(json.dumps([check(functions,events,n,rng) for n in (65,257,1024)],indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
