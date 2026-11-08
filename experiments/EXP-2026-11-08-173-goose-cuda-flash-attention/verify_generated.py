from pathlib import Path
import re,subprocess,hashlib,json
F=Path(__file__).resolve().parents[2];E=Path(__file__).resolve().parent;S=E/'source';B=F/'runtime/exp173/build/source-verify';B.mkdir(parents=True,exist_ok=True);C=F/'runtime/compiler/stage3-f849233b999c';H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
modules=[('vector-device','vector-functions-generated.cuh','fg175vd_'),('loaders-device','loaders-functions-generated.cuh','fg175ld_'),('staging-device','staging-functions-generated.cuh','fg175qd_'),('mma-iter-device','mma-iter-functions-generated.cuh','fg175mi_'),('mma-process-device','mma-process-functions-generated.cuh','fg175mp_'),('mma-entry-device','mma-entry-device-functions-generated.cuh','fg175me_'),('mask-fixups-device','mask-fixups-device-functions-generated.cuh','fg175fx_'),('value-prefill-device','value-prefill-functions-generated.cuh','fg175cv_'),('value-prefill-device','value-prefill-functions-generated.cuh','fg175cv_'),('host','host-qualified.c','fg175hh_')]
def bodies(text):
 found={}
 for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',text):
  if m[1]=='gg_function_0':continue
  at=m.end();depth=1
  while depth:depth+=(text[at]=='{')-(text[at]=='}');at+=1
  found[m[1]]=text[m.start():at]
 return found
rows=[]
for name,header,prefix in modules:
 out=B/(name+'.c');q=subprocess.run([str(C),'-o',str(out),str(S/(name+'.goose'))],capture_output=True,text=True);assert q.returncode==0,q.stderr;tracked=S/(name+'-generated.c');assert out.read_bytes()==tracked.read_bytes(),name+' generated C is stale';g=tracked.read_text();h=(S/header).read_text().replace(prefix,'').replace('static __device__ __forceinline__ ','static ');assert bodies(g)==bodies(h),name+' compute header differs from actual Goose emission'
 if name!='host':assert not any(x in '\n'.join(bodies(h).values())for x in ['gg_allocate(','gg_get_stack(','gg_append(','gg_ref('])
 rows.append(dict(module=name,generated_c_sha256=H(tracked),function_count=len(bodies(g))));print('PASS actual self-hosted emission and compute body',name,flush=True)
(E/'source-verify.json').write_text(json.dumps(dict(status='PASS_CURRENT_SELFHOSTED_C_AND_MECHANICAL_COMPUTE_HEADERS',compiler_sha256=H(C),modules=rows),indent=2)+'\n')
