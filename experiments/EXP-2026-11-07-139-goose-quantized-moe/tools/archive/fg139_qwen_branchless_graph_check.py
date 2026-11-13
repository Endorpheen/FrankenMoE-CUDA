from pathlib import Path
import json,subprocess,os,re,hashlib,shutil
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized';v=r/'variants/qwen839a-moe-quantized-branchless'
shutil.copy2('/tmp/fg139_qwen_graph_fixture.c',s/'qwen-graph-fixture.c')
cmd=['/usr/bin/gcc-13','-O3','-I/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml/include',str(s/'qwen-graph-fixture.c'),'-I/home/end0/AI/LlamaServer/franken-qwen38-v31/source/ggml/src','-L'+str(v/'run'),'-lggml-cpu','-lggml-base','-lm','-lpthread','-Wl,-rpath,'+str(v/'run'),'-o',str(v/'build/qwen-graph-fixture')]
p=subprocess.run(cmd,capture_output=True,text=True);(v/'build/graph-fixture-compile.log').write_text(p.stdout+p.stderr);print('graph fixture compile',p.returncode,(p.stdout+p.stderr)[-1600:],flush=True);p.check_returncode();(v/'build/graph-fixture-command.json').write_text(json.dumps(cmd,indent=2)+'\n')
runs={};witness={}
for mode in ['native','off','on']:
 env=dict(os.environ);env.pop('LD_PRELOAD',None)
 env.update(LD_LIBRARY_PATH=str(r/'variants/qwen839a-shared/build/bin' if mode=='native' else v/'run'),GGML_GOOSE_MOE_MATH='1' if mode=='on' else '0',GGML_GOOSE_MOE_QUANT='1' if mode=='on' else '0',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0',GGML_CPU_IQ2S_FUSED='1',GGML_CPU_IQ2S_FUSED_STATS='1')
 p=subprocess.run([str(v/'build/qwen-graph-fixture'),str(r/'results'/f'moe-qwen-branchless-graphs-{mode}.bin')],env=env,capture_output=True,text=True,timeout=180)
 (r/'results'/f'moe-qwen-branchless-graphs-{mode}.log').write_text(p.stdout+p.stderr);print(mode,p.returncode,p.stdout,p.stderr[-1800:],flush=True);p.check_returncode();runs[mode]=json.loads(p.stdout.strip())
 witness[mode]=[{'component':a,'mode':int(b),'kind':int(c),'calls':int(d),'rows':int(e),'blocks':int(f)} for a,b,c,d,e,f in re.findall(r'FG139_MATH component=(\w+) mode=(\d+) kind=(\d+) calls=(\d+) rows=(\d+) dot_blocks=(\d+)',p.stderr)]
a=(r/'results/moe-qwen-branchless-graphs-native.bin').read_bytes();b=(r/'results/moe-qwen-branchless-graphs-off.bin').read_bytes();c=(r/'results/moe-qwen-branchless-graphs-on.bin').read_bytes()
if a!=b or a!=c:
 other=b if a!=b else c;i=next(i for i in range(min(len(a),len(other))) if a[i]!=other[i]);fail={'status':'FAIL_GRAPH_BYTES','arm':'off' if a!=b else 'on','offset':i,'native':a[max(0,i-8):i+16].hex(),'actual':other[max(0,i-8):i+16].hex()};(r/'results/moe-qwen-branchless-graphs-summary.json').write_text(json.dumps(fail,indent=2)+'\n');raise AssertionError(fail)
assert not witness['off'] and all(any(x['kind']==kind and x['component']=='cpu' for x in witness['on']) for kind in range(5))
assert all(any(x['kind']==kind and x['component']=='base' and x['mode']==2 for x in witness['on']) for kind in range(5))
assert any(x['component']=='cpu' and x['mode']==1 and x['kind']==4 for x in witness['on'])
report={'status':'PASS_REAL_GGML_QWEN_ALL_FIVE_COMPUTE_DEQUANT_FUSED','runs':runs,'witness':witness,'all_bytes_equal_unmodified_native':True,'output_sha256':hashlib.sha256(a).hexdigest(),'bytes_compared':len(a),'preserved':'Original explicit verification window, cold/bank/tiled/thread scheduling; all five CPU and base dequant computations execute Goose','real_model_inference':'Not yet tested in this stage','production_changed':False}
(r/'results/moe-qwen-branchless-graphs-summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({'status':report['status'],'graphs':runs['on']['graph_cases'],'bytes_compared':len(a),'live_goose_groups':len(witness['on'])},indent=2))
