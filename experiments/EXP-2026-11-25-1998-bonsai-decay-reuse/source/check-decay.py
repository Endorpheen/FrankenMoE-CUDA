"""Check gate/mix bytes and time the weight-reuse geometry on real matrices."""
import ctypes as ct
import importlib.util
import json
import math
import mmap
from pathlib import Path
import sys

import numpy as np

prior=Path(__file__).resolve().parents[2]/'EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py'
spec=importlib.util.spec_from_file_location('fixture',prior)
component=importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu=component.gpu
P,U,I=gpu.P,gpu.U,gpu.I
MODES=('original','warp_pack','reuse2','reuse4','reuse8')


def test(k,ev,weights,count,rng):
    src=component.rec.Layer.stage(rng.normal(0,0.9,(count,5120)).astype(np.float32).tobytes())
    gate=gpu.allocate(count*48*4)
    mix=gpu.allocate(count*48*4)
    try:
        args=(src,*weights,gate,mix)
        def run(name):
            if name=='original':
                component.dispatch(k[name],(count*48,1),32,args)
            else:
                tile={'warp_pack':1,'reuse2':2,'reuse4':4,'reuse8':8}[name]
                groups=((count+tile-1)//tile)*12
                component.dispatch(k[name],(groups,1),128,(*args,U(count)))
        run('original')
        original=[bytes(gpu.download(p,count*48)) for p in (gate,mix)]
        assert all(np.isfinite(np.frombuffer(v,dtype='<f4')).all() for v in original)
        for name in MODES[1:]:
            run(name)
            assert original==[bytes(gpu.download(p,count*48)) for p in (gate,mix)],(name,count)
        values={name:[] for name in MODES}
        for name in (MODES+MODES[::-1])*2:
            values[name].append(component.elapsed(lambda:run(name),*ev))
        return dict(tokens=count,bit_exact=True,timings_ms=values)
    finally:
        for ptr in (mix,gate,src):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        module=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        k={}
        for name in MODES:
            symbol='bonsai_decay' if name=='original' else 'bonsai_decay_'+name
            fn=P()
            gpu.call('cuModuleGetFunction',ct.byref(fn),module,symbol.encode())
            k[name]=fn
        ev=component.cuda_event(),component.cuda_event()
        tensors={r['name']:r for r in gpu.inventory['tensors']}
        rng=np.random.default_rng(1998)
        data=[]
        with gpu.model.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            for layer,batches in [(0,(1,33,273,1024)),(42,(1023,1024))]:
                weights=[]
                for name in ('ssm_alpha.weight','ssm_beta.weight','ssm_dt.bias','ssm_a'):
                    t=tensors[f'blk.{layer}.{name}']
                    offset=gpu.inventory['data_offset']+t['offset']
                    assert t['type']==0
                    weights.append(component.rec.Layer.stage(m[offset:offset+4*math.prod(t['shape'])]))
                try:
                    for count in batches:
                        data.append(dict(layer=layer,**test(k,ev,weights,count,rng)))
                finally:
                    for ptr in weights:
                        gpu.call('cuMemFree_v2',ptr)
        print(json.dumps(data,indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
