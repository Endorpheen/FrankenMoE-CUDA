from pathlib import Path
import json,subprocess,os
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-157-goose-context-core';B=F/'runtime/exp157/build';rows=json.loads((B/'commands.json').read_text())
def run(label,args,cwd=B):
 args=list(map(str,args));p=subprocess.run(args,cwd=cwd,env=dict(os.environ,LD_LIBRARY_PATH=str(F/'runtime/exp157/run'),GGML_NO_BACKTRACE='1'),capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=p.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,p.stdout[-1500:],flush=True)
 if p.returncode:print(p.stderr[-3000:]);raise SystemExit(p.returncode)
 return p
r=next(r for r in rows if r['label']=='context-core-build');cmd=r['argv'].copy();old=cmd[cmd.index('-o')+1];obj=B/'ggml-context-faults.o';cmd[cmd.index('-o')+1]=str(obj);cmd.insert(1,'-DFG157_TESTING');run('context-faults-build',cmd,Path(r['cwd']))
r=next(r for r in rows if r['label']=='context-core-link');cmd=[str(obj) if x==old else x for x in r['argv']];lib=B/'libggml-base-faults.so';cmd[cmd.index('-o')+1]=str(lib);run('context-faults-link',cmd,Path(r['cwd']))
run('context-fault-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include',E/'source/faults.cpp','-ldl','-o',B/'fault-fixture']);p=run('context-failure-injection',[B/'fault-fixture',lib]);presults=E/'local-results.json';d=json.loads(presults.read_text());d['allocation_failures']=p.stdout;d['failure_injection_scope']='Same production Goose and C projections;only OS allocation primitive returns are injected in a separate non-active DSO';presults.write_text(json.dumps(d,indent=2)+'\n')
