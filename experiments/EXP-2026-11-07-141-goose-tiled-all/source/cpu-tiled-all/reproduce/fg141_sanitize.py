from pathlib import Path
import json,subprocess,os,hashlib,sys
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');profile=sys.argv[1];assert profile in ['apex','qwen'];old=r/'variants'/('tiled-all-'+profile);v=r/'variants'/('tiled-all-'+profile+'-sanitize');(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True);flags=['-O1','-g','-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer'];commands=[]
for path in (old/'run').iterdir():
 if path.name.startswith('libggml-cpu.so'):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for row in json.loads((old/'build/commands.json').read_text()):
 cmd=[a.replace(str(old/'build'),str(v/'build')).replace(str(old/'run'),str(v/'run')) for a in row['argv'] if a!='-O3']+flags;p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(v/'build'/(row['label']+'.log')).write_text(p.stdout+p.stderr);print(profile,row['label'],p.returncode,(p.stdout+p.stderr)[-1200:],flush=True);p.check_returncode();commands.append({'label':row['label'],'cwd':row['cwd'],'argv':cmd})
for suffix in ['.so','.so.0']:
 path=v/'run'/('libggml-cpu'+suffix)
 if not path.exists() and not path.is_symlink():path.symlink_to('libggml-cpu.so.0.27.0')
headers=r/'source/ggml' if profile=='apex' else Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml');cmd=['/usr/bin/gcc-13','-I'+str(headers/'include'),str(r/'source/cpu-tiled-all/graph-fixture.c'),'-L'+str(v/'run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(v/'run'),'-o',str(v/'build/fixture'),*flags];p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/fixture-compile.log').write_text(p.stdout+p.stderr);p.check_returncode();commands.append({'label':'fixture','argv':cmd});results={};new_failed=False
for forced in [0,1]:
 for mode in ['off','on']:
  env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(v/'run'),GGML_GOOSE_TILED_GENERAL='1' if mode=='on' else '0',GGML_CPU_TILED_MM='1',GGML_CPU_TILED_MM_FORCE=str(forced),GGML_GOOSE_MOE_CONTROL='1',GGML_GOOSE_MOE='1',GGML_GOOSE_TILED='1',GGML_GOOSE_MOE_MATH='1',GGML_GOOSE_MOE_QUANT='1',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_GOOSE_ELEMENTWISE='0',ASAN_OPTIONS='detect_leaks=1:halt_on_error=1',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1');name=f'cpu-tiled-all-{profile}-sanitized-force{forced}-{mode}';output=r/'results'/f'{name}.bin';p=subprocess.run([str(v/'build/fixture'),str(output)],env=env,capture_output=True,text=True,timeout=240);(r/'results'/f'{name}.log').write_text(p.stdout+p.stderr);print(profile,forced,mode,p.returncode,p.stdout.strip(),p.stderr[-2400:],flush=True);row={'exit_code':p.returncode,'log':f'results/{name}.log'}
  if p.returncode==0:
   tag="forced-graphs" if forced else "graphs";expected=r/'results'/f'cpu-tiled-all-{profile}-{tag}-native.bin'
   actual=output.read_bytes();golden=expected.read_bytes();row.update(graphs=json.loads(p.stdout),all_bytes_equal_original=actual==golden,sha256=hashlib.sha256(actual).hexdigest());assert actual==golden,(profile,forced,mode,'sanitized bytes differ')
  else:
   row['diagnostic_tail']=p.stderr[-3000:];new_failed|=mode=='on'
  results[f'force{forced}-{mode}']=row
report={'status':'FAIL_NEW_GOOSE_TILED_SANITIZER' if new_failed else 'PASS_NEW_GOOSE_TILED_SCOPED_SANITIZERS','profile':profile,'runs':results,'all_native_fallback_runs_pass':all(row['exit_code']==0 for key,row in results.items() if key.endswith('-off')),'instrumented':'Whole new Goose tiled unit, modified CPU core, bridge, renamed native tiled fallback and C fixture','uninstrumented':'Earlier math/quant/base/other CPU objects including native microkernel object; no TSan/full comparative memory-safety claim','production_changed':False,'compiler_changed':False};(r/'results'/f'cpu-tiled-all-{profile}-sanitizer-summary.json').write_text(json.dumps(report,indent=2)+'\n');(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n');print(json.dumps({'status':report['status'],'native_fallback_all_pass':report['all_native_fallback_runs_pass'],'runs':{k:v['exit_code'] for k,v in results.items()}},indent=2));assert not new_failed
