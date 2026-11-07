from pathlib import Path
import ctypes as C,json,random,struct,hashlib,concurrent.futures
import fg141_unpack_check as unp
r=unp.r;s=unp.s;goose=unp.goose;native=unp.native;Guarded=unp.Guarded
class Route(C.Structure):_fields_=[('slot',C.c_int32),('token',C.c_int32)]
gather=goose.fg141_gather;gather.argtypes=[C.c_int,C.c_void_p,C.c_int64,C.c_void_p]+[C.c_int64]*7+[C.c_void_p];gather.restype=None
ref_y=native.fg141_oracle_y;ref_y.argtypes=[C.c_int,C.c_void_p,C.c_int64,C.c_void_p,C.c_int64,C.c_int,C.c_int,C.c_int,C.c_int,C.c_void_p];ref_y.restype=None
micro=goose.fg141_micro;micro.argtypes=[C.c_int,C.c_void_p]+[C.c_int64]*5;micro.restype=None
ref_micro=native.fg141_oracle_micro;ref_micro.argtypes=[C.c_int,C.c_void_p]+[C.c_int]*5;ref_micro.restype=None

def inputs(count,nk,seq,first=1):
 slots=[1,2,4][seq%3];start=seq%3;stride=start+nk+2;ny=slots*5*stride;y=Guarded(ny*292);payload=bytearray(ny*292)
 for row in range(slots*5):
  for ib in range(stride):
   base=(row*stride+ib)*292;d=0.0 if seq%7==0 else (-0.03125 if (ib+row)%2 else 0.015625);struct.pack_into('<f',payload,base,d)
   payload[base+4:base+260]=bytes((i*47+row*31+ib*13+seq*7)%256 for i in range(256));q=struct.unpack_from('<256b',payload,base+4)
   for j in range(16):struct.pack_into('<h',payload,base+260+2*j,sum(q[j*16:j*16+16]))
 C.memmove(y.address,bytes(payload),len(payload));routes=(Route*(first+count+2))(*[Route((i*3+1)%(slots+2),i%5) for i in range(first+count+2)]);return y,ny,routes,slots,stride,start

def check_gather(arg):
 kind,count,nk,seq=arg;first=seq%3;y,ny,routes,slots,stride,start=inputs(count,nk,seq,first);a=Guarded(445440);b=Guarded(445440);before=y.data();mapping=bytes(routes)
 ref_y(kind,y.address,stride,routes,first,count,slots,start,nk,a.address);gather(kind,y.address,ny,routes,len(routes),first,count,slots,stride,start,nk,b.address)
 aa=a.data();bb=b.data()
 if aa!=bb:
  pos=next(i for i in range(len(aa)) if aa[i]!=bb[i]);error={'status':'FAIL_TILED_Q8_GATHER','kind':unp.names[kind],'count':count,'nk':nk,'offset':pos,'native':aa[pos:pos+24].hex(),'goose':bb[pos:pos+24].hex()};(r/'results/cpu-tiled-all-gather-failure.json').write_text(json.dumps(error,indent=2)+'\n');raise AssertionError(error)
 assert y.data()==before and bytes(routes)==mapping;y.check();a.check();b.check();return count*nk

jobs=[]
for kind in range(13):
 for count in [0,1,8,15,16,17,63,64,65,255,256]:
  for nk in ([1,2,3,8,16] if count<=16 else [1]):jobs.append((kind,count,nk,len(jobs)))
blocks=sum(check_gather(j) for j in jobs);print('gather cases',len(jobs),'blocks',blocks,flush=True)

def check_micro(arg):
 kind,nk,pattern,seq=arg;row0=[0,16,48][seq%3] if nk==1 else 0;col0=[0,16,64][(seq//3)%3] if nk==1 else 0;out0=row0+(16 if seq%2 else 0);count=row0+16;first=seq%3;start=seq%4;weights,stride=unp.weights(kind,first,count,start,nk,pattern,20261107+seq*37);reference=Guarded(445440)
 unp.reference(kind,weights.address,stride,first,start,count,nk,reference.address)
 y,ny,routes,slots,ystride,ystart=inputs(col0+16,nk,seq)
 ref_y(kind,y.address,ystride,routes,1,col0+16,slots,ystart,nk,reference.address)
 init=struct.pack('<16f',*[((i*3+seq)%31-15)*0.0078125 for i in range(16)])*4096;C.memmove(reference.address+183296,init,len(init));actual=Guarded(445440);C.memmove(actual.address,reference.address,445440);before_weights=weights.data();before_y=y.data();mapping=bytes(routes)
 for slab in range(nk):
  ref_micro(kind,reference.address,row0,col0,out0,nk,slab);micro(kind,actual.address,row0,col0,out0,nk,slab)
  aa=reference.data();bb=actual.data()
  if aa!=bb:
   pos=next(i for i in range(len(aa)) if aa[i]!=bb[i]);error={'status':'FAIL_TILED_16X16_MICRO','kind':unp.names[kind],'nk':nk,'slab':slab,'row0':row0,'col0':col0,'out0':out0,'pattern':pattern,'offset':pos,'native':aa[max(0,pos-8):pos+24].hex(),'goose':bb[max(0,pos-8):pos+24].hex()};(r/'results/cpu-tiled-all-micro-failure.json').write_text(json.dumps(error,indent=2)+'\n');raise AssertionError(error)
 assert weights.data()==before_weights and y.data()==before_y and bytes(routes)==mapping;weights.check();y.check();reference.check();actual.check();return nk
micro_jobs=[]
for kind in range(13):
 for nk in [1,2,3,5,8,16]:
  for pattern in range(5):micro_jobs.append((kind,nk,pattern,len(micro_jobs)))
slabs=sum(check_micro(j) for j in micro_jobs);print('micro cases',len(micro_jobs),'slabs',slabs,flush=True)
thread_jobs=[(i%13,1+i%4,i%5,50000+i) for i in range(104)]
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:thread_slabs=sum(pool.map(check_micro,thread_jobs))
report={'status':'PASS_ALL_13_Q8_GATHER_AND_SEVEN_NATIVE_SIMD_MICROS','gather_cases':len(jobs),'gather_blocks':blocks,'micro_cases':len(micro_jobs),'micro_slabs':slabs,'eight_thread_micro_cases':len(thread_jobs),'all_workspace_bytes_equal_native_after_each_micro':True,'inputs_canaries_unchanged':True,'coverage':'SUBBLK16/32, bias0/4/32/128, act-bias and IQ4 sign trick, with/without mins, nonzero initial accumulator, -128 activations, offset 16x16 windows and multi-slab accumulation','kernel_sha256':hashlib.sha256((s/'kernel.goose').read_bytes()).hexdigest(),'producer_sha256':'0424b7bce8ddfbf308d2208612cef01de1780ada26337ff0f556a6786f9ea364','scope':'Full unpack/repack and micro compute verified; driver/inference/sanitizers/performance pending','production_changed':False};(r/'results/cpu-tiled-all-micro-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
