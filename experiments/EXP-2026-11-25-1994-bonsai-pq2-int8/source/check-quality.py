"""Test free generation and full-vocabulary logits with matched histories."""
import json
import math
import os
from pathlib import Path
import runpy
import struct
import subprocess
import sys

import jinja2
import numpy as np

here=Path(__file__).resolve().parent
cases=runpy.run_path(str(here/'quality-cases.py'))['CASES']
out,build,model,metadata,fixture,tokenizer=map(Path,sys.argv[1:7])
if len(sys.argv)>7:
    selected=set(sys.argv[7:])
    cases=[c for c in cases if c['id'] in selected]
    assert len(cases)==len(selected)
out.mkdir()
meta=json.loads(metadata.read_text())['metadata']
template=jinja2.Environment().from_string(meta['tokenizer.chat_template'])
VOCAB=248320


def read_vocab(path):
    widths={0:1,1:1,2:2,3:2,4:4,5:4,6:4,7:1,10:8,11:8,12:8}
    with path.open('rb') as f:
        assert f.read(4)==b'GGUF'
        version,_,keys=struct.unpack('<IQQ',f.read(20))
        assert version==3
        def string():
            size,=struct.unpack('<Q',f.read(8))
            return f.read(size).decode()
        for _ in range(keys):
            key=string()
            kind,=struct.unpack('<I',f.read(4))
            if kind==9:
                subtype,count=struct.unpack('<IQ',f.read(12))
                if key=='tokenizer.ggml.tokens':
                    assert subtype==8 and count==VOCAB
                    return [string() for _ in range(count)]
                if subtype==8:
                    for _ in range(count):
                        length,=struct.unpack('<Q',f.read(8))
                        f.seek(length,1)
                else:
                    f.seek(widths[subtype]*count,1)
            elif kind==8:
                length,=struct.unpack('<Q',f.read(8))
                f.seek(length,1)
            else:
                f.seek(widths[kind],1)
    raise ValueError('No vocabulary')


vocab=read_vocab(fixture)
printable=set(range(33,127))|set(range(161,173))|set(range(174,256))
missing=sorted(set(range(256))-printable)
alphabet={chr(c):c for c in printable}
alphabet.update({chr(256+i):c for i,c in enumerate(missing)})
eog={248044,248046,248063,248064,248065}


def detokenize(ids):
    out=bytearray()
    for i in ids:
        if i in eog:
            continue
        symbol=vocab[i]
        if all(c in alphabet for c in symbol):
            out.extend(alphabet[c] for c in symbol)
        else:
            out.extend(symbol.encode())
    return out.decode(errors='replace')


def score(text,expected):
    text=text.strip()
    start=text.find('{')
    try:
        value,_=json.JSONDecoder().raw_decode(text[start:])
    except (ValueError,TypeError):
        return dict(json=False,correct=False)
    return dict(json=True,correct=value==expected,value=value)


def rows(path):
    dtype=np.dtype([('step','<u4'),('values','<f4',(VOCAB,))])
    return {int(r['step']):r['values'] for r in np.fromfile(path,dtype=dtype)}


def normalized(x):
    s=x-x.max()
    return s-math.log(float(np.exp(s).sum()))


def metric(a,b):
    x,y=a.astype(np.float64),b.astype(np.float64)
    err=x-y
    p,q=normalized(x),normalized(y)
    ax,bx=int(np.argmax(x)),int(np.argmax(y))
    topx=set(np.argpartition(x,-10)[-10:].tolist())
    topy=set(np.argpartition(y,-10)[-10:].tolist())
    return dict(reference_id=ax,candidate_id=bx,top1_agrees=ax==bx,top10_overlap=len(topx&topy),
                normalized_rmse=float(np.linalg.norm(err)/np.linalg.norm(x-x.mean())),
                max_absolute=float(np.max(np.abs(err))),
                kl_ref_to_candidate=float((np.exp(p)*(p-q)).sum()),
                fp16_winner_rank=1+int(np.count_nonzero(y>y[ax])))


env=dict(PATH='/usr/bin:/bin',HOME=os.environ['HOME'],LANG='C',CUDA_VISIBLE_DEVICES='0',
    BONSAI_UBATCH='1024',BONSAI_FLASH_F32='1',BONSAI_FLASH_REUSE='1',BONSAI_FLASH_ORDERED_MERGE='1',
    BONSAI_FLASH_COMPACT='1',BONSAI_FLASH_ASYNC='1')
summary=[]
for case in cases:
    base=out/case['id']
    base.mkdir()
    text=template.render(messages=[{'role':'user','content':case['prompt']}],tools=[],enable_thinking=False,
                         add_generation_prompt=True)
    result=subprocess.run([str(tokenizer),'-m',str(fixture),'-p',text,'--no-bos','--no-escape','--ids','--log-disable'],
                          capture_output=True,text=True,check=True,timeout=10)
    prompt=json.loads(result.stdout)
    assert len(prompt)>=129 and all(isinstance(x,int) and 0<=x<VOCAB for x in prompt), (case['id'],len(prompt))
    assert prompt[0]==248045, prompt[:5]
    (base/'prompt.json').write_text(json.dumps(dict(tokens=prompt,rendered=text),indent=2,ensure_ascii=False)+'\n')
    cmd=['taskset','0f0f',str(build/'bonsai-quality'),str(model),str(build/'pq2-int8.cubin'),
         str(build/'frozen-goose.ptx'),'36864']

    def run(mode, tag, budget, dump=None, replay=None):
        runtime_env={**env,'BONSAI_PQ2_MMQ_INT8':str(mode)}
        if dump:
            runtime_env['BONSAI_LOGITS_FILE']=str(dump)
        if replay:
            runtime_env['BONSAI_REPLAY_IDS']=str(replay)
        with (base/(tag+'.jsonl')).open('w') as out_file,(base/(tag+'.log')).open('w') as log:
            subprocess.run([*cmd,str(budget),*map(str,prompt)],env=runtime_env,
                  stdout=out_file,stderr=log,timeout=60,check=True)
        return [json.loads(line)['token'] for line in (base/(tag+'.jsonl')).read_text().splitlines()]

    max_budget=case['budget']
    fa=base/'f16-logits.bin'
    a=run(0,'f16',max_budget,fa)
    b=run(1,'int8',max_budget)
    force=base/'replay.txt'
    replay_steps=min(len(a),64)
    assert replay_steps>0
    force.write_text('\n'.join(map(str,a[:replay_steps-1]))+'\n')
    fb=base/'int8-replay-logits.bin'
    run(1,'int8-replay',replay_steps,fb,force)
    aa,bb=rows(fa),rows(fb)
    readings=[dict(step=i,**metric(aa[i],bb[i])) for i in sorted(aa.keys()&bb.keys())]
    assert len(readings)>0
    answers=[detokenize(x) for x in (a,b)]
    scores=[score(x,case['expected']) for x in answers]
    for tag,text in zip(('f16','int8'),answers):
        (base/(tag+'-answer.txt')).write_text(text)
    same_prefix=next((i for i,(x,y) in enumerate(zip(a,b)) if x!=y),min(len(a),len(b)))
    row=dict(case=case['id'],prompt_tokens=len(prompt),budget=max_budget,
        output_tokens=[len(a),len(b)],free_generation_ids_same=a==b,matching_prefix=same_prefix,
        scores=scores,answers=answers,
        fp16_ended_by_eog=bool(a and a[-1] in eog),int8_ended_by_eog=bool(b and b[-1] in eog),
        expected=case['expected'],teacher_forced_steps=replay_steps,readings=readings)
    summary.append(row)
    (out/'quality.json').write_text(json.dumps(summary,indent=2,ensure_ascii=False)+'\n')
    print(case['id'],len(prompt),row['output_tokens'],same_prefix,[x['correct'] for x in scores],
          'teacher top1',sum(x['top1_agrees'] for x in readings),len(readings),flush=True)
