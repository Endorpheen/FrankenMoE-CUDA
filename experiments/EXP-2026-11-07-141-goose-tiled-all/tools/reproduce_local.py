from pathlib import Path
import argparse,hashlib,json,subprocess,os,re
ap=argparse.ArgumentParser();ap.add_argument('--build-root',required=True);args=ap.parse_args();snapshot=Path(__file__).resolve().parents[1];out=Path(args.build_root).expanduser().resolve();allowed=Path('/home/end0/AI/LlamaServer').resolve();assert out.is_relative_to(allowed) and out!=allowed and not out.exists(),out
for row in json.loads((snapshot/'external-dependencies.json').read_text()):
 p=Path(row['path']);assert p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],('dependency changed',str(p))
out.mkdir();root=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');plan=json.loads((snapshot/'build-plan.json').read_text());commands=[];reports={}
for profile,rows in plan.items():
 current=out/profile;(current/'build').mkdir(parents=True);(current/'run').mkdir();old=root/'variants'/('tiled-all-'+profile)
 for path in (old/'run').iterdir():
  if path.name.startswith('libggml-cpu.so'):continue
  (current/'run'/path.name).symlink_to(path)
 def remap(arg):
  return arg.replace(str(root/'source/cpu-tiled-all')+'/',str(snapshot/'source/cpu-tiled-all')+'/').replace(str(old/'build')+'/',str(current/'build')+'/').replace(str(old/'run')+'/',str(current/'run')+'/')
 for row in rows:
  cmd=[remap(x) for x in row['argv']];p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(current/'build'/(row['label']+'.log')).write_text(p.stdout+p.stderr);print(profile,row['label'],p.returncode,flush=True)
  if p.returncode:print((p.stdout+p.stderr)[-3000:]);p.check_returncode()
  commands.append({'profile':profile,'label':row['label'],'cwd':row['cwd'],'argv':cmd})
 for suffix in ['.so','.so.0']:(current/'run'/('libggml-cpu'+suffix)).symlink_to('libggml-cpu.so.0.27.0')
 source=snapshot/'source/cpu-tiled-all/graph-fixture.c';headers=root/'source/ggml' if profile=='apex' else Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml');cmd=['/usr/bin/gcc-13','-O3','-I'+str(headers/'include'),'-I'+str(headers/'src'),str(source),'-L'+str(current/'run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(current/'run'),'-o',str(current/'build/fixture')];p=subprocess.run(cmd,capture_output=True,text=True);(current/'build/fixture-compile.log').write_text(p.stdout+p.stderr);p.check_returncode();commands.append({'profile':profile,'label':'fixture','argv':cmd})
 env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(current/'run'),GGML_GOOSE_TILED_GENERAL='1',GGML_CPU_TILED_MM='1',GGML_CPU_TILED_MM_FORCE='0',GGML_GOOSE_ELEMENTWISE='0',GGML_GOOSE_MOE_CONTROL='1',GGML_GOOSE_MOE='1',GGML_GOOSE_TILED='1',GGML_GOOSE_MOE_QUANT='1',GGML_GOOSE_MOE_MATH='1',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_CPU_IQ2S_FUSED='1',GGML_CPU_IQ2S_FUSED_STATS='1');output=current/'build/graphs.bin';p=subprocess.run([str(current/'build/fixture'),str(output)],env=env,capture_output=True,text=True,timeout=180);(current/'build/fixture.log').write_text(p.stdout+p.stderr);p.check_returncode();rawhash=hashlib.sha256(output.read_bytes()).hexdigest();expected=json.loads((snapshot/'results'/f'cpu-tiled-all-{profile}-graphs-summary.json').read_text());assert rawhash==expected.get('sha256',expected.get('output_sha256')),(profile,rawhash);witness=[{'stage':int(a),'kind':int(b),'units':int(c)} for a,b,c in re.findall(r'FG141_TILED stage=(\d+) kind=(\d+) units=(\d+)',p.stderr)];assert all({x['kind'] for x in witness if x['stage']==stage and x['units']>0}==set(range(13)) for stage in range(6))
 reports[profile]={'graphs':json.loads(p.stdout),'sha256':rawhash,'tiled_witness':witness};print(profile,'exact graph output verified',rawhash,flush=True)
report={'status':'PASS_SNAPSHOT_REBUILD_APEX104_QWEN104_WHOLE_TILED','profiles':reports,'objects_rebuilt':sum('-c' in x['argv'] for x in commands),'external_dependencies_verified':True,'model_loaded':False,'build_root':str(out),'commands':commands};(out/'reproduction.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='commands'},indent=2))
