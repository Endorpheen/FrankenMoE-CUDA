"""Time INT8 PQ2 and cross-check its block-scaled arithmetic independently."""
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


def cpu_reference(data,raw,rows,width,residual):
    sample_rows=np.unique(np.array([0,1,7,8,15,16,31,32,63,64,rows//2,rows-1]))
    sample_tokens=np.unique(np.linspace(0,len(data)-1,24,dtype=int))
    blocks=width//128
    a=data[sample_tokens].reshape(-1,blocks,128)
    if residual:
        a=a.astype(np.float16).astype(np.float32)
    scale_x=np.max(np.abs(a),axis=-1)/np.float32(127)
    scale_safe=np.where(scale_x>0,scale_x,np.float32(1))
    qx=np.rint(a/scale_safe[...,None]).clip(-127,127).astype(np.int32)
    matrix=np.frombuffer(raw,dtype=np.uint8).reshape(rows,blocks,34)[sample_rows]
    scale_w=matrix[:,:,:2].copy().view('<f2').reshape(-1,blocks).astype(np.float64)
    table=np.array([0,2,4,6],dtype=np.uint8)
    code=((matrix[:,:,2:,None]>>table)&3).reshape(-1,blocks,128).astype(np.int32)-1
    dots=np.einsum('tbk,rbk->trb',qx,code,dtype=np.int32)
    if residual:
        error=a-qx.astype(np.float32)*scale_x[...,None]
        qlo=np.rint(error / (scale_safe[...,None]/np.float32(254))).clip(-127,127).astype(np.int32)
        dots=dots.astype(np.float64)+np.einsum('tbk,rbk->trb',qlo,code,dtype=np.int32)/254
    expected=(dots*scale_w[None,:,:]*scale_x[:,None,:]).sum(axis=-1)
    return sample_tokens,sample_rows,expected


def run_case(k,events,raw,width,rows,tokens,rng):
    x=rng.normal(0,.7,(tokens,width)).astype(np.float32)
    x[0]=0
    weights=component.rec.Layer.stage(raw)
    src=component.rec.Layer.stage(x.tobytes())
    half=gpu.allocate(tokens*width*2)
    q8=gpu.allocate(tokens*width)
    qlow=gpu.allocate(tokens*width)
    scales=gpu.allocate(tokens*(width//128)*4)
    dst=gpu.allocate(tokens*rows*4)
    try:
        pairs=tokens*width//2
        blocks=tokens*(width//128)
        grid=((rows+63)//64,(tokens+63)//64)
        shape=U(width),U(rows),U(tokens)

        def f16():
            component.dispatch(k['half'],((pairs+255)//256,1),256,(src,half,U(pairs)))
            component.dispatch(k['f16'],grid,256,(weights,half,dst,*shape))

        def integer(mode):
            packs=(q8,qlow,scales) if mode=='residual' else (q8,scales)
            convert='quant_residual' if mode=='residual' else 'quant8'
            component.dispatch(k[convert],((blocks+3)//4,1),128,(src,*packs,U(blocks)))
            component.dispatch(k[mode],grid,256,(weights,*packs,dst,*shape))

        f16()
        a=np.ctypeslib.as_array(gpu.download(dst,rows*tokens)).reshape(tokens,rows)
        differences={}
        for mode in ('int8','residual'):
            integer(mode)
            b=np.ctypeslib.as_array(gpu.download(dst,rows*tokens)).reshape(tokens,rows)
            assert np.isfinite(a).all() and np.isfinite(b).all()
            sample,indices,gold=cpu_reference(x,raw,rows,width,mode=='residual')
            observed=b[np.ix_(sample,indices)]
            error=float(np.max(np.abs(observed-gold)))
            np.testing.assert_allclose(observed,gold,atol=2e-4,rtol=3e-5)
            assert not b[0].any()
            l2=float(np.linalg.norm(a-b)/np.linalg.norm(a))
            differences[mode]=dict(relative_l2_to_f16=l2,max_abs_to_f16=float(np.max(np.abs(a-b))),
                                   independent_max_absolute=error,fixture_pass=l2<(.0001 if mode=='residual' else .015))
        modes=('f16','int8','residual')
        times=[]
        for mode in (modes+modes[::-1])*2:
            fn=f16 if mode=='f16' else lambda:integer(mode)
            times.append((mode,component.elapsed(fn,*events)))
        return dict(rows=rows,width=width,tokens=tokens,precision=differences,ordered_gpu_ms=times)
    finally:
        for ptr in (dst,scales,qlow,q8,half,src,weights):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        module=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        k={}
        for name,symbol in dict(half='bonsai_pack_half',f16='bonsai_pq2_mmq_64x64',
                                quant8='bonsai_pack_q8_128',int8='bonsai_pq2_mmq_int8',
                                quant_residual='bonsai_pack_q8_residual',residual='bonsai_pq2_mmq_q8_residual').items():
            handle=P()
            gpu.call('cuModuleGetFunction',ct.byref(handle),module,symbol.encode())
            k[name]=handle
        events=component.cuda_event(),component.cuda_event()
        tensors={t['name']:t for t in gpu.inventory['tensors']}
        rng=np.random.default_rng(1994)
        output=[]
        with gpu.model.open('rb') as fd, mmap.mmap(fd.fileno(),0,access=mmap.ACCESS_READ) as model:
            for name,tokens,trim in (
                ('blk.0.attn_qkv.weight',33,1),('blk.0.attn_qkv.weight',257,0),
                ('blk.0.attn_qkv.weight',1024,0),('blk.3.attn_output.weight',1024,0),
                ('blk.0.ffn_up.weight',1024,0),('blk.0.ffn_down.weight',1024,0),
            ):
                tensor=tensors[name]
                width,rows=tensor['shape']
                assert tensor['type']==142
                rows-=trim
                offset=gpu.inventory['data_offset']+tensor['offset']
                raw=model[offset:offset+rows*(width//128)*34]
                output.append(dict(tensor=name,**run_case(k,events,raw,width,rows,tokens,rng)))
        print(json.dumps(output,indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
