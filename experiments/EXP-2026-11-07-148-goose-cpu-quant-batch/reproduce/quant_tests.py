from pathlib import Path
import os,json,subprocess,sys
p=Path('/home/end0/AI/LlamaServer/franken-goose-exp148-quant-20261107');s=p/'source/cpu-backend/quant';sanitize='--sanitize' in sys.argv;v=p/'variants'/('sanitized' if sanitize else 'cpu-backend');out=p/'results'/('sanitized' if sanitize else 'release');out.mkdir(exist_ok=True)
env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(v/'run'),ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
records=[]
for label,source in [('quant-traits',s/'fixture.c'),('activation-contract',s/'activation-contract.c')]:
 cmd=['/usr/bin/gcc-13','-O1' if sanitize else '-O2','-march=native','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/src',str(source),'-L'+str(v/'run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-o',str(v/'build'/label)]
 if sanitize:cmd+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
 q=subprocess.run(cmd,capture_output=True,text=True);(out/(label+'-compile.log')).write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr
 for arm in (['candidate','rollback'] if label=='quant-traits' else ['candidate']):
  local=dict(env,GGML_GOOSE_CPU_QUANT='0' if arm=='rollback' else '1');q=subprocess.run([str(v/'build'/label)],env=local,capture_output=True,text=True,timeout=180);(out/(label+'-'+arm+'.jsonl')).write_text(q.stdout);(out/(label+'-'+arm+'.log')).write_text(q.stderr);print(label,arm,q.returncode,q.stdout[-500:],flush=True);assert q.returncode==0,q.stderr[-1500:];records.append({'label':label,'arm':arm,'compile':cmd,'exit_code':q.returncode,'result':json.loads(q.stdout)})
(out/'quant-summary.json').write_text(json.dumps({'status':'PASS_NINE_QUANT_FORMATS_TRAITS_ROLLBACK_AND_Q8_SPECIALS','records':records,'scope':'Bit-exact C++ valid finite corpus,8threads,canaries plus retained Q8_0 NaN/Inf/subnormal corpus;compact nonfinite uses explicit native fallback;not whole type family closure'},indent=2)+'\n')
