"""Compare real inputs to the Q4 writer and their measured quantized bytes."""
import json
from itertools import zip_longest
from pathlib import Path
import struct
import sys

import numpy as np

out=Path(sys.argv[1])
header=struct.Struct('<IIIII')
labels={0:'layer_sample',1:'key_input',2:'value_input',3:'key_q4',4:'value_q4',5:'gdn_sample'}


def records(path):
    with path.open('rb') as f:
        while key:=f.read(header.size):
            kind,pos,count,layer,size=header.unpack(key)
            payload=f.read(size)
            assert len(payload)==size
            yield (kind,pos,count,layer,size),payload


def q4_codes(packed):
    payload=packed[:,2:]
    return np.column_stack((payload&15,payload>>4))


def coordinates(flat):
    token,idx=divmod(flat,32)
    return dict(token=token//32,head=token//8%4,block=token%8,index=idx)


ranges=[]
last=[]
for l,r in zip_longest(records(out/'fp16-state.bin'),records(out/'residual-state.bin')):
    assert l[0]==r[0]
    (kind,pos,count,layer,size),a,b=l[0],l[1],r[1]
    row=dict(kind=labels[kind],position=pos,count=count,layer=layer)
    if kind in (3,4):
        x,y=(np.frombuffer(p,dtype=np.uint8).reshape(-1,18) for p in (a,b))
        code_x,code_y=q4_codes(x),q4_codes(y)
        changed=code_x!=code_y
        scale_diff=np.any(x[:,:2]!=y[:,:2],axis=1)
        same_pivot_candidates=changed & ~scale_diff[:,None]
        row.update(blocks=len(x),changed_blocks=int(np.any(x!=y,axis=1).sum()),
                   changed_scales=int(scale_diff.sum()),changed_codes=int(changed.sum()),
                   changed_codes_with_same_half_scale=int(same_pivot_candidates.sum()),
                   changed_bytes=int((x!=y).sum()))
        assert last and last[-1][0]==(kind-2,pos,count,layer,count*4096)
        if same_pivot_candidates.any():
            idx=int(np.flatnonzero(same_pivot_candidates)[0])
            term,element=divmod(idx,32)
            aa,bb=(np.frombuffer(p,dtype='<f4').reshape(-1,32)[term] for p in last[-1][1:])
            # Match the writer's order: signed maximum, F32 division and multiplication,
            # then truncation of normalized weight + 8.5.
            scaled=[]
            for source in (aa,bb):
                d=np.float32(-source[np.argmax(np.abs(source))]/8)
                scaled.append(float(np.float32(source[element]*np.float32(1/d)+np.float32(8.5))))
            row['example']=dict(**coordinates(idx),inputs=[float(aa[element]),float(bb[element])],
                                pretruncation=scaled,codes=[int(code_x[term,element]),int(code_y[term,element])])
    else:
        x,y=(np.frombuffer(p,dtype='<f4').astype(np.float64) for p in (a,b))
        delta=x-y
        row.update(elements=len(x),relative_l2=float(np.linalg.norm(delta)/max(np.linalg.norm(x),1e-20)),
                   max_abs=float(np.max(np.abs(delta))))
    ranges.append(row)
    if kind in (1,2):
        last=[(l[0],a,b)]
dtype=np.dtype([('step','<u4'),('logits','<f4',(248320,))])
tops={}
ref=np.fromfile(out/'fp16-logits.bin',dtype=dtype)[-1]
for key in ('fp16','residual','both','k','v'):
    file=out/(key+'-logits.bin')
    if not file.exists():
        continue
    sample=np.fromfile(file,dtype=dtype)[-1]
    assert sample['step']==ref['step']
    a=sample['logits']
    order=np.argsort(a)[-5:][::-1]
    tops[key]=dict(step=int(sample['step']),selected=int(order[0]),top5=[[int(i),float(a[i])] for i in order],
                   max_abs_delta=float(np.max(np.abs(a-ref['logits']))),
                   centered_relative_l2=float(np.linalg.norm(a-ref['logits'])/
                         np.linalg.norm(ref['logits']-np.mean(ref['logits']))))
print(json.dumps(dict(samples=ranges,choices=tops),indent=2))
