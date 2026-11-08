from pathlib import Path
import subprocess,json
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');B=A/'build/recurrent-memory';D=A/'source/recurrent-memory';rows=json.loads((A/'build/hybrid-memory/commands.json').read_text());r=next(x for x in rows if '-c' in x['argv'] and str(A/'source/hybrid-memory/hybrid.cpp') in x['argv']);args=r['argv'].copy();args[1:1]=['-I'+str(D/'patched')];args[args.index('-o')+1]=str(B/'hybrid-caller.o')
h=D/'patched/llama-memory-hybrid.h';text=h.read_text().replace('~llama_memory_hybrid() = default;','~llama_memory_hybrid();');h.write_text(text)
source=D/'hybrid-caller.cpp';source.write_text((A/'source/hybrid-memory/hybrid.cpp').read_text()+'\nllama_memory_hybrid::~llama_memory_hybrid() = default;\n');args[args.index('-c')+1]=str(source);args+=['-I'+str(A/'source/hybrid-memory')]
p=subprocess.run(args,cwd=r['cwd'],capture_output=True,text=True);(B/'caller-compile.json').write_text(json.dumps({'argv':args,'cwd':r['cwd'],'exit_code':p.returncode,'stderr':p.stderr},indent=2)+'\n');print('hybrid caller compile',p.returncode,flush=True)
if p.returncode:print(p.stderr[-5000:]);raise SystemExit(p.returncode)
objects=json.loads((B/'objects.json').read_text());objects['objects']=[x for x in objects['objects'] if not x.endswith('/hybrid-caller.o')]+[str(B/'hybrid-caller.o')];(B/'objects.json').write_text(json.dumps(objects,indent=2)+'\n')
