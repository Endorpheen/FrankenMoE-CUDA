from pathlib import Path
import ctypes as C,json,random,struct,concurrent.futures,os
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
os.environ['GGML_GOOSE_MOE_QUANT']='1'
native=C.CDLL(str(r/'run/libggml-cpu.so.0.27.0'))
goose=C.CDLL(str(r/'variants/moe-quantized/build/libgoose-moe-activation.so'))
target=goose.fg139_quantize;target.argtypes=[C.c_int,C.c_void_p,C.c_void_p,C.c_int64];target.restype=None
refs=[native.quantize_row_q8_0,native.quantize_row_q8_K]
for f in refs:f.argtypes=[C.c_void_p,C.c_void_p,C.c_int64];f.restype=None
rng=random.Random(20261107)
counts=[0,0];values_tested=[0,0]
def check(kind,values,tag,offset=0):
 if isinstance(values,bytes):raw=values;n=len(raw)//4
 else:n=len(values);raw=bytes((C.c_float*n)(*values))
 size=(n//256*292) if kind else (n//32*34)
 prefix=64;fill=bytes((i*37+11)%256 for i in range(size+prefix*2))
 a=C.create_string_buffer(fill,len(fill));b=C.create_string_buffer(fill,len(fill))
 inp=C.create_string_buffer(b'\x5a'*(64+offset)+raw+b'\x5a'*64,128+offset+len(raw));before=inp.raw
 x=C.byref(inp,64+offset)
 refs[kind](x,C.byref(a,prefix),n);target(kind,x,C.byref(b,prefix),n)
 if a.raw!=b.raw:
  pos=next(i for i in range(len(fill)) if a.raw[i]!=b.raw[i])
  fail={'kind':kind,'tag':str(tag),'n':n,'byte':pos-prefix,'native':a.raw[max(0,pos-8):pos+16].hex(),'goose':b.raw[max(0,pos-8):pos+16].hex()}
  (r/'results/moe-activation-first-mismatch.json').write_text(json.dumps(fail,indent=2)+'\n')
  raise AssertionError(fail)
 assert inp.raw==before and b.raw[:prefix]==fill[:prefix] and b.raw[-prefix:]==fill[-prefix:]
 return n
for kind in [0,1]:
 lengths=[0,32,64,96,256,640,1024,2560,16384] if kind==0 else [0,256,512,1024,2048,4096,16384]
 for n in lengths:
  patterns=[[0.0]*n,[-0.0]*n,[127.0]*n,[-127.0]*n,[(-1.0 if i%2 else 1.0) for i in range(n)],[(i%255-127)*0.5 for i in range(n)]]
  if not kind:
   ties=[127.0,-127.0,0.5,1.5,2.5,-0.5,-1.5,-2.5,126.5,-126.5,127.5,-127.5]
   patterns.append([ties[i%len(ties)] for i in range(n)])
  for power in range(12):patterns.append([rng.uniform(-10.0**(power-6),10.0**(power-6)) for _ in range(n)])
  for index,data in enumerate(patterns):
   values_tested[kind]+=check(kind,data,(n,index),4 if index%2 else 0);counts[kind]+=1
specials=[0x7fc12345,0xffc23456,0x7f812345,0x7f800000,0xff800000,0x00000001,0x80000001,0x00800000,0x80000000]
for bits in specials:
 for pos in [0,1,7,8,15,16,23,24,31]:
  a=[0x3f800000]*32;a[pos]=bits
  values_tested[0]+=check(0,struct.pack('<32I',*a),('special',hex(bits),pos));counts[0]+=1
thread_inputs=[(i%2,[rng.uniform(-3,3) for _ in range(2560 if i%2==0 else 2048)]) for i in range(256)]
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
 totals=list(pool.map(lambda item:check(item[0],item[1],'thread',4),thread_inputs))
report={'status':'PASS_EXACT_NATIVE_Q8_0_AND_Q8_K','single_cases':{'Q8_0':counts[0],'Q8_K':counts[1]},'values_checked':{'Q8_0':values_tested[0],'Q8_K':values_tested[1]},'eight_thread_cases':256,'eight_thread_values':sum(totals),'full_block_bytes_equal':True,'inputs_and_canaries_unchanged':True,'q8_0_nonfinite_subnormal_signed_zero_nearest_even_cases':True,'q8_k_zero_block_original_bsums_preserved':True,'producer_sha256':'7ab1a1ca1cdb82bd45c447cafc9a8143cbcb23dab29bae3e813f66069c5d0846'}
(r/'results/moe-activation-correctness.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
