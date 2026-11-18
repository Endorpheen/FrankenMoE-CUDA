#!/usr/bin/env python3
"""Real-header loader failure/lifetime contracts, no graph or model inference."""
import argparse
import ctypes as C
import mmap
from pathlib import Path
import struct
from gate import Reader,B,S,I,U,Buffer,fn


def main():
    p=argparse.ArgumentParser();p.add_argument('library',type=Path);a=p.parse_args();r=Reader(a.library);l=r.lib
    fn(l,'qr_model_open',[B,C.c_uint64,I,S,S,S,S,S,I,I,S,S,S,S,S,S,S,S,B,S],I)
    fn(l,'qr_model_close',[S],I);fn(l,'qr_frame_open',[S,I,S],I);fn(l,'qr_frame_close',[S,S],I)
    path=Path('/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf')
    with path.open('rb') as file,mmap.mmap(file.fileno(),0,access=mmap.ACCESS_READ) as mm:
        buf=Buffer();get=C.pythonapi.PyObject_GetBuffer;get.argtypes=[C.py_object,C.POINTER(Buffer),C.c_int];get.restype=C.c_int
        release=C.pythonapi.PyBuffer_Release;release.argtypes=[C.POINTER(Buffer)];release.restype=None
        assert get(mm,C.byref(buf),0)==0
        try:
            original=B(C.cast(buf.buf,C.POINTER(U)),min(buf.len,64*2**20));r.check(r.parse(original,buf.len)==0,'original header')
            header=bytes(mm[:r.state.data[7]])
            owner=(I*12)();layers=(I*(128*32))();handles=(I*4)();attn=(I*14)();gdn=(I*7)();moe=(I*3)();sections=(I*4)();session=(I*6)();name=(U*64)()
            scratch=(I*(r.state.data[3]+4))()
            def open_model(capacity=512,budget=96*2**20,override=None):
                args=[r.blob,buf.buf,buf.len,r.state,r.kv,r.tensors,r.ks,r.ts,capacity,budget,S(owner,12),S(layers,len(layers)),S(handles,4),
                      S(attn,14),S(gdn,7),S(moe,3),S(sections,4),S(session,6),B(name,64),S(scratch,len(scratch))]
                if override:
                    for index,value in override.items():args[index]=value
                return l.qr_model_open(*args)
            def failed(why,**kw):
                r.check(open_model(**kw)<0,why)
                r.check(owner[0]==0 and owner[3]==owner[4]==0,'failed load retained/published contexts')
            failed('bad capacity',capacity=31);failed('bad state budget',budget=1*2**20)
            failed('short owner',override={10:S(owner,11)});failed('short layers',override={11:S(layers,31)})
            failed('short handles',override={12:S(handles,3)});failed('short config',override={13:S(attn,13)})
            failed('short scratch usage',override={19:S(scratch,4)})
            r.check(open_model()==40 and owner[0]==9191 and owner[11]==733,'valid model open')
            r.check(C.string_at(handles[3],16)==struct.pack('<4i',*sections),'model-owned RoPE bytes')
            epoch=owner[9];snapshot=bytes(owner),bytes(layers),bytes(attn)
            r.check(open_model()<0 and snapshot==(bytes(owner),bytes(layers),bytes(attn)),'double model open mutated live model')
            frame=(I*4)();other=(I*4)()
            c=l.qr_frame_open(S(owner,12),1*2**20,S(frame,4));r.check(c!=0 and owner[6]==1,'frame create')
            old_frame=(I*4)(*frame)
            r.check(l.qr_frame_open(S(owner,12),1*2**20,S(frame,4))==0 and owner[6]==1,'double frame create')
            r.check(l.qr_model_close(S(owner,12))==-1 and owner[0]==9191,'model closed with live frame')
            r.check(l.qr_frame_open(S(owner,12),1*2**20,S(other,4))!=0 and owner[6]==2,'second frame lifetime lease')
            r.check(l.qr_frame_close(S(owner,12),S(frame,4))==0 and owner[6]==1,'first frame close')
            r.check(l.qr_model_close(S(owner,12))==-1,'model closed with another live frame')
            r.check(l.qr_frame_close(S(owner,12),S(other,4))==0 and owner[6]==0,'last frame close')
            r.check(l.qr_frame_close(S(owner,12),S(frame,4))==-1,'double frame close')
            r.check(l.qr_model_close(S(owner,12))==0 and owner[0]==owner[3]==owner[4]==0,'model cleanup')
            r.check(l.qr_model_close(S(owner,12))==-1 and l.qr_frame_open(S(owner,12),1*2**20,S(frame,4))==0,'closed model reused')
            r.check(open_model()==40 and owner[9]>epoch,'reload generation')
            r.check(l.qr_frame_open(S(owner,12),1*2**20,S(frame,4))!=0,'reloaded frame')
            r.check(l.qr_frame_close(S(owner,12),S(old_frame,4))==-1 and owner[6]==1,'stale generation closed new frame')
            r.check(l.qr_frame_close(S(owner,12),S(frame,4))==0 and l.qr_model_close(S(owner,12))==0,'reload cleanup')
            def key_row(key):
                raw=key.encode();q=(U*len(raw)).from_buffer_copy(raw);index=l.gr_find_kv(r.blob,r.state,r.kv,r.ks,B(q,len(raw)));assert index>=0
                return list(r.kv.data[index*8:index*8+8])
            scalar_changes=[('qwen35moe.context_length',0),('qwen35moe.expert_used_count',65),('qwen35moe.ssm.inner_size',4097)]
            for key,value in scalar_changes:
                r.check(r.parse(original,buf.len)==0,'reset original header');row=key_row(key);mut=bytearray(header);struct.pack_into('<I',mut,row[4],value)
                r.check(r.parse(mut,buf.len)==0,'valid modified header');failed('unsupported/inconsistent model geometry')
            r.check(r.parse(original,buf.len)==0,'reset float case');row=key_row('qwen35moe.attention.layer_norm_rms_epsilon');mut=bytearray(header);struct.pack_into('<I',mut,row[4],0x7fc00000)
            r.check(r.parse(mut,buf.len)==0,'NaN metadata wire valid');failed('nonfinite model epsilon')
            # Shape products/byte counts remain valid; model role constraints
            # must reject the swapped router axes and unwind allocated contexts.
            for tensor_name,rename in [('blk.0.ffn_gate_inp.weight',False),('blk.39.ffn_gate_inp_shexp.weight',True),('output.weight',True)]:
                r.check(r.parse(original,buf.len)==0,'reset tensor case');raw=tensor_name.encode();q=(U*len(raw)).from_buffer_copy(raw);index=l.gr_find_tensor(r.blob,r.state,r.tensors,r.ts,B(q,len(raw)));assert index>=0
                row=list(r.tensors.data[index*12:index*12+12]);mut=bytearray(header)
                if rename:mut[row[0]]=ord('X')
                else:struct.pack_into('<qq',mut,row[0]+row[1]+4,row[5],row[4])
                r.check(r.parse(mut,buf.len)==0,'role failure remains a valid GGUF header');failed('bad/missing/unreferenced tensor accepted')
            r.check(r.parse(original,buf.len)==0,'reset after deep failures')
            r.check(open_model()==40,'valid reload after failures');r.check(l.qr_model_close(S(owner,12))==0,'last cleanup')
            print(f'PASS model ownership/contracts checks={r.checks} deep_failure_cleanup=1 generation_and_live_frames=1 inference_runs=0')
        finally:r.blob=None;release(C.byref(buf))


if __name__=='__main__':main()
