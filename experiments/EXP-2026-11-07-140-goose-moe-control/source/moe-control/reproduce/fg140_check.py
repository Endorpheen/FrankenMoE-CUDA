from pathlib import Path
import ctypes as C,json,random,threading,concurrent.futures,collections,sys,subprocess,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');fields=json.loads((r/'source/moe-control/fields.json').read_text())
class Plan(C.Structure):_fields_=[(s,C.c_int64) for s in fields]
class Row(C.Structure):_fields_=[('slot',C.c_int32),('token',C.c_int32)]
lib=C.CDLL(str(r/'variants/moe-control/build/libgoose-moe-control-test.so'));entry=lib.fg140_entry;entry.argtypes=[C.POINTER(Plan),C.c_int64]+[x for _ in range(6) for x in (C.c_void_p,C.c_int64)];entry.restype=None
contexts={};callbacks=[];errors=[]
def log(c,event):
 if c in contexts:contexts[c]['events'].append(event)
def quant(c,a,b,n):log(c,('quant',a,b,n))
def reset(c,e,n):
 if c in contexts:contexts[c]['counter'][e]=n
 log(c,('reset',e,n))
def barrier(c):
 if c in contexts:
  try:contexts[c]['barrier'].wait(timeout=20)
  except BaseException as e:errors.append(repr(e))
def ready(c,e):log(c,('ready',e))
def tiled(c,e,n):
 result=int(c in contexts and contexts[c]['p'].tiled_bytes>0 and e%3==1);log(c,('tiled',e,n,result));return result
def chunk(c,e,a,b,x,y,f):log(c,('chunk',e,a,b,x,y,f))
def matrix(c,w,y,o,n,rows):log(c,('matrix',w,y,o,n,rows))
def claim(c,e):
 if c not in contexts:return 0
 q=contexts[c]['counter'];old=q[e];q[e]+=1;log(c,('claim',e));return old
def stats(c,i):log(c,('stats',i))
def witness(c,k,v):log(c,('witness',k,v))
sigs=[(None,[C.c_int64]*4,quant),(None,[C.c_int64,C.c_int64,C.c_int32],reset),(None,[C.c_int64],barrier),(None,[C.c_int64]*2,ready),(C.c_uint8,[C.c_int64]*3,tiled),(None,[C.c_int64]*7,chunk),(None,[C.c_int64]*6,matrix),(C.c_int32,[C.c_int64]*2,claim),(None,[C.c_int64]*2,stats),(None,[C.c_int64]*3,witness)]
for result,args,fn in sigs:callbacks.append(C.CFUNCTYPE(result,*args)(fn))
pointers=(C.c_void_p*len(callbacks))(*[C.cast(f,C.c_void_p).value for f in callbacks]);lib.fg140_set_callbacks.argtypes=[C.c_void_p];lib.fg140_set_callbacks(pointers)
def base_plan():
 p=Plan();p.nth=1;p.experts=5;p.real_experts=5;p.ids_per_token=4;p.tokens=1;p.block_size=256;p.block_bytes=292;p.ne10=2560;p.ne11=1;p.ne12=1;p.ne13=1;p.output_rows=33;p.row_size=2920;p.cache_line=64;p.wdata_bytes=100000000;p.nb10=4;p.nb11=10240;p.nb12=10240;p.nb13=10240;p.weight_row_bytes=1234;p.nb01=1234;p.nb02=1234*33;p.dst_nb1=33*4;p.dst_nb2=33*4*4;p.dst_bytes=p.dst_nb2;p.ids_stride0=1;p.ids_stride1=4;return p
def call_layout(p):entry(C.byref(p),0,None,0,None,0,None,0,None,0,None,0,None,0)
def expected_layout(p):
 cursor=0
 def reserve(size,align):
  nonlocal cursor
  start=((p.wdata_mod+cursor+align-1)//align)*align-p.wdata_mod;cursor=start+size;return start
 if p.need_quant:reserve(p.row_size*p.ne11*p.ne12*p.ne13,8)
 offsets=[reserve(p.experts*8,8),reserve(p.experts*p.ids_per_token*p.tokens*8,8),reserve(p.experts*p.cache_line,p.cache_line)];offsets.append(reserve(p.tiled_bytes,64) if p.mode==0 else cursor);offsets.append(cursor);return offsets
if len(sys.argv)>1:
 p=base_plan();kind=sys.argv[1]
 if kind=='overflow':p.experts=0x7fffffffffffffff
 elif kind=='workspace':p.wdata_bytes=1
 elif kind=='thread':p.nth=0
 elif kind=='alignment':p.cache_line=63
 else:raise ValueError(kind)
 call_layout(p);raise AssertionError('invalid plan accepted')
rng=random.Random(20261107);layout_cases=0
for i in range(4096):
 p=base_plan();p.mode=i%3;p.need_quant=i%2;p.cache_line=64 if i%2 else 128;p.wdata_mod=i%p.cache_line;p.experts=rng.randrange(0,33);p.real_experts=p.experts;p.ids_per_token=rng.randrange(0,9);p.tokens=rng.randrange(0,65);p.ne11=rng.randrange(1,5);p.ne12=p.tokens;p.ne13=rng.randrange(1,3);p.tiled_bytes=rng.randrange(0,4097);expected=expected_layout(p);call_layout(p);actual=[p.counts_offset,p.rows_offset,p.chunks_offset,p.scratch_offset,p.total_bytes];assert actual==expected,(i,actual,expected);layout_cases+=1
control_cases=0;event_count=0;row_count=0
for mode in range(3):
 for nth in [1,2,8]:
  for tokens in [1,2,3,7,64]:
   for apex in [0,1]:
    seq=control_cases;p=base_plan();p.mode=mode;p.nth=nth;p.tokens=tokens;p.ne12=tokens;p.ne11=[1,2,4][seq%3];p.experts=3 if mode==2 else 5;p.real_experts=5;p.need_quant=seq%2;p.block_size=32 if seq%3==0 else 256;p.block_bytes=34 if p.block_size==32 else 292;p.row_size=p.ne10//p.block_size*p.block_bytes;p.nb10=4 if p.need_quant else p.block_bytes;p.nb11=(p.ne10*4 if p.need_quant else p.row_size)+64;p.nb12=p.nb11*p.ne11+32;p.nb13=p.nb12*tokens+64;p.source_contiguous=seq%2;p.output_rows=[1,17,33,257][seq%4];p.nb01=p.weight_row_bytes+(16 if seq%4==0 else 0);p.nb02=p.nb01*p.output_rows+32;p.dst_nb1=p.output_rows*4+16;p.dst_nb2=p.dst_nb1*p.ids_per_token+32;p.dst_bytes=p.dst_nb2*tokens;p.apex_math=apex;p.is_iq2s=int(seq%2==0);p.fused_available=1;p.disable_chunking=int(seq%7==0);p.tiled_bytes=4096 if seq%3==0 and not mode else 0;p.ids_stride0=1+seq%2;p.ids_stride1=p.ids_per_token*p.ids_stride0+2;p.wdata_mod=seq%64
    call_layout(p);capacity=p.ids_per_token*tokens;ni=(tokens-1)*p.ids_stride1+(p.ids_per_token-1)*p.ids_stride0+1;ids=(C.c_int32*ni)(*([-9]*ni));mask=(C.c_int32*5)(1,1,0,1,0);lut=(C.c_int32*5)(2,1,-1,0,-1);counts=(C.c_int64*(p.experts+2))(*([-91]*(p.experts+2)));rows=(Row*(p.experts*capacity+2))();C.memset(rows,0xa5,C.sizeof(rows));before_rows=bytes(rows);dst=(C.c_uint8*p.dst_bytes)(*([0xa5]*p.dst_bytes));expected_counts=[0]*p.experts;expected_rows=bytearray(before_rows);groups=[[] for _ in range(p.experts)]
    for t in range(tokens):
     for slot in range(p.ids_per_token):
      real=(slot+t)%5;ids[t*p.ids_stride1+slot*p.ids_stride0]=real
      if mode and mask[real]==0:continue
      e=lut[real] if mode==2 else real;offset=(e*capacity+expected_counts[e])*8;route=Row(slot,t);expected_rows[offset:offset+8]=bytes(route);expected_counts[e]+=1;groups[e].append((slot,t))
    inputs=(bytes(ids),bytes(mask),bytes(lut));shared={'p':p,'events':[],'counter':[0]*p.experts,'barrier':threading.Barrier(nth)};plans=[]
    for tid in range(nth):
     q=Plan.from_buffer_copy(p);q.ith=tid;q.context=seq*32+tid+1;contexts[q.context]=shared;plans.append(q)
    with concurrent.futures.ThreadPoolExecutor(max_workers=nth) as pool:
     list(pool.map(lambda q:entry(C.byref(q),1,counts,p.experts,rows,p.experts*capacity,ids,ni,mask,5,lut,5 if mode==2 else 0,dst,p.dst_bytes),plans))
    assert not errors,errors;assert list(counts)==expected_counts+[-91,-91];assert bytes(rows)==expected_rows,(seq,'row map');assert inputs==(bytes(ids),bytes(mask),bytes(lut));assert bytes(dst)==bytes([0 if mode else 0xa5])*p.dst_bytes
    expected=[]
    for tid in range(nth):
     if p.need_quant:
      first=(tid*p.ne10//p.block_size)//nth;last=((tid+1)*p.ne10//p.block_size)//nth
      for sample in range(p.ne13):
       for t in range(p.ne12):
        for slot in range(p.ne11):expected.append(('quant',sample*p.nb13+t*p.nb12+slot*p.nb11+first*p.block_size*p.nb10,sample*p.row_size*p.ne11*p.ne12+t*p.row_size*p.ne11+slot*p.row_size+first*p.block_bytes,(last-first)*p.block_size))
     for e in range(tid,p.experts,nth):expected.append(('reset',e,nth))
    for e,g in enumerate(groups):
     n=len(g)
     if not n:continue
     if not mode:
      expected += [('ready',e)]*nth
      if p.is_iq2s:expected += [('stats',12),('stats',min(3,n-1)),('stats',8+min(3,tokens-1))]
     fused=not mode and p.fused_available and p.is_iq2s and tokens in [2,3] and n in [2,3] and not p.disable_chunking
     if fused:
      expected += [('stats',14)]*nth+[('stats',4+n-1)];nc0=(p.output_rows+15)//16;nc1=1;dr0=(p.output_rows+nc0-1)//nc0;dr1=n
     else:
      delegated=not mode and p.tiled_bytes>0 and e%3==1
      if not mode:expected += [('tiled',e,n,int(delegated))]*nth
      if delegated:continue
      chunk_size=64 if p.output_rows==1 or n==1 else 16;nc0=(p.output_rows+chunk_size-1)//chunk_size;nc1=(n+chunk_size-1)//chunk_size
      if nc0*nc1<nth*4 or p.disable_chunking:nc0,nc1=(nth,1) if p.output_rows>n else (1,nth)
      dr0=(p.output_rows+nc0-1)//nc0;dr1=(n+nc1-1)//nc1
     if nth<nc0*nc1:expected += [('claim',e)]*(nc0*nc1)
     for c in range(nc0*nc1):
      a=dr0*(c%nc0);b=min(a+dr0,p.output_rows);x=dr1*(c//nc0);y=min(x+dr1,n)
      if fused or not apex:expected.append(('chunk',e,a,b,x,y,int(fused)))
      else:
       for bb1 in range(x,y,16):
        for bb0 in range(a,b,16):
         for col in range(bb1,min(bb1+16,y)):
          slot,t=g[col];si=slot%p.ne11;inp=(si+t*p.ne11)*p.row_size if p.source_contiguous or p.need_quant else si*p.nb11+t*p.nb12;out=slot*p.dst_nb1+t*p.dst_nb2+bb0*4;w=e*p.nb02+bb0*p.nb01;num=min(bb0+16,b)-bb0
          if p.nb01==p.weight_row_bytes:expected.append(('matrix',w,inp,out,p.ne10,num))
          else:
           for k in range(num):expected.append(('matrix',w+k*p.nb01,inp,out+k*4,p.ne10,1))
    actual=[e for e in shared['events'] if e[0]!='witness'];assert collections.Counter(actual)==collections.Counter(expected),(seq,collections.Counter(actual)-collections.Counter(expected),collections.Counter(expected)-collections.Counter(actual));event_count+=len(actual);row_count+=sum(expected_counts);control_cases+=1
    for q in plans:contexts.pop(q.context)
invalid={}
for name in ['overflow','workspace','thread','alignment']:
 p=subprocess.run([sys.executable,__file__,name],capture_output=True,text=True,timeout=10);assert p.returncode==1 and 'goose runtime error:' in p.stderr,(name,p.returncode,p.stdout,p.stderr);invalid[name]={'exit':p.returncode,'stderr':p.stderr.strip()}
report={'status':'PASS_COMMON_MOE_LAYOUT_GROUPING_DISPATCH_SERVICE_FIXTURE','layout_cases':layout_cases,'control_cases':control_cases,'compared_service_events':event_count,'grouped_routes':row_count,'threads':[1,2,8],'stock_cold_bank':True,'bank_real_ids_exceed_slots':True,'strided_ids':True,'inputs_and_sentinels_preserved':True,'invalid_plan_rejections':invalid,'scope':'Goose control with mocked quant/matrix/tiled/atomic/barrier services; actual native backend/model integration pending','performance_claim':False,'producer_sha256':'7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846','goose_sha256':hashlib.sha256((r/'source/moe-control/kernel.goose').read_bytes()).hexdigest()};(r/'results/moe-control-functional-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
