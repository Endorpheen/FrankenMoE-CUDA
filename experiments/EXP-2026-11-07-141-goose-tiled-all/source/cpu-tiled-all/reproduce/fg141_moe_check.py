from pathlib import Path
import os
os.environ['GGML_CPU_TILED_MM']='1'
import fg141_micro_check as core
import ctypes as C,json,struct,hashlib,concurrent.futures,random
r=core.r;s=core.s;goose=core.goose;native=core.native;Guarded=core.Guarded;Route=core.Route
fields=json.loads((s/'fields.json').read_text())
class Plan(C.Structure):_fields_=[(x,C.c_int64) for x in fields]
execute=goose.fg141_execute;execute.argtypes=[C.POINTER(Plan),C.c_void_p,C.c_int64,C.c_void_p,C.c_int64,C.c_void_p,C.c_int64,C.c_void_p,C.c_void_p,C.c_int64];execute.restype=C.c_int64
ref=native.fg141_oracle_moe;ref.argtypes=[C.POINTER(Plan)]+[C.c_void_p]*5;ref.restype=None
layout=goose.fg141_workspace_offset;layout.argtypes=[C.POINTER(Plan)];layout.restype=C.c_int64

def make_y(tokens,slots,stride,seed):
 n=tokens*slots*stride;raw=bytearray(n*292)
 for row in range(tokens*slots):
  for ib in range(stride):
   base=(row*stride+ib)*292;d=0.0 if seed%7==0 else (-0.03125 if (row+ib)%2 else 0.015625);struct.pack_into('<f',raw,base,d);qs=bytes((i*47+row*31+ib*13+seed*7)%256 for i in range(256));raw[base+4:base+260]=qs;values=struct.unpack('<256b',qs)
   for j in range(16):struct.pack_into('<h',raw,base+260+j*2,sum(values[j*16:j*16+16]))
 result=Guarded(len(raw));C.memmove(result.address,bytes(raw),len(raw));return result,n

def run_case(arg,parallel=False):
 kind,nroutes,width_blocks,outrows,threads,seq=arg;p=Plan();p.mode=3;p.kind=kind;p.width=width_blocks*256;p.output_rows=outrows;p.slots=[1,2,4][seq%3];p.n11=4;p.n12=max(1,(nroutes+3)//4);p.n13=1;p.y_stride=width_blocks+1;p.count=nroutes;p.enabled=1;p.forced=1;p.nth=threads;p.out_stride1=outrows+8;p.out_stride2=p.out_stride1*4+16
 weights,stride=core.unp.weights(kind,0,outrows,0,width_blocks,seq%5,20261107+seq*17);p.stride=stride;routes=(Route*max(1,nroutes))(*[Route(i%4,i//4) for i in range(nroutes)]);y,ny=make_y(p.n12,p.slots,p.y_stride,seq);noutput=p.out_stride2*p.n12;expected=Guarded(noutput*4);actual=Guarded(noutput*4);native_ws=Guarded(threads*524288);goose_ws=Guarded(threads*524288);before=(weights.data(),y.data(),bytes(routes));params=[]
 for tid in range(threads):q=Plan.from_buffer_copy(p);q.ith=tid;params.append(q)
 for q in params:ref(C.byref(q),weights.address,y.address,routes,native_ws.address,expected.address)
 def launch(q):
  before_plan=bytes(q);status=execute(C.byref(q),weights.address,weights.size,y.address,ny,routes,nroutes,goose_ws.address+q.ith*524288,actual.address,noutput);assert status==1 and bytes(q)==before_plan
 if parallel:
  with concurrent.futures.ThreadPoolExecutor(max_workers=threads) as pool:list(pool.map(launch,params))
 else:
  for q in params:launch(q)
 a=expected.data();b=actual.data()
 if a!=b:
  pos=next(i for i in range(len(a)) if a[i]!=b[i]);error={'status':'FAIL_FULL_13_TILED_MOE','kind':core.unp.names[kind],'routes':nroutes,'width_blocks':width_blocks,'output_rows':outrows,'threads':threads,'parallel':parallel,'offset':pos,'native':a[max(0,pos-8):pos+24].hex(),'goose':b[max(0,pos-8):pos+24].hex()};(r/'results/cpu-tiled-all-moe-failure.json').write_text(json.dumps(error,indent=2)+'\n');raise AssertionError(error)
 assert before==(weights.data(),y.data(),bytes(routes))
 for b in [weights,y,expected,actual,native_ws,goose_ws]:b.check()
 return noutput
jobs=[];counts=[0,1,7,8,15,16,17,63,64,255,256,257,273];widths=[1,3,5,8,10,16,17];outputs=[1,17,63,64,65,129,257]
for kind in range(13):
 for i,n in enumerate(counts):jobs.append((kind,n,widths[(kind+i)%len(widths)],outputs[(kind*3+i)%len(outputs)],[1,2,8][i%3],len(jobs)))
values=0
for i,j in enumerate(jobs):
 values+=run_case(j)
 if (i+1)%26==0:print('full tiled cases',i+1,flush=True)
parallel_jobs=[(kind,17+(kind%3)*8,1+kind%3,65,8,1000+kind) for kind in range(13)];parallel_values=sum(run_case(j,True) for j in parallel_jobs)
fallback=0;dummy=Guarded(445440);out=Guarded(64)
for kind in range(13):
 for enabled,count,forced in [(0,16,1),(1,7,0)]:
  p=Plan();p.mode=3;p.kind=kind;p.enabled=enabled;p.count=count;p.forced=forced;before=out.data();assert execute(C.byref(p),None,0,None,0,None,0,dummy.address,out.address,16)==0 and out.data()==before;fallback+=1
rng=random.Random(20261107);layout_cases=0
for i in range(1024):
 p=Plan();p.width=256*widths[i%7];p.n11=1+rng.randrange(64);p.n12=1+rng.randrange(4);p.n13=1+rng.randrange(3);p.need_quant=i%2;p.nth=[1,2,8][i%3];p.ith=i%p.nth;p.workspace_mod=i%64;converted=p.width//256*292*p.n11*p.n12*p.n13 if p.need_quant else 0;p.wdata_bytes=converted+64+p.nth*524288;expected=((p.workspace_mod+((converted+63)//64)*64+63)//64)*64-p.workspace_mod+p.ith*524288;before=bytes(p);assert layout(C.byref(p))==expected and bytes(p)==before;layout_cases+=1
report={'status':'PASS_FULL_13_FORMAT_TILED_MOE_DRIVER_NATIVE','serial_cases':len(jobs),'parallel_eight_thread_cases':len(parallel_jobs),'output_values_compared':values+parallel_values,'native_exact_outputs':True,'inputs_plan_canaries_preserved':True,'disabled_or_small_batch_fallback_cases':fallback,'workspace_layout_cases':layout_cases,'coverage':'Narrow/standard, L1-clamped extent, odd K extents,0/1/7/8/15/16/17/63/64/255/256/257/273routes,64-row groups, output tails, broadcast input slots, strided Q8 and destination, full scatter and multi-thread disjoint row ownership','producer_sha256':'0424b7bce8ddfbf308d2208612cef01de1780ada26337ff0f556a6786f9ea364','kernel_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'scope':'Complete MoE tiled computation validated against native private driver. Dense source compiled but real GGML/dense/native integration, sanitizers, models and timing still pending.','production_changed':False};(r/'results/cpu-tiled-all-moe-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
