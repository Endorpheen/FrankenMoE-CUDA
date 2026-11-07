from pathlib import Path
import struct,json,collections,re
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
root=Path('/home/end0/AI/LocalAI/Models/Qwen3.8-Flash-Next-GGUF')
manifest=json.loads((root/'download-verification.json').read_text())
header=Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml/include/ggml.h').read_text()
types={int(number):name for name,number in re.findall(r'GGML_TYPE_(\w+)\s*=\s*(\d+)',header)}
formats={0:'B',1:'b',2:'H',3:'h',4:'I',5:'i',6:'f',7:'B',10:'Q',11:'q',12:'d'}
reports=[]
for item in manifest['files']:
    if not item['file'].startswith('UD-IQ2_XS/'):continue
    path=root/item['file'];assert path.stat().st_size==item['bytes']
    with path.open('rb') as f:
        bytes_read=0
        def read(n):
            global bytes_read
            assert 0<=n<=32*1024*1024 and f.tell()+n<=min(path.stat().st_size,64*1024*1024)
            b=f.read(n);assert len(b)==n
            bytes_read+=n
            return b
        def integer(fmt):return struct.unpack('<'+fmt,read(struct.calcsize(fmt)))[0]
        def string(keep=True):
            n=integer('Q')
            if keep:return read(n).decode('utf-8')
            assert f.tell()+n<=64*1024*1024;f.seek(n,1)
        def value(kind,keep=False):
            if kind in formats:
                result=integer(formats[kind]);return result if keep else None
            if kind==8:return string(keep)
            if kind==9:
                element=integer('I');count=integer('Q');assert count<10000000
                if element in formats:
                    n=struct.calcsize(formats[element])*count
                    assert f.tell()+n<=64*1024*1024;f.seek(n,1)
                else:
                    for _ in range(count):value(element)
                return None
            raise AssertionError(kind)
        assert read(4)==b'GGUF';version=integer('I');assert version in [2,3]
        nt,nkv=integer('Q'),integer('Q');assert nt<100000 and nkv<100000
        selected={}
        for _ in range(nkv):
            key=string();kind=integer('I')
            keep=key in ['general.architecture','split.no','split.count','split.tensors.count'] or key.endswith(('.expert_count','.expert_used_count','.block_count'))
            result=value(kind,keep)
            if keep:selected[key]=result
        tensors=[];all_types=collections.Counter();experts=collections.Counter()
        for _ in range(nt):
            name=string();ndim=integer('I');assert ndim<=4
            dims=[integer('Q') for _ in range(ndim)];kind=integer('I');offset=integer('Q')
            all_types[types.get(kind,str(kind))]+=1
            if '_exps.' in name:
                experts[types.get(kind,str(kind))]+=1
                tensors.append({'name':name,'shape':dims,'type':kind,'type_name':types.get(kind,str(kind)),'offset':offset})
        reports.append({'file':item['file'],'gguf_version':version,'tensor_count':nt,'selected_metadata':selected,'all_type_counts':dict(all_types),'expert_type_counts':dict(experts),'expert_tensors':tensors,'metadata_end_offset':f.tell(),'bytes_read':bytes_read,'weight_payload_read':False})
combined=collections.Counter()
for x in reports:combined.update(x['expert_type_counts'])
out={'status':'READ_ALL_SHARD_HEADERS_ONLY','model_revision':manifest['revision'],'shards':reports,'combined_expert_type_counts':dict(combined),'weight_payload_read':False}
(r/'results/qwen-expert-type-inventory-exp139.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'expert_types':dict(combined),'shards':[{'file':x['file'],'tensor_count':x['tensor_count'],'expert_types':x['expert_type_counts'],'metadata_end':x['metadata_end_offset'],'bytes_read':x['bytes_read']} for x in reports]},indent=2))
