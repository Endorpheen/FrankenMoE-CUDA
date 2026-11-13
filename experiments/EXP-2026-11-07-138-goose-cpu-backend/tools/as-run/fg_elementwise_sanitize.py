from pathlib import Path
import json,subprocess,os,struct,collections
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
source=r/'variants/elementwise'
v=r/'variants/elementwise-sanitize'
(v/'build').mkdir(parents=True,exist_ok=True)
(v/'run').mkdir(exist_ok=True)
cmds=json.loads((source/'build/commands.json').read_text())
flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
commands=[]
for name,index in [('bridge',2),('unit',3)]:
 cmd=[x.replace(str(source/'build'),str(v/'build')) for x in cmds[index] if x not in ['-O3','-DFG_ELEMENT_TRACE=1']]+flags
 p=subprocess.run(cmd,capture_output=True,text=True)
 (v/'build'/f'{name}.log').write_text(p.stdout+p.stderr)
 print(name,p.returncode,(p.stdout+p.stderr)[-1000:],flush=True)
 p.check_returncode();commands.append(cmd)
mapping={str(source/'run/libggml-cpu.so.0.27.0'):str(v/'run/libggml-cpu.so.0.27.0'),str(source/'build/bridge.o'):str(v/'build/bridge.o'),str(source/'build/goose-element.o'):str(v/'build/goose-element.o')}
link=[mapping.get(x,x) for x in cmds[-1]]+flags
p=subprocess.run(link,capture_output=True,text=True)
(v/'build/link.log').write_text(p.stdout+p.stderr)
p.check_returncode();commands.append(link)
for path in (source/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for name in ['libggml-cpu.so','libggml-cpu.so.0']:
 p=v/'run'/name
 if not p.exists() and not p.is_symlink():p.symlink_to('libggml-cpu.so.0.27.0')
fixture=[x.replace(str(source/'run'),str(v/'run')).replace(str(source/'build'),str(v/'build')) for x in json.loads((source/'build/fixture-compile.json').read_text()) if x!='-O3']+flags
fixture[0]='/usr/bin/gcc-13'
p=subprocess.run(fixture,capture_output=True,text=True)
(v/'build/fixture-compile.log').write_text(p.stdout+p.stderr)
p.check_returncode();commands.append(fixture)
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n')
runs={}
for mode in ['off','on']:
 env=dict(os.environ);env.pop('LD_PRELOAD',None)
 env.update(LD_LIBRARY_PATH=str(v/'run'),GGML_GOOSE_ELEMENTWISE='1' if mode=='on' else '0',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 p=subprocess.run([str(v/'build/elementwise-fixture'),str(r/'results'/f'elementwise-sanitized-{mode}.bin')],env=env,capture_output=True,text=True,timeout=180)
 (r/'results'/f'elementwise-sanitized-{mode}.jsonl').write_text(p.stdout)
 (r/'results'/f'elementwise-sanitized-{mode}.stderr').write_text(p.stderr)
 print(mode,p.returncode,p.stdout.splitlines()[-1:] if p.stdout else '',p.stderr[-1000:],flush=True)
 p.check_returncode();runs[mode]=[json.loads(x) for x in p.stdout.splitlines()]
metas=[json.loads(x) for x in (r/'results/elementwise-parent.jsonl').read_text().splitlines()]
datasets={m:(r/'results'/f'elementwise-sanitized-{m}.bin').read_bytes() for m in runs}
parent=(r/'results/elementwise-parent.bin').read_bytes()
u_formats=[(0,0),(1,1),(2,2),(2,0),(1,0)];b_formats=[(0,0,0),(1,1,1),(2,2,2),(2,0,2),(2,0,0),(1,0,1),(1,0,0)]
def isnan(bits,kind):
 return (bits&0x7f800000==0x7f800000 and bits&0x7fffff!=0) if kind==0 else (bits&0x7c00==0x7c00 and bits&0x3ff!=0) if kind==1 else (bits&0x7f80==0x7f80 and bits&0x7f!=0)
counts={}
for mode,data in datasets.items():
 assert len(data)==len(parent) and runs[mode]==metas
 offset=0;nan=0;other=[]
 for meta in metas[:-1]:
  n=struct.unpack_from('<Q',parent,offset)[0];assert struct.unpack_from('<Q',data,offset)[0]==n
  kind=(b_formats if meta['mode'] else u_formats)[meta['format']][-1];size=4 if kind==0 else 2;fmt='<I' if kind==0 else '<H'
  for i in range(0,n,size):
   a=struct.unpack_from(fmt,parent,offset+8+i)[0];b=struct.unpack_from(fmt,data,offset+8+i)[0]
   if a==b:continue
   if isnan(a,kind) and isnan(b,kind):nan+=1
   else:other.append({'case':meta,'byte':i,'expected':hex(a),'actual':hex(b)})
  offset+=8+n
 counts[mode]={'changed_nan_payloads':nan,'other_mismatches':len(other),'examples':other[:8]}
report={'status':'SANITIZERS_PASS_NUMERICAL_COMPARISON_PENDING' if any(c['other_mismatches'] for c in counts.values()) else 'PASS_SCOPED_ASAN_UBSAN_LSAN_WITH_RECORDED_NAN_VARIATION','runs':{m:a[-1] for m,a in runs.items()},'comparison':counts,'instrumented':'New entire Goose binary/unary modules and new C++ metadata bridge, plus fixture','uninstrumented':'Inherited CPU core/other compute modules, native rollback objects and base library','limitations':'Not TSan or complete backend ownership/security audit. Strict NaN payload equality remains unqualified.'}
(r/'results/elementwise-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
assert not any(c['other_mismatches'] for c in counts.values())
