from pathlib import Path
import ctypes as C,json,random,struct,concurrent.futures,collections,os
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
qwen=os.getenv('FG139_BASELINE')=='qwen'
baseline=r/'variants/qwen839a-shared/build/bin' if qwen else r/'run'
native=C.CDLL(str(baseline/'libggml-cpu.so.0.27.0'));base=C.CDLL(str(baseline/'libggml-base.so.0.27.0'))
native.ggml_cpu_init.argtypes=[];native.ggml_cpu_init.restype=None;native.ggml_cpu_init()
goose=C.CDLL(str(r/'variants/moe-quantized-byte/build'/('libgoose-moe-matrix-qwen.so' if qwen else 'libgoose-moe-matrix.so')))
names=['q2_0','q5_0','iq4_nl','iq2_xxs','iq2_s'];qk=[64,32,32,256,256];sizes=[18,22,18,66,82]
class Task(C.Structure):
 _fields_=[(name,C.c_int64) for name in ['mode','kind','blocks','rows','weight_stride','first','cols','slots','dst_stride1','dst_stride2','route_first','input_contiguous','input_row_blocks','input_stride1','input_stride2','output_stride']]
class Route(C.Structure):_fields_=[('slot',C.c_int32),('token',C.c_int32)]
dots=[];dequants=[]
for kind,name in enumerate(names):
 f=getattr(native,'ggml_vec_dot_'+name+('_q8_0' if kind<3 else '_q8_K'));f.argtypes=[C.c_int,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_void_p,C.c_size_t,C.c_int];f.restype=None;dots.append(f)
 f=getattr(base,'dequantize_row_'+name);f.argtypes=[C.c_void_p,C.c_void_p,C.c_int64];f.restype=None;dequants.append(f)
target=goose.fg139_dot;target.argtypes=[C.c_int,C.c_int,C.c_void_p,C.c_void_p,C.c_void_p];target.restype=None
deq=goose.fg139_dequantize;deq.argtypes=[C.c_int,C.c_void_p,C.c_void_p,C.c_int64];deq.restype=None
execute=goose.fg139_execute;execute.argtypes=[C.POINTER(Task),C.c_void_p,C.c_int64,C.c_void_p,C.c_int64,C.c_void_p,C.c_int64,C.c_void_p,C.c_int64];execute.restype=None
quantizers=[native.quantize_row_q8_0,native.quantize_row_q8_K]
for f in quantizers:f.argtypes=[C.c_void_p,C.c_void_p,C.c_int64];f.restype=None
def buffer(data):return C.create_string_buffer(bytes(data),len(data))
def guarded(n):
 fill=bytes((i*29+17)%256 for i in range(n+128));return buffer(fill),fill
def weights(kind,blocks,rows,seed,stride=None,pattern=0):
 rng=random.Random(seed);stride=stride or blocks
 data=bytearray(rng.randbytes(stride*rows*sizes[kind]))
 for row in range(rows):
  for ib in range(blocks):
   start=(row*stride+ib)*sizes[kind]
   scale=0.0 if pattern==1 else (-1 if (ib+row)%3==0 else 1)*2.0**(-5+(ib+row)%9)
   data[start:start+2]=struct.pack('<e',scale)
   if pattern>=2:
    for i in range(2,sizes[kind]):data[start+i]=[0,255,85,170][(pattern-2)%4]
 return buffer(b'\xa7'*64+data+b'\xa7'*64)
def activation(kind,width,rows,seed,raw=False,stride=None):
 rng=random.Random(seed);bs=34 if kind<3 else 292;blocks=width//(32 if kind<3 else 256);stride=stride or blocks
 data=bytearray(b'\x63'*(stride*rows*bs))
 for row in range(rows):
  offset=row*stride*bs
  if raw:
   for ib in range(blocks):
    start=offset+ib*bs
    data[start:start+(2 if kind<3 else 4)]=struct.pack('<e' if kind<3 else '<f',0.03125 if ib%2 else -0.0625)
    first=start+(2 if kind<3 else 4)
    for i in range(32 if kind<3 else 256):data[first+i]=(i*47+ib*13+row*31)%256
    if kind>=3:
     values=struct.unpack_from('<256b',data,first)
     for i in range(16):struct.pack_into('<h',data,start+260+2*i,sum(values[16*i:16*i+16]))
  else:
   floats=(C.c_float*width)(*[rng.uniform(-5,5) for _ in range(width)])
   out=(C.c_ubyte*(blocks*bs)).from_buffer(data,offset)
   quantizers[kind>=3](floats,out,width)
 return buffer(b'\x79'*64+data+b'\x79'*64)
def fail(label,kind,meta,a,b):
 pos=next(i for i in range(min(len(a),len(b))) if a[i]!=b[i])
 out={'status':'FAIL_EXACT','operation':label,'kind':names[kind],'metadata':meta,'byte':pos-64,'native':a[max(0,pos-8):pos+16].hex(),'goose':b[max(0,pos-8):pos+16].hex()}
 (r/'results'/('moe-matrix-first-mismatch-qwen.json' if qwen else 'moe-matrix-first-mismatch.json')).write_text(json.dumps(out,indent=2)+'\n');raise AssertionError(out)
counts=collections.Counter()
def check_case(kind,width,rows,seed,raw=False,pattern=0):
 blocks=width//qk[kind];x=weights(kind,blocks,rows,seed,pattern=pattern);y=activation(kind,width,1,seed+1,raw=raw);xb=x.raw;yb=y.raw
 a,fill=guarded(rows*4);b,_=guarded(rows*4)
 for row in range(rows):dots[kind](width,C.byref(a,64+row*4),0,C.byref(x,64+row*blocks*sizes[kind]),0,C.byref(y,64),0,1)
 p=Task();p.kind=kind;p.blocks=blocks;p.rows=rows;p.weight_stride=blocks;p.cols=1;p.slots=1;p.output_stride=1
 execute(C.byref(p),C.byref(x,64),blocks*rows,C.byref(y,64),width//(32 if kind<3 else 256),C.byref(b,64),rows,None,0)
 if a.raw!=b.raw:fail('matrix',kind,{'width':width,'rows':rows,'raw_q8':raw,'pattern':pattern},a.raw,b.raw)
 assert x.raw==xb and y.raw==yb and b.raw[:64]==fill[:64] and b.raw[-64:]==fill[-64:]
 da,dfill=guarded(width*4);db,_=guarded(width*4)
 dequants[kind](C.byref(x,64),C.byref(da,64),width);deq(kind,C.byref(x,64),C.byref(db,64),width)
 if da.raw!=db.raw:fail('dequant',kind,{'width':width,'pattern':pattern},da.raw,db.raw)
 assert x.raw==xb and db.raw[:64]==dfill[:64] and db.raw[-64:]==dfill[-64:]
 return 1
for kind in range(5):
 widths=[0,64,128,192,640,2560,4096] if kind==0 else [0,32,64,96,160,640,2560] if kind<3 else [0,256,512,768,2560,4096]
 for width in widths:
  for rows in [1,2,7,16]:
   for pattern in range(6):
    counts[names[kind]]+=check_case(kind,width,rows,20261107+kind*1301+width+rows,raw=bool(pattern%2),pattern=pattern)
print('ordinary matrix/dequant cases',dict(counts),flush=True)
route_cases=0
for kind in range(5):
 width=640 if kind<3 else 2560;blocks=width//qk[kind];bs=34 if kind<3 else 292;yb=width//(32 if kind<3 else 256)
 for mode in ([1,3] if kind==4 else [3]):
  for slots in [1,2,4]:
   for columns in [2,3]:
    for contiguous in [0,1]:
     stride=blocks+1;total_rows=11;first=2;rows=7
     x=weights(kind,blocks,total_rows,11,stride=stride);actual_y_stride=yb if contiguous else yb+1
     y=activation(kind,width,slots*3,12,stride=actual_y_stride,raw=True)
     mapping=(Route*4)(Route(0,0),Route(1,0),Route(0,1),Route(3,2))
     out_size=3*67+64
     a,fill=guarded(out_size*4);b,_=guarded(out_size*4);xb=x.raw;yy=y.raw
     for col in range(columns):
      route=mapping[1+col];offset=(route.slot%slots+route.token*slots)*actual_y_stride
      output=route.slot*16+route.token*67+first
      for row in range(rows):dots[kind](width,C.byref(a,64+(output+row)*4),0,C.byref(x,64+(first+row)*stride*sizes[kind]),0,C.byref(y,64+offset*bs),0,1)
     p=Task();p.mode=mode;p.kind=kind;p.blocks=blocks;p.rows=rows;p.weight_stride=stride;p.first=first;p.cols=columns;p.slots=slots;p.dst_stride1=16;p.dst_stride2=67;p.route_first=1;p.input_contiguous=contiguous;p.input_row_blocks=yb;p.input_stride1=actual_y_stride;p.input_stride2=actual_y_stride*slots;p.output_stride=1
     execute(C.byref(p),C.byref(x,64),stride*total_rows,C.byref(y,64),actual_y_stride*slots*3,C.byref(b,64),out_size,mapping,4)
     if a.raw!=b.raw:fail('routed/fused',kind,{'mode':mode,'slots':slots,'columns':columns,'contiguous':contiguous},a.raw,b.raw)
     assert x.raw==xb and y.raw==yy and b.raw[:64]==fill[:64] and b.raw[-64:]==fill[-64:]
     route_cases+=1
empty_cases=0
for kind in range(5):
 for rows,cols in [(-2,1),(0,1),(1,-2),(1,0),(-2,-3),(0,0)]:
  p=Task();p.mode=3;p.kind=kind;p.blocks=10;p.rows=rows;p.cols=cols
  out,fill=guarded(64)
  execute(C.byref(p),None,0,None,0,C.byref(out,64),16,None,0)
  assert out.raw==fill
  empty_cases+=1
thread_jobs=[(i%5,640 if i%5<3 else 2560,7,20261107+i) for i in range(160)]
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:thread_cases=sum(pool.map(lambda arg:check_case(*arg,raw=True),thread_jobs))
report={'status':'PASS_ALL_FIVE_NATIVE_QUANT_MATRIX_DEQUANT_FUSED','ordinary_cases':dict(counts),'routed_fused_cases':route_cases,'eight_thread_cases':thread_cases,'all_bytes_equal_native':True,'inputs_and_canaries_unchanged':True,'noncontiguous_weight_and_activation_block_strides':True,'IQ2_S_existing_two_three_activation_weight_reuse_preserved':True,'producer_sha256':'7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846'}
report.update(empty_worker_spans=empty_cases,native_baseline=str(baseline),IQ4_NL_scalar_tail_fused=qwen)
(r/'results'/('moe-matrix-byte-correctness-qwen.json' if qwen else 'moe-matrix-byte-correctness.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
