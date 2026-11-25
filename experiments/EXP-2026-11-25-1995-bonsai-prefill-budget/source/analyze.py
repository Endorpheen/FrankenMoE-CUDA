"""Attribute every GPU event and partition phase wall time without double counting."""
from collections import defaultdict
from pathlib import Path
import csv
import hashlib
import json
import re
import sqlite3
import statistics
import sys


def merge(spans):
    out=[]
    for a,b in sorted(spans):
        if a>=b:
            continue
        if out and a<=out[-1][1]:
            out[-1]=(out[-1][0],max(b,out[-1][1]))
        else:
            out.append((a,b))
    return out


def duration(spans):
    return sum(b-a for a,b in merge(spans))


def clipped(spans, first, end):
    return [(max(first,a),min(end,b)) for a,b in spans if a<end and b>first]


def split(src, ranges):
    spans=merge(src)
    out=[]
    i=j=0
    while i<len(spans) and j<len(ranges):
        a,b=spans[i]
        c,d=ranges[j]
        if max(a,c)<min(b,d):
            out.append((max(a,c),min(b,d)))
        if b<d:
            i+=1
        else:
            j+=1
    return out


def complement(spans,first,end):
    out=[]
    at=first
    for a,b in merge(spans):
        if a>at:
            out.append((at,a))
        at=max(at,b)
    if end>at:
        out.append((at,end))
    return out


def stage(name,grid):
    if name=='bonsai_pq2_mmq_64x64':
        return 'PQ2 batched matrix'
    if name.startswith('goose_gdn'):
        return 'GDN'
    if name=='bonsai_decay':
        return 'F32 decay and beta'
    if name.startswith('bonsai_flash') or name.startswith('goose_q4_attention'):
        return 'Attention stage, tiles and merge'
    if name.startswith('goose_rope') or name=='goose_set_q4_rows':
        return 'RoPE and Q4 cache writes'
    if name.startswith('bonsai_pq2_matvec'):
        return 'Final vocabulary projection' if grid==62080 else 'Scalar PQ2 projections'
    if name=='bonsai_fwht1024':
        return 'FWHT'
    if name=='bonsai_pack_half':
        return 'F16 activation packing'
    if name=='bonsai_swiglu':
        return 'SwiGLU'
    if name=='bonsai_norm':
        return 'Normalization'
    if name.startswith('bonsai_conv'):
        return 'Causal convolution'
    if name=='bonsai_residual':
        return 'Residual adds'
    if name in ('bonsai_split_q_gate','bonsai_attention_gate'):
        return 'Attention split and gate'
    if name in ('bonsai_embed_batch','bonsai_pq2_row'):
        return 'Embedding'
    raise ValueError(('Unclassified kernel',name))


def read(path):
    db=sqlite3.connect(f'file:{path/"trace.sqlite"}?mode=ro',uri=True)
    ranges=db.execute('select start,end,text,globalTid from NVTX_EVENTS where end is not null order by start').fetchall()
    kernels=db.execute('''
        select k.start,k.end,s.value,k.gridX,k.globalPid
        from CUPTI_ACTIVITY_KIND_KERNEL k join StringIds s on k.shortName=s.id order by k.start
    ''').fetchall()
    pid=int(kernels[0][4]>>24)&0xffffff
    assert len({k[4] for k in kernels})==1
    mem=db.execute('select start,end,bytes,copyKind from CUPTI_ACTIVITY_KIND_MEMCPY').fetchall()
    fills=db.execute('select start,end,bytes from CUPTI_ACTIVITY_KIND_MEMSET').fetchall()
    log=(path/'trace.log').read_text()
    phase={}
    for first,end,label,tid in ranges:
        key=label.split('.')[-1]
        groups=defaultdict(lambda:{'event_ns':0,'calls':0})
        gpu=[]
        for a,b,name,grid,_ in kernels:
            if a<end and b>first:
                x=groups[stage(name,grid)]
                x['event_ns']+=min(b,end)-max(a,first)
                x['calls']+=1
                gpu.append((max(a,first),min(b,end)))
        active=merge(gpu)
        transfer=clipped([(a,b) for a,b,_,_ in mem],first,end)
        memset=clipped([(a,b) for a,b,_ in fills],first,end)
        gpu_total=merge(active+transfer+memset)
        gaps=complement(gpu_total,first,end)
        copy_detail=defaultdict(lambda:{'bytes':0,'event_ns':0,'calls':0})
        for a,b,size,kind in mem:
            if a<end and b>first:
                x=copy_detail[str(kind)]
                x['bytes']+=size
                x['event_ns']+=min(b,end)-max(a,first)
                x['calls']+=1
        api={}
        for table in ('CUPTI_ACTIVITY_KIND_RUNTIME','OSRT_API'):
            rows=db.execute(f'''
                select x.start,x.end,s.value from {table} x
                join StringIds s on x.nameId=s.id
                where x.globalTid=? and x.start<? and x.end>?
            ''',(tid,end,first)).fetchall()
            by_name=defaultdict(list)
            for a,b,n in rows:
                by_name[n].append((max(a,first),min(b,end)))
            api[table]={
                'union_ns':duration([(a,b) for spans in by_name.values() for a,b in spans]),
                'union_in_gpu_gaps_ns':duration(split([(a,b) for spans in by_name.values() for a,b in spans],gaps)),
                'by_name':{n:dict(calls=len(spans),union_ns=duration(spans),
                                  union_in_gpu_gaps_ns=duration(split(spans,gaps)))
                           for n,spans in sorted(by_name.items(),key=lambda z:-duration(z[1]))},
            }
        cpu=re.search(r'cpu_'+key.replace('model_load','load')+r'_ms=([\d.]+)',log)
        phase[key]=dict(wall_ns=end-first,cpu_process_ms=float(cpu[1]),gpu_union_ns=duration(gpu_total),
            kernel_union_ns=duration(active),copy_union_ns=duration(transfer),memset_union_ns=duration(memset),
            gpu_idle_ns=duration(gaps),kernel_groups=dict(groups),copy_by_kind=dict(copy_detail),main_thread_apis=api,
            distinct_gpu_idle_intervals=len(gaps))
        assert phase[key]['gpu_union_ns']+phase[key]['gpu_idle_ns']==end-first
    n=int(path.name)
    start,end,_,_=next(r for r in ranges if r[2]=='bonsai.prefill')
    stamps=[(0,0)]+[(int(x),float(t)) for x,t in re.findall(r'Prefill (\d+)/\d+, ([\d.]+) s',log)]
    span=(end-start)/1e9
    if stamps[-1][0]!=n:
        stamps.append((n,span))
    else:
        stamps[-1]=(n,span)
    blocks=[]
    for (a,ta),(b,tb) in zip(stamps,stamps[1:]):
        first,stop=start+round(ta*1e9),start+round(tb*1e9)
        group=defaultdict(int)
        for ks,ke,name,grid,_ in kernels:
            if ks<stop and ke>first:
                group[stage(name,grid)]+=min(ke,stop)-max(ks,first)
        blocks.append(dict(first_token=a,last_token=b,wall_ns=stop-first,kernel_ns=dict(group)))
    sm=[]
    board_peak=0
    for row in csv.reader((path/'board.csv').read_text().splitlines()):
        try:
            board_peak=max(board_peak,int(row[1]))
            if int(row[8])>=90:
                sm.append(int(row[2]))
        except (IndexError,ValueError):
            continue
    sample=[int(row[1]) for row in csv.reader((path/'process-vram.csv').read_text().splitlines())
            if len(row)==2 and row[0].strip()==str(pid) and row[1].strip().isdigit()]
    assert sample, pid
    result=json.loads((path/'resources.json').read_text())
    settings=json.loads((path/'profile-settings.json').read_text())
    home=settings['env']['HOME']
    settings['env']['HOME']='~'
    settings['command']=[s.replace(home+'/', '~/') for s in settings['command']]
    return dict(tokens=n,outputs=[json.loads(line)['token'] for line in log.splitlines() if line.startswith('{"step":')],
        phases=phase,blocks=blocks,peak_process_vram_mib=max(sample),peak_board_vram_mib=board_peak,
        high_utilization_sm_mhz=dict(samples=len(sm),min=min(sm),median=statistics.median(sm),max=max(sm)),
        wall_record=result,settings=settings,
        trace_sha256=hashlib.sha256((path/'trace.nsys-rep').read_bytes()).hexdigest())


if __name__=='__main__':
    print(json.dumps([read(Path(arg)) for arg in sys.argv[1:]],indent=2))
