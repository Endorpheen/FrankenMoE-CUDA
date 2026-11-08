from pathlib import Path
import os,json,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-161-goose-allocator';S=E/'source';B=F/'runtime/exp161/build';R=F/'runtime/exp161/run';rows=json.loads((B/'commands.json').read_text());m=json.loads((S/'manifest.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B,env=env):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2000:],flush=True)
 if q.returncode:print(q.stderr[-4000:]);raise SystemExit(q.returncode)
 return q
r=next(x for x in rows if x['label']=='allocator-core-build');probes=[]
for kind,base in [('native',(F/'runtime/exp160/run/libggml-base.so').resolve()),('candidate',Path(m['library']))]:
 cmd=r['argv'].copy();cmd.insert(1,'-DFG161_TESTING');cmd[cmd.index('-c')+1]=str(S/('probe-'+kind+'.c'));obj=B/('probe-'+kind+'.o');cmd[cmd.index('-o')+1]=str(obj);run(kind+'-allocator-probe-build',cmd,Path(r['cwd']));out=B/('liballocator-'+kind+'.so')
 if kind=='native':
  link=next(x for x in json.loads((E.parent/'EXP-2026-11-08-160-goose-shared-fusion/build-commands.json').read_text()) if x['label']=='shared-base-link');original='/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107/variants/cpu-backend/build/base-ggmlalloc.o'
 else:link=next(x for x in rows if x['label']=='allocator-base-link');original=str(B/'allocator.o')
 cmd=[str(obj) if x==original else x for x in link['argv']];assert cmd.count(str(obj))==1;cmd[cmd.index('-o')+1]=str(out);run(kind+'-allocator-probe-link',cmd,Path(link['cwd']));probes.append(out)
run('allocator-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',S/'fixture.cpp','-ldl','-o',B/'allocator-fixture']);p=run('allocator-native-parity',[B/'allocator-fixture']+probes);q=run('allocator-native-rollback',[B/'allocator-fixture']+probes,env=dict(env,GGML_GOOSE_ALLOCATOR='0'));(E/'local-results.json').write_text(json.dumps(dict(status='PASS_RELEASE_ALLOCATOR_NATIVE_PARITY',goose=p.stdout,rollback=q.stdout,model_runs=0,production_changed=False),indent=2)+'\n')
