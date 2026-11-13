import hashlib,json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
p=root/'build/silu_gate.c';s=p.read_text()
m=re.search(r'static float goose_silu_gate_g\(float gate_g, float up_g\) \{',s)
assert m,'Expected pinned scalar ABI'
start=m.start();depth=1;i=m.end()
while depth:
 if s[i]=='{':depth+=1
 if s[i]=='}':depth-=1
 i+=1
function=s[start:i]
assert 'GS_UNREACHABLE' in function
reachable=function.split('    GS_UNREACHABLE',1)[0]
assert reachable.rstrip().endswith('return t0;')
assert set(re.findall(r'\b([A-Za-z_]\w*)\s*\(',reachable))-{'goose_silu_gate_g','float','expf'}==set()
assert not any(word in reachable for word in ['gs_','malloc','thread_local','static '][:-1])
output=reachable.replace('static float','static __device__ __forceinline__ float',1)+'}\n'
(root/'build/goose_device.cuh').write_text(output)
(root/'build/device-translation.json').write_text(json.dumps({'input_generated_c_sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'device_cuh_sha256':hashlib.sha256(output.encode()).hexdigest(),'transform':'Retain generated scalar arithmetic verbatim; add CUDA device/inline qualifiers; omit unreachable host diagnostic tail after unconditional return. No Goose runtime or host export wrapper on device.','scope':'Pinned pure-f32 scalar function only; not a general Goose CUDA compiler.'},indent=2)+'\n')
print(output)
