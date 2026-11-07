from pathlib import Path
import json,subprocess,os,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized';v=r/'variants/moe-quantized-sanitize';(v/'build').mkdir(parents=True,exist_ok=True)
shutil.copy2('/tmp/fg139_activation_fixture.c',s/'activation-fixture.c')
flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
compile,link=json.loads((r/'variants/moe-quantized/build/activation-commands.json').read_text());compile=[x.replace(str(r/'variants/moe-quantized/build/activation.o'),str(v/'build/activation.o')) for x in compile if x not in ['-O3','-DFG139_QUANT_TRACE=1']]+flags
commands=[]
def run(label,cmd):
 p=subprocess.run(cmd,capture_output=True,text=True);(v/'build'/f'{label}.log').write_text(p.stdout+p.stderr);print(label,p.returncode,(p.stdout+p.stderr)[-1200:],flush=True);p.check_returncode();commands.append(cmd)
run('activation',compile)
link=[x.replace(str(r/'variants/moe-quantized/build'),str(v/'build')) for x in link]+flags
run('link',link)
fixture=['/usr/bin/gcc-13','-O1','-g','-I'+str(r/'source/ggml/include'),str(s/'activation-fixture.c'),'-L'+str(v/'build'),'-lgoose-moe-activation','-L'+str(r/'run'),'-lggml-cpu','-lggml-base','-lpthread','-lm','-Wl,-rpath,'+str(v/'build')+':'+str(r/'run'),'-o',str(v/'build/activation-fixture'),*flags]
run('fixture',fixture)
env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(v/'build')+':'+str(r/'run'),GGML_GOOSE_MOE_QUANT='1',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
p=subprocess.run([str(v/'build/activation-fixture')],env=env,capture_output=True,text=True,timeout=120)
(r/'results/moe-activation-sanitizer.log').write_text(p.stdout+p.stderr);print('sanitized run',p.returncode,p.stdout,p.stderr[-2000:],flush=True);p.check_returncode()
result=json.loads(p.stdout.strip());result.update(sanitizers=['ASan','UBSan','LSan'],instrumented='New Goose activation module and fixture; inherited native reference/backend libraries not instrumented',compiler_source_changed=False)
(r/'results/moe-activation-sanitizer-summary.json').write_text(json.dumps(result,indent=2)+'\n');(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n')
