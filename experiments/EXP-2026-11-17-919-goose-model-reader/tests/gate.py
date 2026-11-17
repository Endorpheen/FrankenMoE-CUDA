#!/usr/bin/env python3
"""Independent wire parser/native GGUF oracle; candidate reader has no C++ ABI."""
import argparse
import ctypes as C
import hashlib
import json
import math
import mmap
from pathlib import Path
import struct
import sys

I,U=C.c_int64,C.c_uint8
FORMATS={0:'B',1:'b',2:'H',3:'h',4:'I',5:'i',6:'f',7:'B',10:'Q',11:'q',12:'d'}


class S(C.Structure):_fields_=[('data',C.POINTER(I)),('len',I)]
class B(C.Structure):_fields_=[('data',C.POINTER(U)),('len',I)]
class Init(C.Structure):_fields_=[('no_alloc',C.c_bool),('ctx',C.c_void_p)]
class Buffer(C.Structure):
    _fields_=[('buf',C.c_void_p),('obj',C.c_void_p),('len',C.c_ssize_t),('itemsize',C.c_ssize_t),('readonly',C.c_int),('ndim',C.c_int),
              ('format',C.c_char_p),('shape',C.POINTER(C.c_ssize_t)),('strides',C.POINTER(C.c_ssize_t)),('suboffsets',C.POINTER(C.c_ssize_t)),('internal',C.c_void_p)]


def fn(lib,name,args,result):
    f=getattr(lib,name);f.argtypes=args;f.restype=result;return f


def hash_name(raw):
    h=14695981039346656037
    for b in raw:h=((h^b)*1099511628211)&((1<<64)-1)
    return h if h<(1<<63) else h-(1<<64)


def padded(n,alignment):return (n+alignment-1)&-alignment


class Oracle:
    def __init__(self,path):
        self.lib=C.CDLL(str(path));l=self.lib
        self.open=fn(l,'gguf_init_from_file',[C.c_char_p,Init],C.c_void_p);self.close=fn(l,'gguf_free',[C.c_void_p],None)
        for name in ['gguf_get_n_kv','gguf_get_n_tensors']:fn(l,name,[C.c_void_p],I)
        for name in ['gguf_get_data_offset','gguf_get_meta_size']:fn(l,name,[C.c_void_p],C.c_size_t)
        for name in ['gguf_get_tensor_offset','gguf_get_tensor_size','gguf_get_arr_n']:fn(l,name,[C.c_void_p,I],C.c_size_t)
        for name in ['gguf_get_kv_type','gguf_get_tensor_type','gguf_get_arr_type']:fn(l,name,[C.c_void_p,I],C.c_int)
        for name in ['gguf_get_key','gguf_get_tensor_name']:fn(l,name,[C.c_void_p,I],C.c_void_p)
        fn(l,'gguf_get_tensor_ne',[C.c_void_p,I],C.POINTER(I));fn(l,'gguf_get_meta_data',[C.c_void_p,C.c_void_p],None)
        self.block=fn(l,'ggml_blck_size',[C.c_int],I);self.size=fn(l,'ggml_type_size',[C.c_int],C.c_size_t)


def reference(blob,oracle):
    version,nt,nk=struct.unpack_from('<IQQ',blob,4);at=24;kv=[];tensors=[];alignment=32
    def string():
        nonlocal at
        n=struct.unpack_from('<Q',blob,at)[0];at+=8;start=at;at+=n
        assert at<=len(blob)
        return start,n
    for i in range(nk):
        start,n=string();key=bytes(blob[start:start+n]);wire=struct.unpack_from('<I',blob,at)[0];at+=4;et=-1;count=1;payload=at
        if wire==8:payload,count=string();size=count
        elif wire==9:
            et,count=struct.unpack_from('<IQ',blob,at);at+=12;payload=at
            if et==8:
                for j in range(count):string()
                size=at-payload
            else:size=struct.calcsize('<'+FORMATS[et])*count;at+=size
        else:size=struct.calcsize('<'+FORMATS[wire]);at+=size
        kv.extend([start,n,wire,et,payload,count,size,hash_name(key)])
        if key==b'general.alignment':alignment=struct.unpack_from('<I',blob,payload)[0]
    kv_end=at
    for i in range(nt):
        start,n=string();name=bytes(blob[start:start+n]);nd=struct.unpack_from('<I',blob,at)[0];at+=4
        ne=list(struct.unpack_from('<'+'q'*nd,blob,at))+[1]*(4-nd);at+=8*nd;ty,offset=struct.unpack_from('<IQ',blob,at);at+=12
        size=(math.prod(ne)//oracle.block(ty))*oracle.size(ty)
        tensors.extend([start,n,ty,nd,*ne,offset,size,0,hash_name(name)])
    data=padded(at,alignment) if nt else at;total=0
    for i in range(nt):
        assert tensors[i*12+8]==total
        tensors[i*12+10]=data+total;total+=padded(tensors[i*12+9],alignment)
    return [919,version,nk,nt,alignment,data,total,at],kv,tensors,kv_end


class Reader:
    def __init__(self,path):
        self.lib=C.CDLL(str(path));fn(self.lib,'goose_init',[C.c_int,C.c_void_p],None)(0,None)
        fn(self.lib,'gr_parse',[B,I,S,S,S,S,S],I)
        for name in ['gr_find_kv','gr_find_tensor']:fn(self.lib,name,[B,S,S,S,B],I)
        for name in ['gr_meta_int','gr_meta_f32_bits']:fn(self.lib,name,[B,S,S,I,S],I)
        fn(self.lib,'gr_array_int',[B,S,S,I,I,S],I);fn(self.lib,'gr_string_array_index',[B,S,S,I,S],I)
        self.checks=0
    def check(self,ok,why):
        self.checks+=1
        if not ok:raise AssertionError(why)
    def parse(self,raw,file_size=None,limits=(512,4096,1024,8192)):
        if isinstance(raw,B):self.raw=None;self.blob=raw
        else:self.raw=(U*len(raw)).from_buffer_copy(raw);self.blob=B(self.raw,len(raw))
        nk,nt,ks,ts=limits;self.storage=[];views=[]
        for n in [8,nk*8,nt*12,ks,ts]:
            a=(I*(n+2))(*([-777]*(n+2)));self.storage.append(a);views.append(S(C.cast(C.byref(a,8),C.POINTER(I)),n))
        self.state,self.kv,self.tensors,self.ks,self.ts=views
        rc=self.lib.gr_parse(self.blob,self.blob.len if file_size is None else file_size,*views)
        self.check(all(a[0]==a[-1]==-777 for a in self.storage),'output guard overwritten')
        if rc:self.check(self.state.data[0]==0,'failed parse published state')
        return rc
    def compare(self,blob,oracle,native_path=None):
        expected,kv,tn,kv_end=reference(blob,oracle)
        self.check(list(self.state.data[:8])==expected,'state/reference mismatch')
        self.check(list(self.kv.data[:len(kv)])==kv,'metadata/reference mismatch')
        self.check(list(self.tensors.data[:len(tn)])==tn,'tensor/reference mismatch')
        for kind,records,slots,stride,count in [('kv',self.kv,self.ks,8,expected[2]),('tensor',self.tensors,self.ts,12,expected[3])]:
            lookup=getattr(self.lib,'gr_find_'+kind)
            for i in range(count):
                at,n=records.data[i*stride],records.data[i*stride+1];key=(U*n).from_buffer_copy(blob[at:at+n])
                self.check(lookup(self.blob,self.state,records,slots,B(key,n))==i,'name lookup mismatch')
            key=(U*7).from_buffer_copy(b'missing');self.check(lookup(self.blob,self.state,records,slots,B(key,7))==-1,'missing key')
        for i in range(expected[2]):
            r=kv[i*8:i*8+8];ty,et,p,n=r[2],r[3],r[4],r[5]
            if ty in FORMATS and ty not in (6,12):
                value=struct.unpack_from('<'+FORMATS[ty],blob,p)[0];out=(I*3)(-99,-99,-99)
                rc=self.lib.gr_meta_int(self.blob,self.state,self.kv,i,S(C.cast(C.byref(out,8),C.POINTER(I)),1))
                if ty==10 and value>(1<<63)-1:self.check(rc==-1 and list(out)==[-99]*3,'unsigned scalar overflow')
                else:self.check(rc==0 and out[1]==value and out[0]==out[2]==-99,'typed scalar accessor')
            if ty==6:
                out=(I*1)();self.check(self.lib.gr_meta_f32_bits(self.blob,self.state,self.kv,i,S(out,1))==0 and out[0]==struct.unpack_from('<I',blob,p)[0],'F32 bits accessor')
            if ty==9 and et in FORMATS and et not in (6,12):
                size=struct.calcsize('<'+FORMATS[et])
                for j in range(n):
                    out=(I*1)();value=struct.unpack_from('<'+FORMATS[et],blob,p+j*size)[0];rc=self.lib.gr_array_int(self.blob,self.state,self.kv,i,j,S(out,1))
                    self.check((rc==0 and out[0]==value) if et!=10 or value<(1<<63) else rc==-1,'typed array accessor')
            if ty==9 and et==8:
                spans=(I*(2*n+2))(*([-99]*(2*n+2)));ptr=C.cast(C.byref(spans,8),C.POINTER(I));cursor=p
                self.check(self.lib.gr_string_array_index(self.blob,self.state,self.kv,i,S(ptr,2*n))==n,'string-array indexing')
                for j in range(n):
                    size=struct.unpack_from('<Q',blob,cursor)[0];self.check((spans[1+2*j],spans[2+2*j])==(cursor+8,size),'string-array span');cursor+=8+size
                self.check(spans[0]==spans[-1]==-99,'string-array guards')
        if native_path:
            l=oracle.lib;ctx=oracle.open(str(native_path).encode(),Init(True,None));self.check(bool(ctx),'native GGUF rejected valid file')
            try:
                self.check((l.gguf_get_n_kv(ctx),l.gguf_get_n_tensors(ctx),l.gguf_get_data_offset(ctx))==(expected[2],expected[3],expected[5]),'native header')
                for i in range(expected[2]):
                    r=kv[i*8:i*8+8];self.check(C.string_at(l.gguf_get_key(ctx,i))==bytes(blob[r[0]:r[0]+r[1]]) and l.gguf_get_kv_type(ctx,i)==r[2],'native metadata key/type')
                    if r[2]==9:self.check((l.gguf_get_arr_type(ctx,i),l.gguf_get_arr_n(ctx,i))==(r[3],r[5]),'native array type/count')
                for i in range(expected[3]):
                    r=tn[i*12:i*12+12]
                    self.check(C.string_at(l.gguf_get_tensor_name(ctx,i))==bytes(blob[r[0]:r[0]+r[1]]) and l.gguf_get_tensor_type(ctx,i)==r[2] and
                               list(l.gguf_get_tensor_ne(ctx,i)[:4])==r[4:8] and (l.gguf_get_tensor_offset(ctx,i),l.gguf_get_tensor_size(ctx,i))==(r[8],r[9]),'native tensor descriptor')
                meta=(U*l.gguf_get_meta_size(ctx))();l.gguf_get_meta_data(ctx,meta)
                self.check(bytes(meta[:kv_end])==bytes(blob[:kv_end]),'native metadata serialized bytes')
            finally:oracle.close(ctx)
        return expected


def encode_value(ty,value):
    if ty==8:return struct.pack('<Q',len(value))+value
    if ty==9:
        et,items=value;return struct.pack('<IQ',et,len(items))+b''.join(encode_value(et,item) for item in items)
    return struct.pack('<'+FORMATS[ty],value)


def wire_file(kv,tensors,alignment=32,version=3):
    data=b'GGUF'+struct.pack('<IQQ',version,len(tensors),len(kv))
    for key,ty,value in kv:data+=struct.pack('<Q',len(key))+key+struct.pack('<I',ty)+encode_value(ty,value)
    offset=0
    for name,ne,ty,bytes_count in tensors:
        data+=struct.pack('<Q',len(name))+name+struct.pack('<I',len(ne))+struct.pack('<'+'q'*len(ne),*ne)+struct.pack('<IQ',ty,offset)
        offset+=padded(bytes_count,alignment)
    end=len(data);start=padded(end,alignment) if tensors else end
    return data+bytes(start-end+offset),end


def synthetic(reader,oracle,out):
    entries=[(b'general.alignment',4,64),(b'a',0,255),(b'i',1,-128),(b'q',2,65535),(b'signed16',3,-32768),
             (b'u32',4,(1<<32)-1),(b'i32',5,-(1<<31)),(b'f32',6,-0.0),(b'bool',7,1),(b'u64',10,(1<<64)-1),
             (b'i64',11,-(1<<63)),(b'f64',12,1.25),(b'string',8,b'A\0B\xe2\x98\x83'),
             (b'arrayi32',9,(5,[-3,0,42])),(b'arraystr',9,(8,[b'',b'A\0B',b'long-name'])),(b'emptyarr',9,(6,[]))]
    tensors=[(b'a',[4,2],0,32),(b'i',[256,2],12,288),(b'q',[],0,4),(b'', [0,3],24,0)]
    data,end=wire_file(entries,tensors,64);path=out/'valid-synthetic.gguf';path.write_bytes(data)
    reader.check(reader.parse(data)==0,'valid synthetic parse');reader.compare(data,oracle,path)
    reader.check(reader.parse(wire_file([],[],version=2)[0])==0,'empty GGUFv2');reader.compare(wire_file([],[],version=2)[0],oracle)
    reader.check(reader.parse(data,limits=(32,8,64,16))==0,'compact hash tables');reader.compare(data,oracle)
    original=data
    for n in sorted(set([0,1,3,4,7,8,15,16,23,24,25,31,end-1])):
        reader.check(reader.parse(original[:n],len(original))<0,'truncated header accepted')
    bad=[]
    def changed(offset,fmt,value):
        b=bytearray(original);struct.pack_into('<'+fmt,b,offset,value);return b
    bad.extend([bytearray(b'BAD!'+original[4:]),changed(4,'I',1),changed(4,'I',0x03000000),changed(8,'Q',(1<<64)-1),changed(16,'Q',1<<63)])
    bad.append(wire_file([(b'a',4,1),(b'a',4,2)],[])[0])
    bad.append(wire_file([],[(b'a',[4],0,16),(b'a',[4],0,16)])[0])
    bad.append(wire_file([(b'',4,1)],[])[0]);bad.append(wire_file([(b'A\0B',4,1)],[])[0]);bad.append(wire_file([],[(b'A\0B',[4],0,16)])[0])
    bad.append(wire_file([(b'general.alignment',4,3)],[])[0]);bad.append(wire_file([(b'general.alignment',5,32)],[])[0])
    bad.append(wire_file([(b'badbool',7,2)],[])[0]);bad.append(wire_file([(b'badbools',9,(7,[0,2]))],[])[0])
    for blob in bad:reader.check(reader.parse(blob)<0,'malformed corpus accepted')
    for limits in [(1,8,64,16),(32,1,64,16),(32,8,1,16),(32,8,64,1),(32,8,63,16)]:reader.check(reader.parse(original,limits=limits)<0,'undersized/nonpower output capacity')
    reader.check(reader.parse(original,len(original)-1)<0,'missing payload accepted')
    reader.check(reader.parse(original)==0,'restore valid tables')
    reader.ks.data[0]=(1<<63)-1
    key=(U*1).from_buffer_copy(b'x');reader.check(reader.lib.gr_find_kv(reader.blob,reader.state,reader.kv,reader.ks,B(key,1))==-1,'forged hash index')
    print(f'PASS synthetic reader checks={reader.checks} malformed_cases={len(bad)} truncations=14')


def real_file(reader,oracle,path):
    with path.open('rb') as file,mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as mm:
        buf=Buffer();get=C.pythonapi.PyObject_GetBuffer;get.argtypes=[C.py_object,C.POINTER(Buffer),C.c_int];get.restype=C.c_int
        release=C.pythonapi.PyBuffer_Release;release.argtypes=[C.POINTER(Buffer)];release.restype=None
        assert get(mm,C.byref(buf),0)==0
        view=None
        try:
            blob=B(C.cast(buf.buf,C.POINTER(U)),min(buf.len,64*2**20));reader.check(reader.parse(blob,buf.len)==0,'real file parse')
            view=memoryview(mm);state=reader.compare(view,oracle,path)
            print(f'PASS real GGUF file={path.name} kv={state[2]} tensors={state[3]} data_offset={state[5]} header_end={state[7]} checks={reader.checks} no_weight_payload_read=1')
        finally:
            if view is not None:view.release()
            reader.blob=None;release(C.byref(buf))


def synthetic_extra(reader,oracle):
    entries=[(b'a',4,1),(b'i',4,2),(b'q',4,3),(b'arr',9,(8,[b'first',b'last']))]
    tensors=[(b'a',[4],0,16),(b'i',[4],0,16),(b'q',[4],0,16)]
    data,end=wire_file(entries,tensors)
    reader.check(reader.parse(data,limits=(8,8,8,8))==0,'colliding hash-table bucket parse')
    reader.compare(data,oracle)
    reader.check(len({hash_name(k)&7 for k,_,_ in entries[:3]})==1,'collision fixture really collides')
    key=(U*1).from_buffer_copy(b'a');bucket=(hash_name(b'a')&((1<<64)-1))&(reader.ks.len-1)
    previous=reader.ks.data[bucket];reader.check(previous>0,'forgery fixture hits occupied bucket')
    reader.ks.data[bucket]=(1<<63)-1
    reader.check(reader.lib.gr_find_kv(reader.blob,reader.state,reader.kv,reader.ks,B(key,1))==-1,'forged occupied hash index accepted')
    reader.ks.data[bucket]=previous;reader.check(reader.lib.gr_find_kv(reader.blob,reader.state,reader.kv,reader.ks,B(key,1))==0,'restored hash positive control')
    # A header-only borrowed span succeeds with the actual full file length;
    # access to a weight byte is impossible through this input slice.
    reader.check(reader.parse(data[:end],len(data),limits=(8,8,8,8))==0,'reader touched absent payload bytes')
    reader.compare(data[:end],oracle)
    reader.check(reader.parse(data[:end],len(data)-1,limits=(8,8,8,8))==-7,'missing final padded payload byte')
    malformed=[]
    for name,ne,ty in [(b'x'*64,[4],0),(b'x',[-1],0),(b'x',[4]*5,0),(b'x',[255],12),(b'x',[1<<62,4],24),(b'x',[1<<62],0),(b'x',[4],43)]:
        malformed.append(wire_file([],[(name,ne,ty,0)])[0])
    malformed.append(b'GGUF'+struct.pack('<IQQ',3,0,1)+struct.pack('<Q',1)+b'a'+struct.pack('<I',8)+struct.pack('<Q',(1<<64)-1))
    malformed.append(b'GGUF'+struct.pack('<IQQ',3,0,1)+struct.pack('<Q',1)+b'a'+struct.pack('<I',9)+struct.pack('<IQ',9,0))
    malformed.append(b'GGUF'+struct.pack('<IQQ',3,0,1)+struct.pack('<Q',1)+b'a'+struct.pack('<I',9)+struct.pack('<IQ',4,1<<63))
    malformed.append(b'GGUF'+struct.pack('<IQQ',3,0,1)+struct.pack('<Q',1)+b'a'+struct.pack('<I',9)+struct.pack('<IQ',8,1000000))
    # Offset and padding/accumulation overflow are checked without allocating
    # the claimed payload. The independent fixture controls the raw fields.
    def header_tensor(name,ne,ty,offset):
        return struct.pack('<Q',len(name))+name+struct.pack('<I',len(ne))+struct.pack('<'+'q'*len(ne),*ne)+struct.pack('<IQ',ty,offset)
    malformed.append(b'GGUF'+struct.pack('<IQQ',3,1,0)+header_tensor(b'x',[4],0,32))
    malformed.append(b'GGUF'+struct.pack('<IQQ',3,1,0)+header_tensor(b'x',[4],0,1<<63))
    for blob in malformed:reader.check(reader.parse(blob)<0,'extra malformed descriptor/array accepted')
    padding=b'GGUF'+struct.pack('<IQQ',3,1,0)+header_tensor(b'x',[(1<<63)-1],24,0)
    reader.check(reader.parse(padding,(1<<63)-1)<0,'signed padding overflow accepted')
    cumulative=b'GGUF'+struct.pack('<IQQ',3,2,0)+header_tensor(b'x',[1<<62],24,0)+header_tensor(b'y',[1<<62],24,1<<62)
    reader.check(reader.parse(cumulative,(1<<63)-1)<0,'signed cumulative data overflow accepted')
    reader.check(reader.parse(data)==0,'valid tables for typed failures')
    sentinel=(I*1)(-991)
    for index in [-1,4,(1<<63)-1]:reader.check(reader.lib.gr_meta_int(reader.blob,reader.state,reader.kv,index,S(sentinel,1))==-1 and sentinel[0]==-991,'invalid scalar accessor wrote output')
    reader.check(reader.lib.gr_meta_int(reader.blob,reader.state,reader.kv,3,S(sentinel,1))==-1,'array treated as scalar')
    for index in [-1,2,(1<<63)-1]:reader.check(reader.lib.gr_array_int(reader.blob,reader.state,reader.kv,3,index,S(sentinel,1))==-1 and sentinel[0]==-991,'string array treated as integer')
    spans=(I*4)(*([-991]*4));reader.check(reader.lib.gr_string_array_index(reader.blob,reader.state,reader.kv,3,S(spans,3))==-1 and list(spans)==[-991]*4,'short string-index output partial write')
    cutoff=reader.kv.data[3*8+4]+reader.kv.data[3*8+6]-1
    reader.check(reader.lib.gr_string_array_index(B(reader.blob.data,cutoff),reader.state,reader.kv,3,S(spans,4))==-1 and list(spans)==[-991]*4,'truncated string-index input partial write')
    before=[bytes(a) for a in reader.storage]
    reader.check(reader.lib.gr_parse(reader.blob,len(data),S(reader.state.data,7),reader.kv,reader.tensors,reader.ks,reader.ts)==-1,'short parser state accepted')
    reader.check(before==[bytes(a) for a in reader.storage],'short parser state modified buffers')
    print(f'PASS extra reader contracts checks={reader.checks} extra_malformed={len(malformed)+2} occupied_hash_corruption=1 header_only_span=1')


def main():
    p=argparse.ArgumentParser();p.add_argument('library',type=Path);p.add_argument('--native',type=Path,required=True);p.add_argument('--case',choices=['synthetic','synthetic-extra','apex','edge-skeleton','edge-ple'],required=True);p.add_argument('--constant-hash',action='store_true');a=p.parse_args()
    if a.constant_hash:
        global hash_name
        hash_name=lambda raw:1
    reader,oracle=Reader(a.library),Oracle(a.native)
    if a.case=='synthetic':synthetic(reader,oracle,a.library.parent)
    elif a.case=='synthetic-extra':synthetic_extra(reader,oracle)
    else:
        root=Path('/home/end0/AI/LocalAI/Models')
        path={'apex':root/'Apex-35B-V1.0-Q4_K_M.gguf','edge-skeleton':root/'Edge0-120B-q3k/model-skeleton.gguf','edge-ple':root/'Edge0-120B-q3k/ple-fp8.gguf'}[a.case]
        before=path.stat();real_file(reader,oracle,path);after=path.stat();reader.check((before.st_size,before.st_mtime_ns,before.st_ino)==(after.st_size,after.st_mtime_ns,after.st_ino),'file changed')


if __name__=='__main__':main()
