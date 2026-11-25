"""Report paired run overhead and per-CTA clock partitions for 64x64 PQ2."""
import ctypes as ct
import importlib.util
import json
import mmap
from pathlib import Path
import statistics
import sys

import numpy as np

prior=Path(__file__).resolve().parents[2]/'EXP-2026-11-25-1978-bonsai-pq2-mmq/source/check-mmq.py'
spec=importlib.util.spec_from_file_location('component',prior)
component=importlib.util.module_from_spec(spec)
spec.loader.exec_module(component)
gpu=component.gpu
P,U,I,S=gpu.P,gpu.U,gpu.I,gpu.S
PHASES=('issue_input_copies','unpack_weights','wait_and_barrier','matrix_load_mma_and_barrier')


def measure(k,events,raw,width,rows,rng):
    tokens=1024
    x=rng.normal(0,.7,(tokens,width)).astype(np.float32)
    weights=component.rec.Layer.stage(raw)
    src=component.rec.Layer.stage(x.tobytes())
    half=gpu.allocate(tokens*width*2)
    out=gpu.allocate(tokens*rows*4)
    grid=((rows+63)//64,(tokens+63)//64)
    counters=gpu.allocate(grid[0]*grid[1]*4*8)
    try:
        gpu.call('cuMemsetD8_v2',counters,ct.c_uint8(0),S(grid[0]*grid[1]*4*8))
        pairs=tokens*width//2
        component.dispatch(k['pack'],((pairs+255)//256,1),256,(src,half,U(pairs)))
        params=weights,half,out,U(width),U(rows),U(tokens)

        def run(label):
            args=(*params,counters) if label=='instrumented' else params
            component.dispatch(k[label],grid,256,args)

        run('baseline')
        original=bytes(gpu.download(out,tokens*rows))
        run('instrumented')
        assert original==bytes(gpu.download(out,tokens*rows))
        raw=np.frombuffer(bytes(gpu.download(counters,grid[0]*grid[1]*8)),dtype='<u8').reshape(-1,4)
        active=raw[np.any(raw,axis=1)]
        assert len(active)==((grid[0]+7)//8)*((grid[1]+1)//2)
        fractions=active/active.sum(axis=1,keepdims=True)
        timing={mode:[] for mode in ('baseline','instrumented')}
        for mode in ('baseline','instrumented','instrumented','baseline')*2:
            timing[mode].append(component.elapsed(lambda:run(mode),*events))
        return dict(width=width,rows=rows,query_tokens=tokens,output_bytes_equal=True,ctas_sampled=len(active),
            phase_median_fraction=dict(zip(PHASES,np.median(fractions,axis=0).tolist())),
            phase_weighted_fraction=dict(zip(PHASES,(active.sum(axis=0)/active.sum()).tolist())),
            gpu_ms={mode:statistics.median(v) for mode,v in timing.items()},samples_ms=timing)
    finally:
        for ptr in (counters,out,half,src,weights):
            gpu.call('cuMemFree_v2',ptr)


def main():
    device,context=I(),P()
    gpu.call('cuInit',U(0))
    gpu.call('cuDeviceGet',ct.byref(device),I(0))
    gpu.call('cuCtxCreate_v2',ct.byref(context),U(0),device)
    try:
        mod=component.rec.Layer.load(Path(sys.argv[3]).read_bytes())
        k={}
        for label,name in dict(pack='bonsai_pack_half',baseline='bonsai_pq2_mmq_64x64',
                               instrumented='bonsai_pq2_mmq_budget').items():
            handle=P()
            gpu.call('cuModuleGetFunction',ct.byref(handle),mod,name.encode())
            k[label]=handle
        ev=component.cuda_event(),component.cuda_event()
        tensors={x['name']:x for x in gpu.inventory['tensors']}
        rng=np.random.default_rng(1995)
        names=('blk.0.attn_qkv.weight','blk.0.ffn_up.weight','blk.0.ffn_down.weight')
        rows=[]
        with gpu.model.open('rb') as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as m:
            for name in names:
                x=tensors[name]
                width,height=x['shape']
                start=gpu.inventory['data_offset']+x['offset']
                raw=m[start:start+width//128*height*34]
                rows.append(dict(tensor=name,**measure(k,ev,raw,width,height,rng)))
        print(json.dumps(rows,indent=2))
    finally:
        gpu.call('cuCtxDestroy_v2',context)


if __name__=='__main__':
    main()
