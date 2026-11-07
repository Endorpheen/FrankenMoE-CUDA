from pathlib import Path
import json,os,subprocess
p=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107');v=p/'variants/pthread-tsan';b=v/'build';run=v/'run';b.mkdir(parents=True,exist_ok=True);run.mkdir(exist_ok=True)
source=p/'variants/pthread';recipes=json.loads((source/'build/commands.json').read_text());records=[]
for path in (source/'run').iterdir():
 if path.is_file() and not path.name.startswith('libggml-cpu.so') and not (run/path.name).exists():(run/path.name).symlink_to(path.resolve())
for label in ['core','pool-unit','control','executor']:
 row=next(x for x in recipes if x['label']==label);argv=[x.replace(str(source/'build'),str(b)) for x in row['argv']];argv=[('-O1' if x in ['-O2','-O3'] else x) for x in argv]+['-fsanitize=thread','-fno-omit-frame-pointer','-g'];q=subprocess.run(argv,cwd=row['cwd'],capture_output=True,text=True);(b/(label+'.log')).write_text(q.stdout+q.stderr);print('tsan',label,q.returncode,flush=True);assert q.returncode==0,q.stderr[-1600:];records.append({'argv':argv,'exit_code':q.returncode})
row=next(x for x in recipes if x['label']=='link-cpu');argv=[]
for arg in row['argv']:
 path=Path(arg)
 if path.parent==source/'build' and path.name in ['core.o','pool.o','control.o','executor.o']:arg=str(b/path.name)
 arg=arg.replace(str(source/'run'),str(run));argv.append(arg)
argv+=['-fsanitize=thread'];q=subprocess.run(argv,cwd=row['cwd'],capture_output=True,text=True);(b/'link.log').write_text(q.stdout+q.stderr);assert q.returncode==0,q.stderr[-1600:]
for name in ['libggml-cpu.so','libggml-cpu.so.0']:(run/name).symlink_to('libggml-cpu.so.0.27.0')
cmd=['/usr/bin/gcc-13','-O1','-g','-fsanitize=thread','-fno-omit-frame-pointer','-I/home/end0/AI/LlamaServer/franken-goose-live-20261107/source/ggml/include',str(Path(__file__).with_name('pool-fixture.c')),'-L'+str(run),'-lggml-cpu','-lggml-base','-lm','-ldl','-Wl,--export-dynamic-symbol=fg149_system_alloc','-Wl,--export-dynamic-symbol=fg149_system_free','-Wl,--export-dynamic-symbol=fg149_system_start','-o',str(b/'pool-fixture')];q=subprocess.run(cmd,capture_output=True,text=True);assert q.returncode==0,q.stderr
q=subprocess.run([str(b/'pool-fixture'),'pthread'],env=dict(os.environ,LD_LIBRARY_PATH=str(run),TSAN_OPTIONS='halt_on_error=1'),capture_output=True,text=True,timeout=180);(p/'results/tsan-runtime.log').write_text(q.stdout+q.stderr)
report={'status':'PASS_SCOPED_PTHREAD_TSAN' if q.returncode==0 else 'TSAN_RUNTIME_LIMITATION' if 'unexpected memory mapping' in q.stderr else 'FAIL_SCOPED_PTHREAD_TSAN','exit_code':q.returncode,'stdout':q.stdout,'stderr_tail':q.stderr[-2000:],'scope':'New core/pool/control/executor and real pthread lifecycle/graph failure fixture instrumented;retained other objects/base/vendor libraries not fully instrumented'}
(p/'results/tsan-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
