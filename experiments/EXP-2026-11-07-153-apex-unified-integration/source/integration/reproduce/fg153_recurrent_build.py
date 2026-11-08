from pathlib import Path
import subprocess,json,shutil,re,hashlib,sys
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');D=A/'source/recurrent-memory';B=A/'build/recurrent-memory';D.mkdir(exist_ok=True);B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A):
 p=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6500:]);raise SystemExit(p.returncode)
 return p
patch=D/'patched'
if '--resume-cpp' not in sys.argv:
 for f in (O/'worker-3').iterdir():
  if f.is_file() and f.suffix in ['.cpp','.h','.goose','.py','.json']:shutil.copyfile(f,D/f.name)
 P=D/'inputs';P.mkdir(exist_ok=True)
 for name in ['llama-memory-recurrent.h','llama-memory-recurrent.cpp','llama-memory.cpp']:shutil.copyfile(O/'inputs'/name,P/name)
 p=D/'replay.py';s=p.read_text();old='BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))';assert old in s;s=s.replace(old,'BASE = os.path.dirname(os.path.abspath(__file__))');p.write_text(s)
 run('recurrent-pin',['python3',str(p),'record']);run('recurrent-replay',['python3',str(p),'emit'])
 patch=D/'patched'
 for f in (U/'src').glob('*.h'):
  if not (patch/f.name).exists():shutil.copyfile(f,patch/f.name)
 generated=A/'generated/recurrent-memory';generated.mkdir(exist_ok=True);producer=A/'compiler/stage3-f849233b999c';assert hashlib.sha256(producer.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
 objects=[]
 for module in ['cells','lifecycle','state']:
  src=D/('recurrent-'+module+'.goose');out=generated/(module+'.c');run('recurrent-emit-'+module,[str(producer),'-o',str(out),str(src)])
  text=out.read_text();main=text[text.rfind('static void gg_function_0('):];matches=re.findall(r'(gg_function_\d+)\(t\d+,\s*t\d+,\s*t\d+,\s*t\d+,\s*t\d+,\s*t\d+,\s*base \+ \d+\)',main);assert len(set(matches))==1,(module,matches)
  unit=D/(module+'-unit.c');unit.write_text('#include "bridge.h"\n#define main fg153_recurrent_'+module+'_unused_main\n#include "../../generated/recurrent-memory/'+module+'.c"\n#undef main\nint64_t fg153_goose_'+module+'_entry(int64_t mode,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e){return '+matches[0]+'(mode,a,b,c,d,e,0);}\n')
  obj=B/(module+'-unit.o');run('recurrent-unit-'+module,['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-fno-strict-aliasing','-Werror=implicit-function-declaration','-c',str(unit),'-o',str(obj)]);objects.append(str(obj))
else:
 objects=[str(B/(module+'-unit.o')) for module in ['cells','lifecycle','state']]
 records=json.loads((B/'commands.json').read_text())
inc=['-I'+str(patch),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
for module in ['llama-memory-recurrent','services','seam','fg153-hooks']:
 obj=B/(module+'.o');source=patch/(module+'.cpp');latest=max([source.stat().st_mtime,*[h.stat().st_mtime for h in patch.glob('*.h')]])
 if not obj.exists() or obj.stat().st_mtime<latest:run('recurrent-compile-'+module,['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-fno-strict-aliasing',*inc,'-c',str(source),'-o',str(obj)])
 else:print('reused qualified object',module,flush=True)
 objects.append(str(obj))
fixture=B/'fixture.o'
if not fixture.exists() or fixture.stat().st_mtime<max([(D/'fixture.cpp').stat().st_mtime,*[h.stat().st_mtime for h in patch.glob('*.h')]]):
 run('recurrent-fixture-compile',['/usr/bin/g++-13','-std=c++17','-O1',*inc,'-c',str(D/'fixture.cpp'),'-o',str(fixture)])
(B/'objects.json').write_text(json.dumps({'objects':objects,'fixture':str(B/'fixture.o'),'model_gate':'PENDING_COMPLETE_BATCH_A'},indent=2)+'\n');print('Real recurrent Goose/bridge/concrete class cohort compiled; integration gate pending.')
