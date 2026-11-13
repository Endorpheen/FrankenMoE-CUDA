from pathlib import Path
import subprocess,json,os,shutil,hashlib,struct
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s=r/'source/elementwise'
v=r/'variants/elementwise'
shutil.copy2('/tmp/fg_elementwise_fixture.c',s/'fixture.c')
cmd=json.loads((r/'variants/cpu-moe-tiled/build/fixture-compile.json').read_text())
cmd=[x.replace(str(r/'source/cpu-moe/fixture.c'),str(s/'fixture.c')).replace(str(r/'variants/cpu-moe-tiled/run'),str(v/'run')).replace(str(r/'variants/cpu-moe-tiled/build/moe-fixture'),str(v/'build/elementwise-fixture')) for x in cmd]
p=subprocess.run(cmd,capture_output=True,text=True)
(v/'build/fixture-compile.log').write_text(p.stdout+p.stderr)
print('fixture compile',p.returncode,(p.stdout+p.stderr)[-1500:],flush=True)
p.check_returncode()
(v/'build/fixture-compile.json').write_text(json.dumps(cmd,indent=2)+'\n')
runs={}
for mode in ['parent','off','on']:
 env=dict(os.environ);env.pop('LD_PRELOAD',None)
 env.update(LD_LIBRARY_PATH=str(r/'variants/cpu-moe-tiled-register-perf/run' if mode=='parent' else v/'run'),GGML_GOOSE_ELEMENTWISE='1' if mode=='on' else '0',GGML_GOOSE_ALLOC='0',GGML_GOOSE_GRAPH_PLAN='0',GGML_GOOSE_COALESCE='0',GGML_GOOSE_LIFETIME='0')
 p=subprocess.run([str(v/'build/elementwise-fixture'),str(r/'results'/f'elementwise-{mode}.bin')],env=env,capture_output=True,text=True,timeout=180)
 (r/'results'/f'elementwise-{mode}.jsonl').write_text(p.stdout)
 (r/'results'/f'elementwise-{mode}.stderr').write_text(p.stderr)
 print(mode,p.returncode,p.stdout.splitlines()[-1:] if p.stdout else '',p.stderr[-1300:],flush=True)
 p.check_returncode()
 runs[mode]=[json.loads(line) for line in p.stdout.splitlines()]
datasets={mode:(r/'results'/f'elementwise-{mode}.bin').read_bytes() for mode in runs}
metas=runs['parent'][:-1]
for mode in ['off','on']:
 assert runs[mode]==runs['parent'],'fixture metadata changed'
 if datasets[mode]!=datasets['parent']:
  offset=0
  for meta in metas:
   size=struct.unpack_from('<Q',datasets['parent'],offset)[0]
   end=offset+8+size
   a=datasets['parent'][offset+8:end];b=datasets[mode][offset+8:end]
   if a!=b:
    pos=next(i for i in range(min(len(a),len(b))) if a[i]!=b[i])
    failure={'status':'FAIL_EXACT_BYTES','mode':mode,'case':meta,'byte_in_tensor':pos,'expected_hex':a[max(0,pos-8):pos+16].hex(),'actual_hex':b[max(0,pos-8):pos+16].hex()}
    (r/'results/elementwise-correctness.json').write_text(json.dumps(failure,indent=2)+'\n')
    print(json.dumps(failure,indent=2));raise AssertionError('exact mismatch')
   offset=end
report={'status':'PASS_FULL_BINARY_UNARY_GGML_CORPUS','runs':{m:rows[-1] for m,rows in runs.items()},'all_bytes_equal_parent_original_cpp':True,'output_sha256':hashlib.sha256(datasets['parent']).hexdigest(),'bytes_compared':len(datasets['parent']),'native_recompile_control_equal':True,'producer_sha256':'81d6d066a675b282c72ee214d7154b709f3a3f581f72103a6b0790469f4f460f','scope':'Entire existing binary-ops.cpp and unary-ops.cpp modules; not GELU/SiLU/GLU elsewhere in ops.cpp','pending':['Sanitizers','Actual Apex witness and inference','Counter-free operation and model timing']}
(r/'results/elementwise-correctness.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
