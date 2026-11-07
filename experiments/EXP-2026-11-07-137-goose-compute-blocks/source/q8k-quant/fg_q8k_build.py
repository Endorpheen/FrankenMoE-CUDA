from pathlib import Path
import subprocess,json,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/q8k-quant'
p=s/'unit.c';t=p.read_text();needle='#undef main\n';addition='_Static_assert(sizeof(gg_type_3)==292,"Q8_K layout");\n_Static_assert(offsetof(gg_type_3,f1)==4 && offsetof(gg_type_3,f2)==260,"Q8_K fields");\n';assert needle in t
if addition not in t:p.write_text(t.replace(needle,needle+addition,1))
commands=[]
for trace in [False,True]:
 lib=r/'build'/('libfranken-goose-q8k-trace.so' if trace else 'libfranken-goose-q8k.so');cmd=['/usr/bin/gcc','-O3','-march=native','-ffp-contract=off','-fPIC','-fno-strict-aliasing','-shared',str(s/'unit.c'),str(s/'hook.c'),'-ldl','-lm','-o',str(lib)]
 if trace:cmd+=['-DFG_Q8K_TRACE=1']
 p=subprocess.run(cmd,cwd=r,capture_output=True,text=True);(s/('build-trace.log' if trace else 'build.log')).write_text(p.stdout+p.stderr);print('build',trace,p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode();commands.append(cmd)
cmd=['/usr/bin/gcc','-O3','-march=native','-ffp-contract=off','-fPIC','-fno-strict-aliasing','-shared',str(s/'unit.c'),'-lm','-o',str(r/'build/libgoose-q8k-quant.so')];p=subprocess.run(cmd,cwd=r,capture_output=True,text=True);(s/'unit-build.log').write_text(p.stdout+p.stderr);print('unit',p.returncode,(p.stdout+p.stderr)[-1000:],flush=True);p.check_returncode();commands.append(cmd)
(s/'build-commands.json').write_text(json.dumps(commands,indent=2)+'\n')
code=(r/'generated/q8k-quant.c').read_text();start=code.index('static void gg_function_1(',code.index('static void gg_function_1(')+1);end=code.index('static void gg_function_0(',start);reachable=code[start:end];assert all(x not in reachable for x in ['gg_get_stack','gg_allocate','gg_stacks']);(s/'manifest.json').write_text(json.dumps({'producer':json.loads((r/'compiler/current-producer.json').read_text()),'reachable_stack_allocator':False,'ffi':'292-byte Q8_K static assertions; only primitive float bit copy extern','sha256':{str(p.relative_to(r)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [s/'kernel.goose',s/'unit.c',s/'hook.c',r/'generated/q8k-quant.c']}},indent=2)+'\n')
