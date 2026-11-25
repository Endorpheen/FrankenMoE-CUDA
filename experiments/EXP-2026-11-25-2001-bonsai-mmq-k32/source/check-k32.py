"""Check K32 and alternative exact K64 geometry against the accepted kernels."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
import sys

import numpy as np

prior=Path(__file__).resolve().parents[2]/'EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py'
spec=importlib.util.spec_from_file_location('component',prior)
component=importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu=component.gpu
P,U,I=gpu.P,gpu.U,gpu.I
SHAPES={'control':(64,128,256),'k32-64x64':(64,64,256),'k32-64x128':(64,128,256),
        'k64-32x128-w8':(32,128,256),'k64-32x128-w4':(32,128,128),
        'k32-32x128-w8':(32,128,256),'k32-64x128-w4':(64,128,128),
        'k32-w4-lut':(64,128,128),'k64-w8-lut':(64,128,256)}


def test(k, events, raw, width, rows, tokens, rng):
    data=np.asarray(rng.normal(0,.7,(tokens,width)),dtype=np.float32)
    weights=component.rec.Layer.stage(raw)
    input_f32=component.rec.Layer.stage(data.tobytes())
    input_half=gpu.allocate(tokens*width*2)
    output=gpu.allocate(rows*tokens*4)
    try:
        pairs=tokens*width//2
        component.dispatch(k['pack'],((pairs+255)//256,1),256,(input_f32,input_half,U(pairs)))
        args=(weights,input_half,output,U(width),U(rows),U(tokens))

        def run(name):
            m,n,threads=SHAPES[name]
            if name=='control' and tokens<512:
                n=64
                name='control64'
            component.dispatch(k[name],((rows+m-1)//m,(tokens+n-1)//n),threads,args)
        reference=None
        for name in SHAPES:
            run(name)
            observed=bytes(gpu.download(output,rows*tokens))
            if reference is None:
                reference=observed
                assert np.isfinite(np.frombuffer(reference,dtype=np.float32)).all()
            else:
                assert observed==reference,(name,width,rows,tokens)
        order=tuple(SHAPES)
        scores=[(name,component.elapsed(lambda:run(name),*events)) for name in (order+order[::-1])*2]
        return dict(rows=rows,width=width,tokens=tokens,bit_exact=True,ordered_gpu_ms=scores)
    finally:
        for ptr in (output,input_half,input_f32,weights):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        mod=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names={
            'pack':'bonsai_pack_half',
            'control':'bonsai_pq2_mmq_64x128_k64','control64':'bonsai_pq2_mmq_64x64_k64',
            'k32-64x64':'bonsai_mmq_64x64_w8_k32','k32-64x128':'bonsai_mmq_64x128_w8_k32',
            'k64-32x128-w8':'bonsai_mmq_32x128_w8_k64','k64-32x128-w4':'bonsai_mmq_32x128_w4_k64',
            'k32-32x128-w8':'bonsai_mmq_32x128_w8_k32','k32-64x128-w4':'bonsai_mmq_64x128_w4_k32',
            'k32-w4-lut':'bonsai_mmq_64x128_w4_k32_lut','k64-w8-lut':'bonsai_mmq_64x128_w8_k64_lut',
        }
        kernels={}
        for label,symbol in names.items():
            kernel=P()
            gpu.call('cuModuleGetFunction',ct.byref(kernel),mod,symbol.encode())
            kernels[label]=kernel
        events=component.cuda_event(),component.cuda_event()
        tensors={t['name']:t for t in gpu.inventory['tensors']}
        rng=np.random.default_rng(1990)
        results=[]
        with gpu.model.open('rb') as fd, mmap.mmap(fd.fileno(),0,access=mmap.ACCESS_READ) as model:
            for name,tokens,trim in (
                ('blk.0.attn_qkv.weight',33,1),('blk.0.attn_qkv.weight',512,0),('blk.0.attn_qkv.weight',257,0),('blk.0.attn_qkv.weight',1024,0),
                ('blk.3.attn_output.weight',1024,0),('blk.0.ffn_up.weight',1024,0),
                ('blk.0.ffn_down.weight',1024,0),('blk.0.ffn_down.weight',1023,0),
                ('blk.0.ffn_gate.weight',1024,0),('blk.42.ffn_gate.weight',1024,0),
                ('blk.42.ffn_up.weight',1024,0),('blk.42.ffn_down.weight',1024,0),
            ):
                tensor=tensors[name]
                width,rows=tensor['shape']
                assert tensor['type']==142
                rows-=trim
                offset=gpu.inventory['data_offset']+tensor['offset']
                weight=model[offset:offset+rows*(width//128)*34]
                results.append(dict(tensor=name,**test(kernels,events,weight,width,rows,tokens,rng)))
        print(json.dumps(results,indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
