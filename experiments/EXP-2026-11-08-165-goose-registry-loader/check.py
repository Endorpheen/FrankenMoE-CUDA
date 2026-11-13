from pathlib import Path
import json,os,subprocess,shlex
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-165-goose-registry-loader';S=E/'source';B=F/'runtime/exp165/build';R=F/'runtime/exp165/run';D=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda');rows=json.loads((B/'commands.json').read_text());env=dict(os.environ,LD_LIBRARY_PATH=str(R),GGML_NO_BACKTRACE='1')
def run(label,args,cwd=B,delta={}):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,env=dict(env,**delta),capture_output=True,text=True,timeout=180);rows.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(rows,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,q.stdout[-2400:],flush=True)
 if q.returncode:print(q.stderr[-4500:]);raise SystemExit(q.returncode)
 return q
build=next(r for r in rows if r['label']=='registry-backend-build');probes=[]
for kind in ['native','candidate']:
 cmd=build['argv'].copy();cmd.insert(1,'-DFG165_TESTING');obj=B/('probe-'+kind+'.o');cmd[cmd.index('-c')+1]=str(S/('probe-'+kind+'.cpp'));cmd[cmd.index('-o')+1]=str(obj);run('registry-'+kind+'-probe-build',cmd,Path(build['cwd']));lib=B/('libregistry-'+kind+'.so');cmd=['/usr/bin/g++-13','-shared','-Wl,-Bsymbolic','-Wl,-z,defs','-o',lib,obj,D/'ggml/src/CMakeFiles/ggml.dir/ggml-backend-dl.cpp.o']+([B/'registry-goose.o'] if kind=='candidate' else [])+['-L'+str(R),'-Wl,--disable-new-dtags,-rpath,'+str(R),'-lggml-base','-ldl'];run('registry-'+kind+'-probe-link',cmd);probes.append(lib)
inc=['-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src']
plugins=B/'plugins';plugins.mkdir(exist_ok=True)
for n in ['best','fallback','all']:(plugins/n).mkdir(exist_ok=True)
for directory,name,target in [('best','libggml-test-low.so','low'),('best','libggml-test-high.so','high'),('best','libggml-test-zero.so','zero'),('best','libggml-test-negative.so','negative'),('best','libggml-test-ignore.so.0','abi'),('fallback','libggml-base.so','base'),('all','libggml-cuda-low.so','low'),('all','libggml-cuda-high.so','high'),('all','libggml-cpu.so','base')]:
 p=plugins/directory/name
 if not p.is_symlink():p.symlink_to(plugins/(target+'.so'))
variants=[('zero',0,[]),('low',3,[]),('high',9,[]),('negative',-1,[]),('base',1,['-DPLUGIN_NO_SCORE']),('missing',8,['-DPLUGIN_NO_INIT']),('null',8,['-DPLUGIN_NULL']),('abi',8,['-DPLUGIN_API=999'])]
for name,score,extra in variants:run('plugin-'+name,['/usr/bin/g++-13','-std=c++17','-shared','-fPIC','-O2']+inc+['-DPLUGIN_SCORE='+str(score),'-DPLUGIN_NAME="'+name+'"']+extra+[S/'plugin.cpp','-o',plugins/(name+'.so')])
run('registry-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2']+inc+[S/'fixture.cpp','-ldl','-lpthread','-o',B/'fixture']);results=[]
for label,extra,delta in [('parity',[],{}),('rollback',[],{'GGML_GOOSE_REGISTRY':'0'}),('threads',['threads'],{})]:
 q=run('registry-'+label,[B/'fixture']+probes+[plugins]+extra,delta=delta);results.append(dict(label=label,stdout=q.stdout,exit_code=q.returncode))
(E/'local-results.json').write_text(json.dumps(dict(status='PASS_REGISTRY_LOADER_NATIVE_PARITY_ROLLBACK_THREADS',runs=results,production_changed=False),indent=2)+'\n')

run('registry-fault-fixture-build',['/usr/bin/g++-13','-std=c++17','-O2']+inc+[S/'faults.cpp','-ldl','-lpthread','-o',B/'faults'])
q=run('registry-bounds-OOM',[B/'faults',B/'libregistry-candidate.so']);p=E/'local-results.json';d=json.loads(p.read_text());d['failure_checks']=q.stdout;p.write_text(json.dumps(d,indent=2)+'\n')
