from pathlib import Path
import json,subprocess,os
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');v=r/'variants/moe-quantized-inline';(v/'build').mkdir(parents=True,exist_ok=True)
flags=['--param=max-inline-insns-auto=2000','--param=max-inline-insns-single=4000','--param=inline-unit-growth=500']
(v/'build/hypothesis.json').write_text(json.dumps({'hypothesis':'Increase host GCC inline budget to eliminate fixed-array aggregate call/stack traffic while retaining unchanged emitted Goose C, checks and FP flags.','flags':flags,'compiler_source_changed':False,'baseline':'moe-quantized-branchless-perf','production_changed':False},indent=2)+'\n')
cmds=json.loads((r/'variants/moe-quantized-branchless-perf/build/commands.json').read_text());out=[]
for i,cmd in enumerate(cmds):
 cmd=[a.replace('variants/moe-quantized-branchless-perf/','variants/moe-quantized-inline/') for a in cmd]
 if i==0:cmd+=flags
 p=subprocess.run(cmd,capture_output=True,text=True);(v/'build'/f'compile-{i}.log').write_text(p.stdout+p.stderr);print('compile',i,p.returncode,p.stderr[-1000:],flush=True);p.check_returncode();out.append(cmd)
cmd=['/usr/bin/gcc-13','-shared',str(v/'build/matrix.o'),'-lm','-o',str(v/'build/libgoose-moe-matrix-qwen.so')];p=subprocess.run(cmd,capture_output=True,text=True);p.check_returncode();out.append(cmd)
(v/'build/commands.json').write_text(json.dumps(out,indent=2)+'\n')
