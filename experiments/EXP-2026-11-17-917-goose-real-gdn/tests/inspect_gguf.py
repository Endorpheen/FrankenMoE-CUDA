#!/usr/bin/env python3
"""Read only GGUF headers and small scalar weights; preserve a bounded receipt."""
import hashlib
import json
from pathlib import Path
import struct
import sys

model, out = map(Path, sys.argv[1:3])
proof = json.loads(Path(sys.argv[3]).read_text())
st = model.stat()
assert proof["result"] == "PASS" and (st.st_size,st.st_mtime_ns,st.st_ino) == (proof["bytes"],proof["mtime_ns"],proof["inode"])
formats = {0:"B",1:"b",2:"H",3:"h",4:"I",5:"i",6:"f",7:"?",10:"Q",11:"q",12:"d"}
with model.open("rb") as f:
    def read(n):
        v=f.read(n)
        assert len(v)==n,"truncated header"
        return v
    def scalar(fmt):return struct.unpack("<"+fmt,read(struct.calcsize("<"+fmt)))[0]
    def string():
        n=scalar("Q");assert n<64*2**20
        return read(n).decode("utf-8")
    def value(t,keep):
        if t in formats:return scalar(formats[t])
        if t==8:return string()
        assert t==9
        et,n=scalar("I"),scalar("Q");assert n<2**24
        if et in formats and not keep:
            f.seek(struct.calcsize("<"+formats[et])*n,1);return None
        result=[]
        for i in range(n):
            v=value(et,keep)
            if keep:result.append(v)
        return result
    assert read(4)==b"GGUF" and scalar("I")==3
    nt,nk=scalar("Q"),scalar("Q");meta={}
    for i in range(nk):
        k=string();t=scalar("I");v=value(t,k.startswith("qwen35moe.") or k=="general.alignment")
        if k.startswith(("qwen35moe.","general.architecture")) or k=="general.alignment":meta[k]=v
    tensors=[]
    for i in range(nt):
        name=string();nd=scalar("I");assert nd<=4
        ne=[scalar("Q") for _ in range(nd)];ty=scalar("I");offset=scalar("Q")
        if name.startswith("blk.0."):tensors.append({"name":name,"shape":ne,"type":ty,"offset":offset})
    alignment=meta.get("general.alignment",32);data=(f.tell()+alignment-1)//alignment*alignment
    for t in tensors:
        if t["name"]=="blk.0.ssm_a":
            assert t["type"]==0 and len(t["shape"])==1
            f.seek(data+t["offset"]);b=read(t["shape"][0]*4);v=struct.unpack("<"+"f"*t["shape"][0],b)
            t["minimum"],t["maximum"],t["sha256"]=min(v),max(v),hashlib.sha256(b).hexdigest()
            assert max(v)<0,"ssm_a is not the expected pre-transformed negative A"
receipt={"model":str(model),"model_sha256_reused":proof["sha256"],"stat":{"bytes":st.st_size,"mtime_ns":st.st_mtime_ns,"inode":st.st_ino},"metadata":meta,"data_offset":data,"layer0_tensors":tensors}
with out.open("x") as f:json.dump(receipt,f,indent=2);f.write("\n")
print("PASS GGUF metadata and negative A; full model not loaded")
for t in tensors:
    if not t["name"].startswith("blk.0.ffn"):print(t)
