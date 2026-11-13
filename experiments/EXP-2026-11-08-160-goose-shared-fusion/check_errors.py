from pathlib import Path
import os,json,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-160-goose-shared-fusion';B=F/'runtime/exp160/build';rows=json.loads((B/'commands.json').read_text());m=json.loads((E/'source/manifest.json').read_text())
def run(label,args):
 args=list(map(str,args));q=subprocess.run(args,cwd=B,env=dict(os.environ,LD_LIBRARY_PATH=str(F/'runtime/exp160/run'),GGML_NO_BACKTRACE='1'),capture_output=True,text=True,timeout=120);rows.append(dict(label=label,argv=args,cwd=str(B),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2000:],flush=True)
 if q.returncode:print(q.stderr[-3000:]);raise SystemExit(q.returncode)
 return q
run('shared-error-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',E/'source/errors.cpp','-ldl','-o',B/'errors']);q=run('shared-input-rejections',[B/'errors',m['libraries']['base']['path']]);p=E/'local-results.json';d=json.loads(p.read_text());d['errors']=q.stdout;p.write_text(json.dumps(d,indent=2)+'\n')
