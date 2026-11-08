from pathlib import Path
import json,os,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-167-goose-cuda-broadcast';S=E/'source';B=F/'runtime/exp167/build';R=F/'runtime/exp167/run';rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R));env.pop('LD_PRELOAD',None)
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=300);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2000:],flush=True)
 if q.returncode:print(q.stderr[-3200:]);raise SystemExit(q.returncode)
 return q
run('host-magic-probe-build',['/usr/bin/gcc-13','-std=gnu11','-O3','-fPIC','-c',S/'host-magic-probe.c','-o',B/'host-magic-probe.o'])
r=next(v for v in rows if v['label']=='cuda-broadcast-build')
for name in ['fastdiv','faults']:
 cmd=r['argv'].copy();cmd[cmd.index('-c')+1]=str(S/(name+'.cu'));cmd[cmd.index('-o')+1]=str(B/(name+'.o'));run(name+'-build',cmd,Path(r['cwd']));cmd=['/usr/bin/g++-13','-o',B/name,B/(name+'.o'),B/'cuda-broadcast.o',B/'host-goose.o']+([B/'host-magic-probe.o'] if name=='fastdiv' else [])+['-L'+str(R),'-Wl,-rpath,'+str(R),'-lggml-cuda','-lggml-base','-lggml','-lcudart','-lcublas','-ldl','-lpthread'];run(name+'-link',cmd)
a=run('full-uint32-fastdiv-correctness',[B/'fastdiv']);b=run('host-preflight',[B/'faults']);p=E/'local-results.json';d=json.loads(p.read_text());d['fastdiv']=a.stdout;d['preflight']=b.stdout;p.write_text(json.dumps(d,indent=2)+'\n')
