"""Compare exact SwiGLU bytes and whole stage timing on real FFN pairs."""
import ctypes as ct
import importlib.util
import json
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
MODES=('original','32x64','64x64')


def check(k,events,weights,width,rows,tokens,rng):
    src=component.rec.Layer.stage(rng.normal(0,.7,(tokens,width)).astype(np.float32).tobytes())
    half=gpu.allocate(tokens*width*2)
    gates,ups,out=[gpu.allocate(tokens*rows*4) for _ in range(3)]
    try:
        shape=U(width),U(rows),U(tokens)
        pairs=tokens*width//2
        grid=((rows+63)//64,(tokens+63)//64)
        component.dispatch(k['pack'],((pairs+255)//256,1),256,(src,half,U(pairs)))

        def run(name):
            if name=='original':
                for w,y in zip(weights,(gates,ups)):
                    component.dispatch(k['projection'],grid,256,(w,half,y,*shape))
                count=tokens*rows
                component.dispatch(k['swiglu'],((count+255)//256,1),256,(gates,ups,out,U(count)))
            else:
                m=int(name.split('x')[0])
                component.dispatch(k[name],((rows+m-1)//m,grid[1]),256,(*weights,half,out,*shape))
        run('original')
        expected=bytes(gpu.download(out,tokens*rows))
        assert np.isfinite(np.frombuffer(expected,dtype='<f4')).all()
        for name in MODES[1:]:
            run(name)
            observed=bytes(gpu.download(out,tokens*rows))
            assert observed==expected,(name,tokens,rows,np.max(
                np.abs(np.frombuffer(expected,dtype='<f4')-np.frombuffer(observed,dtype='<f4'))))
        times={n:[] for n in MODES}
        for name in (MODES+MODES[::-1])*2:
            times[name].append(component.elapsed(lambda:run(name),*events))
        return dict(rows=rows,width=width,tokens=tokens,bit_exact=True,timings_ms=times)
    finally:
        for ptr in (out,ups,gates,half,src):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        mod=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names={'pack':'bonsai_pack_half','projection':'bonsai_pq2_mmq_64x64','swiglu':'bonsai_swiglu',
            **{n:'bonsai_ffn_fused_'+n for n in MODES[1:]}}
        kernels={}
        for label,name in names.items():
            fn=P()
            gpu.call('cuModuleGetFunction',ct.byref(fn),mod,name.encode())
            kernels[label]=fn
        ev=component.cuda_event(),component.cuda_event()
        tensors={x['name']:x for x in gpu.inventory['tensors']}
        rng=np.random.default_rng(1999)
        results=[]
        with gpu.model.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            for layer,shapes in [(0,((33,1),(257,0),(1024,0))), (42,((1023,0),(1024,0)))]:
                for n,trim in shapes:
                    weights=[]
                    for name in ('ffn_gate','ffn_up'):
                        t=tensors[f'blk.{layer}.{name}.weight']
                        width,rows=t['shape']
                        rows-=trim
                        assert t['type']==142
                        at=gpu.inventory['data_offset']+t['offset']
                        weights.append(component.rec.Layer.stage(m[at:at+rows*(width//128)*34]))
                    try:
                        results.append(dict(layer=layer,**check(kernels,ev,weights,width,rows,n,rng)))
                    finally:
                        for ptr in weights:
                            gpu.call('cuMemFree_v2',ptr)
        print(json.dumps(results,indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
