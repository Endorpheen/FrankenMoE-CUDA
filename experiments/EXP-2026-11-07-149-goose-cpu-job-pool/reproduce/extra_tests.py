from pathlib import Path
import json,os,subprocess
p=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107');out=p/'results';records=[]
for mode in ['cpu-backend','pthread','sanitized','pthread-sanitized']:
 v=p/'variants'/mode;b=v/'build';san='sanitized' in mode;threaded=mode.startswith('pthread');folder=out/(mode+'-extra');folder.mkdir(exist_ok=True)
 common=['/usr/bin/gcc-13','-O1' if san else '-O2','-g']+(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if san else [])
 fixtures=[('pool-fixture',Path(__file__).with_name('pool-fixture.c')),('policy-fixture',Path(__file__).with_name('policy-fixture.c')),('dispatch-fixture',p/'source/cpu-backend/control/dispatch-fixture.c')]
 for name,source in fixtures:
  args=common+[str(source)]
  if name=='pool-fixture':args+=['-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-L'+str(v/'run'),'-lggml-cpu','-lggml-base','-ldl','-Wl,--export-dynamic-symbol=fg149_system_alloc','-Wl,--export-dynamic-symbol=fg149_system_free','-Wl,--export-dynamic-symbol=fg149_system_start']
  else:args+=[str(b/('pool.o' if name=='policy-fixture' else 'control.o'))]
  args+=['-lm','-o',str(b/name)]
  q=subprocess.run(args,capture_output=True,text=True);(folder/(name+'-compile.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr[-1800:]
  env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(v/'run'),ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
  cmd=[str(b/name)]+(['pthread'] if name=='pool-fixture' and threaded else [])
  q=subprocess.run(cmd,env=env,capture_output=True,text=True,timeout=90);(folder/(name+'.jsonl')).write_text(q.stdout);(folder/(name+'.log')).write_text(q.stderr);print(mode,name,q.returncode,q.stdout[-400:],flush=True);assert q.returncode==0,q.stderr[-1600:];records.append({'mode':mode,'name':name,'compile':args,'execute':cmd,'exit_code':q.returncode,'result':json.loads(q.stdout)})
(out/'extra-summary.json').write_text(json.dumps({'status':'PASS_REAL_OMP_PTHREAD_POOL_POLICY_DISPATCH_RELEASE_AND_SANITIZED','records':records,'scope':'Real pool/graph/threads and injected allocator/start failures;mock NUMA/priority/wrap policy;dispatcher callbacks stubbed,actual numerical GGML corpus separate'},indent=2)+'\n')
