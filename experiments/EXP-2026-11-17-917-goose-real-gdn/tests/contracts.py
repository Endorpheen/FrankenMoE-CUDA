#!/usr/bin/env python3
"""Independent F32 config and host snapshot checks; no GGML/model tensors."""
import ctypes as C
import struct
import sys

I,F=C.c_int64,C.c_float


class S(C.Structure):
    _fields_=[('data',C.POINTER(I)),('len',I)]


class V(C.Structure):
    _fields_=[('data',C.POINTER(F)),('len',I)]


lib=C.CDLL(sys.argv[1]);lib.goose_init.argtypes=[C.c_int,C.c_void_p];lib.goose_init(0,None)
lib.q35_gdn_config.argtypes=[I]*5+[S];lib.q35_gdn_config.restype=I
for name in ['q35_gdn_host_save','q35_gdn_host_restore']:
    fn=getattr(lib,name);fn.argtypes=[V,V,S,S,V];fn.restype=I
sqrt=C.CDLL('libm.so.6').sqrtf;sqrt.argtypes=[F];sqrt.restype=F
bits=lambda v:struct.unpack('<I',struct.pack('<f',v))[0]
fp=lambda v:C.c_float(v).value
checks=0


def check(ok,why):
    global checks
    checks+=1
    assert ok,why


for head in [1,3,7,32,64,128,256,4096]:
    for epsilon in [1e-9,1e-6,1.0,3.4028234663852886e38]:
        out=(I*9)(*([-777]*9));ptr=C.cast(C.byref(out,8),C.POINTER(I))
        check(lib.q35_gdn_config(head,1,2,4,bits(epsilon),S(ptr,7))==7,'valid config')
        expected=[head,1,2,4,bits(epsilon),bits(fp(fp(epsilon)/head)),bits(fp(1.0/sqrt(head)))]
        check(list(out)[1:8]==expected and out[0]==out[8]==-777,'native F32 parameter bits/guards')
for args in [(0,1,2,4,bits(1e-6)),(128,0,2,4,bits(1e-6)),(128,3,2,4,bits(1e-6)),(128,1,2,1,bits(1e-6)),
             *[(128,1,2,4,v) for v in [0,0x80000000,0x7f800000,0x7fc00000,1,0x100000000+bits(1e-6)]]]:
    out=(I*7)(*([-777]*7));check(lib.q35_gdn_config(*args,S(out,7))==-1 and list(out)==[-777]*7,'invalid config partial write')
cfg=(I*7)();check(lib.q35_gdn_config(4,1,2,2,bits(1e-6),S(cfg,7))==7,'small state config')
conv,state=(F*16)(*[i/16 for i in range(16)]),(F*32)(*[-i/32 for i in range(32)])
out=(F*50)(*([-99]*50));ptr=C.cast(C.byref(out,4),C.POINTER(F));meta=(I*8)()
cv,ss,config,payload=V(conv,16),V(state,32),S(cfg,7),V(ptr,48)
check(lib.q35_gdn_host_save(cv,ss,config,S(meta,8),payload)==48,'save')
original=bytes(conv)+bytes(state);check(C.string_at(ptr,48*4)==original and out[0]==out[49]==-99,'saved bits/guards')
saved_meta=list(meta)
for which in range(8):
    before=bytes(conv),bytes(state),bytes(meta),bytes(out);meta[which]+=1
    check(lib.q35_gdn_host_restore(cv,ss,config,S(meta,8),payload)==-1,'forged metadata accepted')
    check(bytes(conv)==before[0] and bytes(state)==before[1] and bytes(out)==before[3],'invalid restore wrote data')
    meta[:]=saved_meta
for fn in [lib.q35_gdn_host_save,lib.q35_gdn_host_restore]:
    for which in range(5):
        args=[cv,ss,config,S(meta,8),payload]
        args[which]=[V(conv,15),V(state,31),S(cfg,6),S(meta,7),V(ptr,47)][which]
        before=bytes(conv),bytes(state),bytes(meta),bytes(out)
        check(fn(*args)==-1,'short state buffer accepted')
        check(before==(bytes(conv),bytes(state),bytes(meta),bytes(out)),'short-buffer partial write')
for i in range(16):conv[i]=99
for i in range(32):state[i]=99
check(lib.q35_gdn_host_restore(cv,ss,config,S(meta,8),payload)==48,'restore')
check(bytes(conv)+bytes(state)==original,'restored state bits')
print(f'PASS GDN config/host contracts checks={checks} model_runs=0')
