from pathlib import Path
import argparse,hashlib,json,subprocess,os,re
ap=argparse.ArgumentParser();ap.add_argument('--build-root',required=True);args=ap.parse_args();snapshot=Path(__file__).resolve().parents[1];out=Path(args.build_root).expanduser().resolve();allowed=Path('/home/end0/AI/LlamaServer').resolve();assert out.is_relative_to(allowed) and out!=allowed and not out.exists(),out
for row in json.loads((snapshot/'external-dependencies.json').read_text()):
 p=Path(row['path']);assert p.is_file() and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],('dependency changed',str(p))
out.mkdir();(out/'build').mkdir();(out/'run').mkdir();root='/home/end0/AI/LlamaServer/franken-goose-live-20261107';commands=[]
def remap(arg):
 for folder in ['moe-quantized-byte','moe-quantized-branchless','moe-quantized']:
  arg=arg.replace(root+'/source/'+folder+'/',str(snapshot/'source'/folder)+'/')
 for folder in ['qwen839a-moe-quantized-byte','qwen839a-moe-quantized']:
  for sub in ['build','run']:arg=arg.replace(root+'/variants/'+folder+'/'+sub+'/',str(out/sub)+'/')
 return arg
for row in json.loads((snapshot/'build-plan.json').read_text()):
 cmd=[remap(a) for a in row['argv']];p=subprocess.run(cmd,cwd=row['cwd'],capture_output=True,text=True);(out/'build'/(row['label']+'.log')).write_text(p.stdout+p.stderr);print(row['label'],p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-3000:]);p.check_returncode()
 commands.append({'label':row['label'],'cwd':row['cwd'],'argv':cmd})
for stem in ['libggml-cpu','libggml-base']:
 for suffix in ['.so','.so.0']:(out/'run'/(stem+suffix)).symlink_to(stem+'.so.0.27.0')
source=Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml');cmd=['/usr/bin/gcc-13','-O3','-I'+str(source/'include'),'-I'+str(source/'src'),'-I'+str(snapshot/'source/moe-quantized'),str(snapshot/'source/moe-quantized/qwen-graph-fixture.c'),'-L'+str(out/'run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(out/'run'),'-o',str(out/'build/graph-fixture')];p=subprocess.run(cmd,capture_output=True,text=True);(out/'build/fixture-compile.log').write_text(p.stdout+p.stderr);p.check_returncode();commands.append({'label':'fixture','argv':cmd})
env=dict(os.environ);env.pop('LD_PRELOAD',None);env.update(LD_LIBRARY_PATH=str(out/'run'),GGML_GOOSE_MOE_MATH='1',GGML_GOOSE_MOE_QUANT='1',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_CPU_IQ2S_FUSED='1',GGML_CPU_IQ2S_FUSED_STATS='1');p=subprocess.run([str(out/'build/graph-fixture'),str(out/'build/graphs.bin')],env=env,capture_output=True,text=True,timeout=180);(out/'build/fixture.log').write_text(p.stdout+p.stderr);p.check_returncode();actual=hashlib.sha256((out/'build/graphs.bin').read_bytes()).hexdigest();expected=json.loads((snapshot/'results/moe-qwen-byte-graphs-summary.json').read_text())['output_sha256'];assert actual==expected,(actual,expected);w=[(a,int(b),int(c),int(e)) for a,b,c,e in re.findall(r'FG139_MATH component=(\w+) mode=(\d+) kind=(\d+) calls=(\d+)',p.stderr)];assert {k for comp,m,k,c in w if comp=='cpu' and c>0}==set(range(5));assert {k for comp,m,k,c in w if comp=='base' and m==2 and c>0}==set(range(5));assert any(comp=='cpu' and m==1 and k==4 and c>0 for comp,m,k,c in w)
report={'status':'PASS_SNAPSHOT_REBUILD_290_REAL_GRAPHS','graph_summary':json.loads(p.stdout),'sha256':actual,'all_five_cpu_and_base_and_fused_witnessed':True,'external_dependencies_verified':True,'model_loaded':False,'build_root':str(out),'objects_rebuilt':sum('-c' in x['argv'] for x in commands),'commands':commands};(out/'reproduction.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='commands'},indent=2))
