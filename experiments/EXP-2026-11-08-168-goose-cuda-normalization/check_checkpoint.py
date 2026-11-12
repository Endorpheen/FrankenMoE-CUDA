from pathlib import Path
import argparse,hashlib,json,re,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--output',type=Path);a=p.parse_args()
E=Path(__file__).resolve().parent;S=E/'source';F=E.parents[1];C=F/'runtime/compiler/stage3-f849233b999c'
H=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert H(C)=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
results=[]
with tempfile.TemporaryDirectory(prefix='fg168-source-check-') as d:
 d=Path(d)
 for name in ['host','device']:
  out=d/(name+'.c');q=subprocess.run([str(C),'-o',str(out),str(S/(name+'.goose'))],capture_output=True,text=True)
  assert q.returncode==0,(name,q.stdout,q.stderr)
  g=out.read_text();functions=[]
  for m in re.finditer(r'(?m)^static [^\n{]+? (gg_function_\d+)\([^;{}]*\)\s*\{',g):
   at=m.end();depth=1
   while depth:
    depth+=(g[at]=='{')-(g[at]=='}');at+=1
   functions.append(g[m.start():at])
  assert functions
  hot='\n'.join(functions)
  for forbidden in ['gg_get_stack(','gg_allocate(','gg_append(','gg_ref(']:assert forbidden not in hot,(name,forbidden)
  results.append(dict(component=name,codegen='PASS',generated_sha256=H(out),emitted_functions=len(functions),vm_allocation_calls_in_emitted_functions=0))
  if name=='host':
   q=subprocess.run(['/usr/bin/gcc-13','-std=gnu11','-fsyntax-only','-Werror=implicit-int','-Werror=implicit-function-declaration','-include',str(S/'host-ffi.h'),str(out)],capture_output=True,text=True)
   assert q.returncode==0,(q.stdout,q.stderr)
   results[-1]['strict_C_FFI_syntax']='PASS'
 recipes=json.loads((S/'recipes.json').read_text());assert [r['mode'] for r in recipes]==list(range(8))
 report=dict(status='SOURCE_CHECKPOINT_ONLY',accepted_runtime='EXP167 6dd3aae1fc5de7057eddda904f36584f5f15be3f',compiler_sha256=H(C),components=results,recipes=8,device_CUDA_build=False,GPU_correctness=False,Apex_inference=False,production_changed=False,source_sha256={str(x.relative_to(S)):H(x) for x in sorted(S.iterdir()) if x.is_file()})
 if a.output:a.output.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(report,indent=2))
