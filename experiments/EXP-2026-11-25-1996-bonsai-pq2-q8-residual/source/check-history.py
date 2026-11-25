"""Inspect the historical decision at a requested fixed-history step."""
import json
import os
from pathlib import Path
import subprocess
import sys

import numpy as np

build,model,case=map(Path,sys.argv[1:4])
prompt=json.loads((case/'prompt.json').read_text())['tokens']
ids=lambda name:[json.loads(l)['token'] for l in (case/(name+'.jsonl')).read_text().splitlines()]
a=ids('f16')
step=int(sys.argv[4])
assert 0<=step<len(a)
other=int(sys.argv[5])
force=case/'first-divergence-history.txt'
force.write_text('\n'.join(map(str,a[:step]))+'\n')
env=dict(PATH='/usr/bin:/bin',HOME=os.environ['HOME'],LANG='C',CUDA_VISIBLE_DEVICES='0',
    BONSAI_UBATCH='1024',BONSAI_FLASH_F32='1',BONSAI_FLASH_REUSE='1',BONSAI_FLASH_ORDERED_MERGE='1',
    BONSAI_FLASH_COMPACT='1',BONSAI_FLASH_ASYNC='1',BONSAI_REPLAY_IDS=str(force))
raw={}
scores={}
for mode,tag in [('0','f16'),('1','int8')]:
    dump=case/(tag+f'-history-{step}-logits.bin')
    cmd=['taskset','0f0f',str(build/'bonsai-quality'),str(model),str(build/'pq2-int8.cubin'),
         str(build/'frozen-goose.ptx'),'36864',str(step+1),*map(str,prompt)]
    with (case/(tag+'-divergence.jsonl')).open('w') as stdout,(case/(tag+'-divergence.log')).open('w') as log:
        subprocess.run(cmd,env={**env,'BONSAI_PQ2_MMQ_INT8':mode,'BONSAI_LOGITS_FILE':str(dump)},
            stdout=stdout,stderr=log,timeout=35,check=True)
    shape=np.dtype([('step','<u4'),('values','<f4',(248320,))])
    rows=np.fromfile(dump,dtype=shape)
    assert rows[-1]['step']==step
    raw[tag]=rows[-1]['values']
    vals=raw[tag]
    top=np.argpartition(vals,-5)[-5:]
    scores[tag]=dict(selected=int(np.argmax(vals)),f16_winner_logit=float(vals[a[step]]),
        historical_int8_winner_logit=float(vals[other]),
        top5=[(int(i),float(vals[i])) for i in sorted(top,key=lambda i:-vals[i])])
assert scores['f16']['selected']==a[step]
r=dict(step_zero_based=step,shared_history_tokens=step,historical_int8_id=other,
       max_absolute_delta=float(np.max(np.abs(raw['f16']-raw['int8']))),scores=scores)
(case/f'history-{step}.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps(r,indent=2))
