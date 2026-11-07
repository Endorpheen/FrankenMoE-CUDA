from pathlib import Path
import ctypes as C,json,random,struct,hashlib,concurrent.futures
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/cpu-tiled-all';v=r/'variants/cpu-tiled-all';goose=C.CDLL(str(v/'build/libgoose-tiled-all.so'));native=C.CDLL(str(v/'build/libnative-tiled-oracle.so'));native.fg141_oracle_init();native.fg141_oracle_workspace_size.restype=C.c_size_t;assert native.fg141_oracle_workspace_size()==445440
actual=goose.fg141_unpack;actual.argtypes=[C.c_int,C.c_void_p]+[C.c_int64]*6+[C.c_void_p];actual.restype=None
reference=native.fg141_oracle_unpack;reference.argtypes=[C.c_int,C.c_void_p,C.c_int64,C.c_int64,C.c_int64,C.c_int,C.c_int,C.c_void_p];reference.restype=None
names=['q4_K','q5_K','q6_K','q3_K','q2_K','iq4_xs','iq2_xxs','iq2_xs','iq2_s','iq3_xxs','iq3_s','iq1_s','iq1_m'];layout=json.loads((s/'block-layout.json').read_text());sub=[32,32,16,16,16,32,32,16,16,32,32,32,16];hasmin={0,1,4};act={2,3,6,7,8,9,10,11,12};total_bytes=0
class Guarded:
 def __init__(self,n,fill=0xa5):
  self.raw=C.create_string_buffer(n+256);self.address=((C.addressof(self.raw)+127)//64)*64;self.size=n;C.memset(self.address-64,fill,n+128)
 def data(self):return C.string_at(self.address,self.size)
 def check(self):assert C.string_at(self.address-64,64)==bytes([0xa5])*64 and C.string_at(self.address+self.size,64)==bytes([0xa5])*64

def weights(kind,first,count,start,nk,pattern,seed):
 size,fields=layout[names[kind]];stride=(start+nk+1)*size;nr=first+count+1;rng=random.Random(seed);data=bytearray(rng.randbytes(nr*stride))
 if pattern:data[:]=bytes([0 if pattern==1 else 255 if pattern==2 else 85 if pattern==3 else 170])*len(data)
 for row in range(nr):
  for ib in range(start+nk+1):
   b=row*stride+ib*size;d=struct.unpack('<H',struct.pack('<e',(-0.0 if pattern==1 else -0.125 if (row+ib)%2 else 0.0625)))[0]
   if kind==12:
    for j in range(4):word=struct.unpack_from('<H',data,b+48+2*j)[0];struct.pack_into('<H',data,b+48+2*j,(word&4095)|(((d>>(4*j))&15)<<12))
   else:
    struct.pack_into('<H',data,b+fields['d'],d)
    if 'dmin' in fields:struct.pack_into('<e',data,b+fields['dmin'],0.0 if pattern==1 else 0.03125)
 x=Guarded(len(data));C.memmove(x.address,bytes(data),len(data));return x,stride

def check_case(arg):
 kind,count,nk,pattern,seq=arg;first=seq%3;start=seq%4;x,stride=weights(kind,first,count,start,nk,pattern,20261107+seq*997);a=Guarded(445440);b=Guarded(445440);before=x.data();reference(kind,x.address,stride,first,start,count,nk,a.address);actual(kind,x.address,x.size,stride,first,start,count,nk,b.address);aa=a.data();bb=b.data();nb=256//sub[kind];ranges=[]
 for slab in range(nk):
  for row in range(count):
   block=row*nk+slab;di=slab*16+row;si=block*nb
   ranges.extend([(block*256,256),(65536+di*4,4),(67584+si*4,nb*4)])
   if kind in hasmin:ranges.extend([(66560+di*4,4),(83968+si*4,nb*4)])
   if kind in act:ranges.append((83968+si*4,4))
 checked=0
 for offset,length in ranges:
  if aa[offset:offset+length]!=bb[offset:offset+length]:
   bad=next(i for i in range(length) if aa[offset+i]!=bb[offset+i]);report={'status':'FAIL_TILED_UNPACK','kind':names[kind],'count':count,'nk':nk,'pattern':pattern,'first':first,'start':start,'stride':stride,'offset':offset+bad,'native':aa[max(0,offset+bad-8):offset+bad+16].hex(),'goose':bb[max(0,offset+bad-8):offset+bad+16].hex()};(r/'results/cpu-tiled-all-unpack-failure.json').write_text(json.dumps(report,indent=2)+'\n');raise AssertionError(report)
  checked+=length
 assert aa[100352:]==bb[100352:]==bytes([0xa5])*(445440-100352)
 if count==0:assert aa==bb==bytes([0xa5])*445440
 assert x.data()==before;x.check();a.check();b.check();return checked
jobs=[]
for kind in range(13):
 for count in [0,1,15,16,17,63,64,65,255,256]:
  for nk in ([1,2,3,5,8,16] if count<=16 else [1]):
   for pattern in range(5):jobs.append((kind,count,nk,pattern,len(jobs)))
checked=sum(check_case(x) for x in jobs);print('serial',len(jobs),'semantic bytes',checked,flush=True)
thread_jobs=[(i%13,16,1+i%4,i%5,50000+i) for i in range(104)]
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:thread_bytes=sum(pool.map(check_case,thread_jobs))
report={'status':'PASS_ALL_13_TILED_UNPACK_REPACK_NATIVE','serial_cases':len(jobs),'eight_thread_cases':len(thread_jobs),'semantic_bytes_compared':checked+thread_bytes,'weight_types':names,'inputs_and_canaries_preserved':True,'native_untouched_fields':'Only documented active q/d/scales, dmin/min when used, or act-bias correction compared; Goose additionally zeros SIMD-tail rows. Entire activation/acc sections unchanged.','kernel_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'producer_sha256':'0424b7bce8ddfbf308d2208612cef01de1780ada26337ff0f556a6786f9ea364','scope':'Complete weight unpack/repack only; microkernel/full tiled/model validation pending','production_changed':False,'compiler_changed':False};(r/'results/cpu-tiled-all-unpack-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
