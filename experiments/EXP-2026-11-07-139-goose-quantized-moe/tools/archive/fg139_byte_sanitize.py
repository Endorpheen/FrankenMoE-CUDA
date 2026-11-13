from pathlib import Path
import json,subprocess,os,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized-byte';v=r/'variants/moe-quantized-byte-sanitize';(v/'build').mkdir(parents=True,exist_ok=True)
shutil.copy2('/tmp/fg139_matrix_fixture.c',s/'matrix-fixture.c')
flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer']
commands=[];reports={}
for profile in ['apex','qwen']:
 src=r/'variants/moe-quantized-byte/build'/('matrix-commands.json' if profile=='apex' else 'matrix-qwen-commands.json')
 compile=json.loads(src.read_text())[0]
 compile=[x for x in compile if x not in ['-O3','-DFG139_MATH_TRACE=1']]+flags
 compile[compile.index('-o')+1]=str(v/'build'/f'matrix-{profile}.o')
 p=subprocess.run(compile,capture_output=True,text=True);(v/'build'/f'matrix-{profile}-compile.log').write_text(p.stdout+p.stderr);print(profile,'compile',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode();commands.append(compile)
 baseline=r/'run' if profile=='apex' else r/'variants/qwen839a-shared/build/bin'
 exe=v/'build'/f'matrix-{profile}-fixture'
 cmd=['/usr/bin/gcc-13','-O1','-g','-I'+str(r/'source/ggml/include'),'-I'+str(r/'source/ggml/src'),'-I'+str(s),str(s/'matrix-fixture.c'),str(v/'build'/f'matrix-{profile}.o'),'-L'+str(baseline),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(baseline),'-o',str(exe),*flags]
 p=subprocess.run(cmd,capture_output=True,text=True);(v/'build'/f'matrix-{profile}-fixture-compile.log').write_text(p.stdout+p.stderr);print(profile,'fixture',p.returncode,(p.stdout+p.stderr)[-1500:],flush=True);p.check_returncode();commands.append(cmd)
 env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(baseline),ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
 p=subprocess.run([str(exe)],env=env,capture_output=True,text=True,timeout=180)
 (r/'results'/f'moe-matrix-byte-sanitized-{profile}.log').write_text(p.stdout+p.stderr);print(profile,'sanitized run',p.returncode,p.stdout,p.stderr[-2000:],flush=True);p.check_returncode();reports[profile]=json.loads(p.stdout.strip())
report={'status':'PASS_ALL_FIVE_SCOPED_ASAN_UBSAN_LSAN','runs':reports,'instrumented':'Whole new Goose matrix/dequant/routed/fused math and C fixture','uninstrumented':'Inherited native reference and backend libraries','compiler_source_changed':False,'limitations':'Not full backend ownership audit or TSan. Native precision policy pinned per reference.'}
(r/'results/moe-matrix-byte-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n');(v/'build/matrix-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
