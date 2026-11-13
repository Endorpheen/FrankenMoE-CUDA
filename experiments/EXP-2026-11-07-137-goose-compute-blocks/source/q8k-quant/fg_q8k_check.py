from pathlib import Path
import ctypes as C,json,random,struct,concurrent.futures
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');native=C.CDLL(str(r/'run/libggml-cpu.so.0.27.0'));goose=C.CDLL(str(r/'build/libgoose-q8k-quant.so'))
a=native.quantize_row_q8_K;b=goose.fg_selfhost_quant_q8k
for f in [a,b]:f.argtypes=[C.c_void_p,C.c_void_p,C.c_int64];f.restype=None
rng=random.Random(20261107)
def compare(values,tag):
 n=len(values);x=(C.c_float*n)(*values);size=n//256*292;prefix=64;fill=bytes((i*37+11)%256 for i in range(size+prefix*2));na=C.create_string_buffer(fill,len(fill));gb=C.create_string_buffer(fill,len(fill));before=bytes(x)
 a(x,C.byref(na,prefix),n);b(x,C.byref(gb,prefix),n)
 if na.raw!=gb.raw:
  i=next(i for i in range(len(fill)) if na.raw[i]!=gb.raw[i]);raise AssertionError((tag,n,i-prefix,na.raw[max(0,i-4):i+8].hex(),gb.raw[max(0,i-4):i+8].hex()))
 assert gb.raw[:prefix]==fill[:prefix] and gb.raw[-prefix:]==fill[-prefix:]
 assert bytes(x)==before
 return 1
cases=0
for n in [0,256,512,1024,2048,4096,16384]:
 patterns=[[0.0]*n,[-0.0]*n,[127.0]*n,[-127.0]*n,[(-1.0 if i%2 else 1.0) for i in range(n)],[(i%255-127)*0.5 for i in range(n)]]
 for p in range(12):patterns.append([rng.uniform(-10.0**(p-6),10.0**(p-6)) for _ in range(n)])
 for p,values in enumerate(patterns):cases+=compare(values,(n,p))
thread_inputs=[[rng.uniform(-3,3) for _ in range(2048)] for _ in range(256)]
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:threads=sum(pool.map(lambda v:compare(v,'thread'),thread_inputs))
report={'status':'PASS','single_cases':cases,'eight_thread_cases':threads,'full_block_bytes_equal':True,'input_unchanged':True,'canaries_unchanged':True,'zero_blocks_preserve_original_unwritten_bsums':True}
(r/'results/q8k-quant-correctness.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
