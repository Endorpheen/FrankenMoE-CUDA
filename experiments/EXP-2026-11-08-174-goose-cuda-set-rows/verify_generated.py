from pathlib import Path
import hashlib,json,subprocess,tempfile,re
E=Path(__file__).resolve().parent;S=E/'source';pin=json.loads((E/'source-checkpoint.json').read_text())['compiler'];C=Path(pin['compiler']);H=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest();assert H(C)==pin['sha256'];rows=[]
with tempfile.TemporaryDirectory(prefix='exp174-source-') as temp:
 for name in ['host','device']:
  p=Path(temp)/(name+'.c');q=subprocess.run([str(C),'-o',str(p),str(S/(name+'.goose'))],capture_output=True,text=True);assert q.returncode==0,q.stderr;assert p.read_bytes()==(S/(name+'-generated.c')).read_bytes();rows.append({'module':name,'source_sha256':H(S/(name+'.goose')),'generated_sha256':H(p)})
 g=(S/'host-generated.c').read_text();qualified=re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg176rs_'+m[1],g);assert qualified==(S/'host-qualified.c').read_text()
 g=(S/'device-generated.c').read_text()
 def body(m):
  at=m.end();depth=1
  while depth:depth+=(g[at]=='{')-(g[at]=='}');at+=1
  return g[m.start():at]
 funcs=['static void gg_fail(const char*){asm("trap;");}']
 for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+|gg_view|gg_double_bits|gg_index|gg_div|gg_mod|gg_udiv|gg_umod)\([^;{}]*\)\s*\{',g):
  if m[1]!='gg_function_0':funcs.append(body(m))
 text='\n'.join(x[:x.index('{')].rstrip()+';'for x in funcs)+'\n'+'\n'.join(funcs)
 text=re.sub(r'\b(gg_|ggf_|gs_)',lambda m:'fg176rd_'+m[1],text);text=re.sub(r'(?m)^static ','static __device__ __forceinline__ ',text)
 actual=(S/'device-functions-generated.cuh').read_text();assert actual=='#pragma once\n'+text+'\n#define FG176R_Q4 fg176rd_gg_function_1\n#define FG176R_F32 fg176rd_gg_function_16\n'
 assert not any(x in text for x in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref('])
 (E/'source-verification.json').write_text(json.dumps({'status':'PASS','compiler_sha256':pin['sha256'],'modules':rows,'mechanical_device_bodies_exact':True,'complete_qualified_host_c_exact':True},indent=2)+'\n')
print('PASS actual selfhosted host/device emission and compiled mechanical bodies')
