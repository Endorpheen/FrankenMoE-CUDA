"""Independently verify MTP embedding, normalization and Q4 EH on a GPU capture."""
import json
import mmap
from pathlib import Path
import sys

import numpy as np

inventory, capture = map(Path, sys.argv[1:3])
token = int(sys.argv[3])
data = json.loads(inventory.read_text())
ts = {x["name"]:x for x in data["tensors"]}
meta = data["metadata"]
WIDTH = 5120

values = np.fromfile(capture, dtype="<f4")
assert values.size == WIDTH * 5 and np.isfinite(values).all()
hidden, embedding, concat, stem = np.split(values, [WIDTH, WIDTH * 2, WIDTH * 4])
signs = np.asarray(meta["prism.hadamard.sign_values"][:WIDTH], dtype=np.float32)


def span(mm, name, size, offset=0):
    a = data["data_offset"] + ts[name]["offset"] + offset
    return mm[a:a+size]


def norm(array, w):
    partial = np.zeros(32, dtype=np.float32)
    for chunk in array.reshape(-1,32):
        partial += chunk * chunk
    lane=np.arange(32)
    for delta in (16,8,4,2,1): partial = partial + partial[lane ^ delta]
    eps = np.float32(meta["qwen35.attention.layer_norm_rms_epsilon"])
    return (array / np.sqrt(partial[0]/np.float32(WIDTH)+eps)) * w


with Path(data["file"]).open("rb") as f,mmap.mmap(f.fileno(),0,access=mmap.ACCESS_READ) as mm:
    stride = WIDTH // 128 * 34
    q = np.frombuffer(span(mm,"token_embd.weight",stride,token*stride),dtype=np.uint8).reshape(-1,34)
    scale = q[:, :2].copy().view("<f2").astype(np.float32)
    packed = (q[:, 2:, None] >> np.asarray([0,2,4,6],dtype=np.uint8)) & 3
    x = ((packed.reshape(-1,128).astype(np.int32)-1)*scale).astype(np.float32)
    x = x.reshape(-1,1024).copy()
    for size in (1,2,4,8,16,32,64,128,256,512):
        view=x.reshape(-1,2,size)
        a,b=view[:,0].copy(),view[:,1].copy()
        view[:,0],view[:,1]=a+b,a-b
    restored = x.ravel()*np.float32(.03125)*signs
    np.testing.assert_array_equal(restored,embedding)
    e = np.frombuffer(span(mm,"blk.64.nextn.enorm.weight",WIDTH*4),dtype="<f4")
    h = np.frombuffer(span(mm,"blk.64.nextn.hnorm.weight",WIDTH*4),dtype="<f4")
    expected = np.concatenate((norm(restored,e),norm(hidden,h)))
    np.testing.assert_allclose(concat,expected,rtol=3e-6,atol=2e-6)

    name="blk.64.nextn.eh_proj.weight"
    assert ts[name]["shape"]==[10240,5120] and ts[name]["type"]==2
    rows=np.arange(WIDTH)
    stride=320*18
    expect=[]
    for row in rows:
        q=np.frombuffer(span(mm,name,stride,int(row)*stride),dtype=np.uint8).reshape(320,18)
        scale=q[:,:2].copy().view("<f2").astype(np.float64)
        packed=np.concatenate((q[:,2:]&15,q[:,2:]>>4),axis=1).astype(np.float64)-8
        expect.append(float(np.sum((packed*scale)*expected.reshape(320,32),dtype=np.float64)))
    expected_stem=np.array(expect,dtype=np.float64)
    np.testing.assert_allclose(stem[rows],expected_stem,rtol=3e-5,atol=5e-5)
    print(json.dumps({"token":token,"embedding_bit_exact":True,"sampled_rows":len(rows),
                     "input_max_abs":float(np.max(np.abs(concat-expected))),
                     "projection_max_abs":float(np.max(np.abs(stem[rows]-expected_stem))),
                     "stem_l2_relative":float(np.linalg.norm(stem[rows]-expected_stem)/np.linalg.norm(expected_stem))}))
