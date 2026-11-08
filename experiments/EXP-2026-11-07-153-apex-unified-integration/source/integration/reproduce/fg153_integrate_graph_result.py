from pathlib import Path
import subprocess,json,hashlib,os,re
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/graph-result';B=A/'build/graph-result';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);(D/'generated').mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=150);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5500:]);raise SystemExit(p.returncode)
 return p
for module in ['graph-result','graph-input','graph-context']:
 gen=D/'generated'/(module+'.c');run('result-emit-'+module,[str(A/'compiler/stage3-f849233b999c'),'-o',str(gen),str(D/(module+'.goose'))]);s=gen.read_text();assert 'gg_function_1(' in s[s.rfind('static void gg_function_0('):];assert not re.search(r'gg_stacks\[[^\]]+\]\.',s[:s.rfind('static void gg_function_0(')])
 p=D/('goose-entry-'+module+'.c');s=p.read_text();s=re.sub(r'^\s*gg_stacks\[0\]\.top = 0;\n','',s,flags=re.M);p.write_text(s)
source=A/'source/hybrid-inputs/graph.cpp';candidate=D/'graph.cpp';run('result-source-replay',['python3',str(D/'replay.py'),'emit','--source',str(source),'--anchors',str(D/'graph-replay-anchors.json'),'--output',str(candidate)])
header=D/'graph-bridge.h';s=header.read_text()
if 'fg153_graph_result_enabled' not in s:s+='\n#ifdef __cplusplus\nextern "C" {\n#endif\nint fg153_graph_result_enabled(void);\n#ifdef __cplusplus\n}\n#endif\n';header.write_text(s)
s=candidate.read_text();s=re.sub(r'#ifndef FG153_NATIVE_ROLLBACK\n(.*?)\n#endif',lambda m:'#ifndef FG153_NATIVE_ROLLBACK\n    if(fg153_graph_result_enabled()) {\n'+m.group(1)+'\n    }\n#endif',s,flags=re.S);s=s.replace('#include "frame.hpp"','#include "'+str(A/'source/hybrid-inputs/frame.hpp')+'"');candidate.write_text(s)
flag=D/'flag.c';flag.write_text('#include <stdlib.h>\n#include <string.h>\nint fg153_graph_result_enabled(void){const char*v=getenv("GGML_GOOSE_GRAPH_RESULT");return !v||strcmp(v,"0");}\n')
incs=['-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
rows=json.loads((A/'build/hybrid-inputs/commands.json').read_text());r=next(x for x in rows if x['label']=='hybrid-input-graph');args=r['argv'].copy();args[args.index('-c')+1]=str(candidate);args[args.index('-o')+1]=str(B/'graph.o');args[1:1]=['-I'+str(D)];run('result-graph-build',args,Path(r['cwd']))
objects=[]
for f in ['goose-entry-graph-result.c','goose-entry-graph-input.c','goose-entry-graph-context.c','flag.c']:
 obj=B/(f+'.o');run('result-unit-'+f,['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-Werror=implicit-function-declaration',*incs,'-c',str(D/f),'-o',str(obj)]);objects.append(obj)
for f in ['graph-services.cpp','graph-context-adapter.cpp']:
 obj=B/(f+'.o');run('result-cpp-'+f,['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-fno-strict-aliasing',*incs,'-c',str(D/f),'-o',str(obj)]);objects.append(obj)
rows=json.loads((A/'build/batch-allocr/commands.json').read_text());r=next(x for x in rows if x['label']=='batch-cohort-link');args=r['argv'].copy();old=str(A/'build/hybrid-inputs/graph.o');assert args.count(old)==1;args=[str(B/'graph.o') if x==old else x for x in args];args += [str(x) for x in objects];fingerprint=hashlib.sha256(b''.join((Path(x) if Path(x).is_absolute() else Path(r['cwd'])/x).read_bytes() for x in args if x.endswith('.o'))).hexdigest()[:12];out=R/('libllama-apex-result-'+fingerprint+'.so.0.7.0');args[args.index('-o')+1]=str(out);run('result-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
for file,name in [('graph-fixture.cpp','owned'),('replay-probe.cpp','public')]:
 binary=B/(name+'-fixture');run('result-fixture-build-'+name,['/usr/bin/g++-13','-std=c++17','-O1',*incs,str(D/file),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lggml-cpu','-lm','-o',str(binary)])
 if name=='owned':run('result-fixture-owned',[str(binary)],env=env)
 else:
  arms=[]
  for value in ['0','1']:
   arm=env.copy();arm['GGML_GOOSE_GRAPH_RESULT']=value;p=run('result-public-'+value,[str(binary)],env=arm);arms.append(p.stdout)
  assert arms[0]==arms[1],arms
summary={'status':'ACCEPTED_GRAPH_RESULT_INPUT_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':7,'entrypoints':17,'default_on':True,'rollback':'GGML_GOOSE_GRAPH_RESULT=0','minor_parent_fix':'Removed unnecessary shared stack resets after proving scalar hot path','model_requests':0,'production_changed':False,'pending':['constructor metadata derivation integration being completed','whole Batch A Apex gate']};(A/'results/accepted-graph-result.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted graph result/input ownership with runtime rollback into current shared cohort.')
