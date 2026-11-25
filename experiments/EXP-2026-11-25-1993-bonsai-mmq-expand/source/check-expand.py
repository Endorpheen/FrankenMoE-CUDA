"""Measure bounded PQ2 expansion together with dense half MMQ."""
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
MODES=('packed','expanded','planes')


def test(k, events, raw, width, rows, tokens, rng):
    data=np.asarray(rng.normal(0,.7,(tokens,width)),dtype=np.float32)
    weights=component.rec.Layer.stage(raw)
    input_f32=component.rec.Layer.stage(data.tobytes())
    input_half=gpu.allocate(tokens*width*2)
    output=gpu.allocate(rows*tokens*4)
    expanded=gpu.allocate(rows*width*2)
    blocks=width//128
    codes=gpu.allocate(rows*blocks*32)
    scales=gpu.allocate(rows*blocks*2)
    try:
        pairs=tokens*width//2
        component.dispatch(k['pack'],((pairs+255)//256,1),256,(input_f32,input_half,U(pairs)))
        def prepare(name):
            if name=='expanded':
                work=rows*width//4
                component.dispatch(k['prepare'],((work+255)//256,1),256,
                                   (weights,expanded,U(width),U(rows)))
            if name=='planes':
                work=rows*blocks*8
                component.dispatch(k['split'],((work+255)//256,1),256,
                                   (weights,codes,scales,U(blocks),U(rows)))

        def run(name):
            prepare(name)
            operands=(codes,scales) if name=='planes' else (expanded if name=='expanded' else weights,)
            component.dispatch(k[name],((rows+63)//64,(tokens+63)//64),256,
                               (*operands,input_half,output,U(width),U(rows),U(tokens)))
        reference=None
        for name in MODES:
            run(name)
            observed=bytes(gpu.download(output,rows*tokens))
            if reference is None:
                reference=observed
                assert np.isfinite(np.frombuffer(reference,dtype=np.float32)).all()
            else:
                assert observed==reference,(name,width,rows,tokens)
        order=MODES
        scores=[(name,component.elapsed(lambda:run(name),*events)) for name in (order+order[::-1])*2]
        return dict(rows=rows,width=width,tokens=tokens,bit_exact=True,ordered_gpu_ms=scores,
                    expansion_gpu_ms=component.elapsed(lambda:prepare('expanded'),*events),
                    split_gpu_ms=component.elapsed(lambda:prepare('planes'),*events),
                    scratch_bytes=rows*width*2,plane_scratch_bytes=rows*blocks*34)
    finally:
        for ptr in (scales,codes,expanded,output,input_half,input_f32,weights):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        mod=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        names=dict(pack='bonsai_pack_half',packed='bonsai_pq2_mmq_64x64',
                   expanded='bonsai_pq2_mmq_expanded',prepare='bonsai_pq2_expand_half',
                   planes='bonsai_pq2_mmq_planes',split='bonsai_pq2_split_planes')
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
                ('blk.0.attn_qkv.weight',33,1),('blk.0.attn_qkv.weight',257,0),('blk.0.attn_qkv.weight',1024,0),
                ('blk.3.attn_output.weight',1024,0),('blk.0.ffn_up.weight',1024,0),
                ('blk.0.ffn_down.weight',1024,0),
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
