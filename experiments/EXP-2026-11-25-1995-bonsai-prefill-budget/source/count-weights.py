"""Count actual matrix work and 2:4 compatibility in the resident PQ2 weights."""
from collections import defaultdict
import json
import math
import mmap
from pathlib import Path
import sys

import numpy as np

inventory=json.loads(Path(sys.argv[1]).read_text())
meta=inventory['metadata']
layer_count=int(meta['qwen35.block_count'])-int(meta['qwen35.nextn_predict_layers'])
group_truth=np.array([sum((value>>shift&3)!=1 for shift in (0,2,4,6))
                      for value in range(256)],dtype=np.int64)
totals=defaultdict(lambda:dict(tensors=0,weights=0,stored_bytes=0,nonzero_groups=[0]*5,code_counts=[0]*4))
with Path(inventory['file']).open('rb') as file, mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as mapping:
    for x in inventory['tensors']:
        if x['type']!=142 or not x['name'].startswith('blk.') or int(x['name'].split('.')[1])>=layer_count:
            continue
        size=math.prod(x['shape'])
        group=x['name'].split('.',2)[2]
        row=totals[group]
        row['tensors']+=1
        row['weights']+=size
        row['stored_bytes']+=size//128*34
        at=inventory['data_offset']+x['offset']
        for start in range(0,size//128,131072):
            n=min(131072,size//128-start)
            q=np.frombuffer(mapping[at+34*start:at+34*(start+n)],dtype=np.uint8).reshape(-1,34)
            zero=(q[:,0]==0)&((q[:,1]&127)==0)
            count=np.bincount(q[~zero,2:].ravel(),minlength=256)
            # A zero scale gives zero regardless of the encoded code. Count it as code 1.
            count[0x55] += int(zero.sum())*32
            for i in range(5):
                row['nonzero_groups'][i]+=int(count[group_truth==i].sum())
            for code in range(4):
                row['code_counts'][code]+=sum(int(count[(np.arange(256)>>shift&3)==code].sum())
                                             for shift in (0,2,4,6))
summary={k:sum(r[k] for r in totals.values()) for k in ('tensors','weights','stored_bytes')}
for k,n in [('nonzero_groups',5),('code_counts',4)]:
    summary[k]=[sum(r[k][i] for r in totals.values()) for i in range(n)]
assert sum(summary['code_counts'])==summary['weights']
assert sum(summary['nonzero_groups'])*4==summary['weights']
groups=sum(summary['nonzero_groups'])
summary['groups_requiring_residual']=sum(summary['nonzero_groups'][3:])
summary['minimum_residual_weights']=summary['nonzero_groups'][3]+2*summary['nonzero_groups'][4]
summary['fraction_groups_requiring_residual']=summary['groups_requiring_residual']/groups
print(json.dumps(dict(trunk=summary,by_projection=totals),indent=2))
