"""Capture paired prefill state and measure the KV-only counterfactual."""
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import numpy as np

build,model,case,out=map(Path,sys.argv[1:5])
out.mkdir(exist_ok=True)
requested=sys.argv[5:] or ['fp16','residual','both']
old=json.loads((case/'history-195.json').read_text())
step=old['step_zero_based']
tokens=json.loads((case/'prompt.json').read_text())['tokens']
history=[json.loads(s)['token'] for s in (case/'f16.jsonl').read_text().splitlines()][:step]
assert len(history)==step
replay=out/'replay.txt'
replay.write_text('\n'.join(map(str,history))+'\n')
env=dict(PATH='/usr/bin:/bin',HOME=os.environ['HOME'],LANG='C',CUDA_VISIBLE_DEVICES='0',
    BONSAI_UBATCH='1024',BONSAI_FLASH_F32='1',BONSAI_FLASH_REUSE='1',BONSAI_FLASH_ORDERED_MERGE='1',
    BONSAI_FLASH_COMPACT='1',BONSAI_FLASH_ASYNC='1',BONSAI_REPLAY_IDS=str(replay))
cmd=['taskset','0f0f',str(build/'bonsai-state-audit'),str(model),str(build/'pq2-residual.cubin'),
     str(build/'frozen-goose.ptx'),'36864',str(step+1),*map(str,tokens)]
header=struct.Struct('<IIIII')
dtype=np.dtype([('step','<u4'),('values','<f4',(248320,))])


def records(path):
    with path.open('rb') as data:
        while raw:=data.read(header.size):
            kind,pos,count,layer,bytes_=header.unpack(raw)
            payload=data.read(bytes_)
            assert len(payload)==bytes_
            yield (kind,pos,count,layer,bytes_),payload


reference=out/'reference-kv.bin'
for label in requested:
    args={**env,'BONSAI_PQ2_MMQ_INT8':'0' if label=='fp16' else '1',
          'BONSAI_LOGITS_FILE':str(out/(label+'-logits.bin'))}
    if label in ('fp16','residual'):
        args['BONSAI_AUDIT_FILE']=str(out/(label+'-state.bin'))
    else:
        assert label in ('k','v','both')
        args['BONSAI_AUDIT_OVERRIDE']=label
        args['BONSAI_AUDIT_REFERENCE_KV']=str(reference)
    with (out/(label+'.log')).open('w') as stderr,(out/(label+'.jsonl')).open('w') as stdout:
        subprocess.run(cmd,env=args,stdout=stdout,stderr=stderr,timeout=60,check=True)
    if label=='fp16':
        with reference.open('wb') as f:
            for key,row in records(out/'fp16-state.bin'):
                if key[0] in (3,4):
                    f.write(header.pack(*key))
                    f.write(row)
    vals=np.fromfile(out/(label+'-logits.bin'),dtype=dtype)[-1]
    assert int(vals['step'])==step
    print(label,int(np.argmax(vals['values'])),flush=True)
