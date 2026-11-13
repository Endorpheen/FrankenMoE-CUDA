from pathlib import Path
import json,struct,collections,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
cases=[json.loads(x) for x in (r/'results/elementwise-vector-parent.jsonl').read_text().splitlines()][:-1]
a=(r/'results/elementwise-vector-parent.bin').read_bytes();b=(r/'results/elementwise-vector-on.bin').read_bytes()
assert len(a)==len(b)
u_formats=[(0,0),(1,1),(2,2),(2,0),(1,0)]
b_formats=[(0,0,0),(1,1,1),(2,2,2),(2,0,2),(2,0,0),(1,0,1),(1,0,0)]
def is_nan(bits,kind):
    if kind==0:return bits&0x7f800000==0x7f800000 and bits&0x7fffff!=0
    if kind==1:return bits&0x7c00==0x7c00 and bits&0x3ff!=0
    return bits&0x7f80==0x7f80 and bits&0x7f!=0
offset=0
counts=collections.Counter();examples=[]
for case in cases:
    n=struct.unpack_from('<Q',a,offset)[0];assert struct.unpack_from('<Q',b,offset)[0]==n
    kind=(b_formats if case['mode'] else u_formats)[case['format']][-1]
    size=4 if kind==0 else 2
    fmt='<I' if kind==0 else '<H'
    for i in range(0,n,size):
        x=struct.unpack_from(fmt,a,offset+8+i)[0];y=struct.unpack_from(fmt,b,offset+8+i)[0]
        if x==y:continue
        label='both_nan_different_bits' if is_nan(x,kind) and is_nan(y,kind) else 'OTHER_MISMATCH'
        counts[(case['mode'],case['op'],case['format'],label)]+=1
        if len(examples)<16:examples.append({'case':case,'byte_in_tensor':i,'kind':kind,'expected':hex(x),'actual':hex(y),'classification':label})
    offset+=8+n
assert offset==len(a)
report={'status':'DIAGNOSIS_ONLY_NOT_EXACT_PASS','counts':[{'mode':k[0],'op':k[1],'format':k[2],'classification':k[3],'values':v} for k,v in sorted(counts.items())],'total_changed_values':sum(counts.values()),'non_nan_mismatches':sum(v for k,v in counts.items() if k[-1]=='OTHER_MISMATCH'),'examples':examples,'original_exact_failure_preserved':True,'producer_sha256':'81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f'}
(r/'results/elementwise-vector-nan-diagnosis.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='examples'},indent=2))
