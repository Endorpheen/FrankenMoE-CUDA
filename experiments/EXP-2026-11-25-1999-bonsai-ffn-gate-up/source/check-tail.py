"""Verify fused SwiGLU/FWHT/FP16 packing on real projection outputs."""
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
MODES=('original','tail')


def check(k,events,weights,signs,width,rows,tokens,rng):
    src=component.rec.Layer.stage(rng.normal(0,.7,(tokens,width)).astype(np.float32).tobytes())
    half=gpu.allocate(tokens*width*2)
    gates,ups,out=[gpu.allocate(tokens*rows*4) for _ in range(3)]
    rotated=gpu.allocate(tokens*rows*4)
    packed=gpu.allocate(tokens*rows*2)
    try:
        shape=U(width),U(rows),U(tokens)
        pairs=tokens*width//2
        grid=((rows+63)//64,(tokens+63)//64)
        component.dispatch(k['pack'],((pairs+255)//256,1),256,(src,half,U(pairs)))

        for w,y in zip(weights,(gates,ups)):
            component.dispatch(k['projection'],grid,256,(w,half,y,*shape))
        count=tokens*rows
        def run(name):
            if name=='original':
                component.dispatch(k['swiglu'],((count+255)//256,1),256,(gates,ups,out,U(count)))
                component.dispatch(k['rotate'],(rows//1024,tokens),256,
                    (out,signs,rotated,U(0),U(0),U(0),U(1),U(rows)))
                component.dispatch(k['pack'],((count//2+255)//256,1),256,(rotated,packed,U(count//2)))
            else:
                component.dispatch(k[name],(rows//1024,tokens),256,(gates,ups,signs,packed,U(rows)))
        run('original')
        expected=bytes(gpu.download(packed,count//2))
        run('tail')
        assert expected==bytes(gpu.download(packed,count//2)), tokens
        times={n:[] for n in MODES}
        for name in (MODES+MODES[::-1])*2:
            times[name].append(component.elapsed(lambda:run(name),*events))
        return dict(rows=rows,width=width,tokens=tokens,packed_bytes_exact=True,timings_ms=times)
    finally:
        for ptr in (packed,rotated,out,ups,gates,half,src):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        mod=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names={'pack':'bonsai_pack_half','projection':'bonsai_pq2_mmq_64x64','swiglu':'bonsai_swiglu',
            'rotate':'bonsai_fwht1024','tail':'bonsai_ffn_tail'}
        kernels={}
        for label,name in names.items():
            fn=P()
            gpu.call('cuModuleGetFunction',ct.byref(fn),mod,name.encode())
            kernels[label]=fn
        ev=component.cuda_event(),component.cuda_event()
        tensors={x['name']:x for x in gpu.inventory['tensors']}
        signs=gpu.inventory['metadata']['prism.hadamard.sign_values']
        widths=gpu.inventory['metadata']['prism.hadamard.sign_widths']
        ix=widths.index(17408)
        start=sum(widths[:ix])
        dev_signs=component.rec.Layer.stage(np.array(signs[start:start+17408],dtype='<i4').tobytes())
        rng=np.random.default_rng(1999)
        results=[]
        with gpu.model.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            for layer,shapes in [(0,((33,0),(257,0),(1024,0))), (42,((1023,0),(1024,0)))]:
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
                        results.append(dict(layer=layer,**check(kernels,ev,weights,dev_signs,width,rows,n,rng)))
                    finally:
                        for ptr in weights:
                            gpu.call('cuMemFree_v2',ptr)
        print(json.dumps(results,indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
