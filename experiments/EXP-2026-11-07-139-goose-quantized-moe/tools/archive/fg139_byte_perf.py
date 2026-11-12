from pathlib import Path
import json,subprocess,os
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');v=r/'variants/moe-quantized-byte-perf';(v/'build').mkdir(parents=True,exist_ok=True)
compile=json.loads((r/'variants/moe-quantized-byte/build/matrix-qwen-commands.json').read_text())[0];compile=[x for x in compile if x!='-DFG139_MATH_TRACE=1'];compile[compile.index('-o')+1]=str(v/'build/matrix.o')
bench=json.loads((r/'variants/moe-quantized-perf/build/commands.json').read_text())[1];bench=[a.replace('variants/moe-quantized-perf/','variants/moe-quantized-byte-perf/') for a in bench]
for i,cmd in enumerate([compile,bench]):
 p=subprocess.run(cmd,capture_output=True,text=True);(v/'build'/f'compile-{i}.log').write_text(p.stdout+p.stderr);print('compile',i,p.returncode,p.stderr[-1000:],flush=True);p.check_returncode()
(v/'build/commands.json').write_text(json.dumps([compile,bench],indent=2)+'\n')
env=dict(os.environ);env.pop('LD_PRELOAD',None);env['LD_LIBRARY_PATH']=str(r/'variants/qwen839a-shared/build/bin');p=subprocess.run([str(v/'build/matrix-bench')],env=env,capture_output=True,text=True,timeout=180);(r/'results/moe-matrix-byte-operation.log').write_text(p.stdout+p.stderr);print('exit',p.returncode,p.stderr[-1000:],flush=True);p.check_returncode();report=json.loads(p.stdout);report.update(preliminary=True,controlled_quiet_run=False,scope='Counter-free whole Goose routed matrix chunk, word Q2 and byte masks plus same increased GCC inline budget; unchanged native control');(r/'results/moe-matrix-byte-operation-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
