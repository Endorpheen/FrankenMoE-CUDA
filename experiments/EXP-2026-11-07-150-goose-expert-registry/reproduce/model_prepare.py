from pathlib import Path
import json,subprocess,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107');old=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107');s=(old/'model_check.py').read_text().replace('franken-goose-exp149-cpu-control-20261107',A.name).replace('EXP149','EXP150').replace('exp149','exp150')
s=s.replace("assert json.loads((r/'results/inherited-quant-evidence.json').read_text())['status']=='PASS_EXACT_UNCHANGED_QUANT_SOURCE_AND_GENERATED_C'", "assert json.loads((r/'results/release/cache-summary.json').read_text())['status']=='PASS_EXPERT_CACHE_NATIVE_DIFFERENTIAL'")
s=s.replace("assert json.loads((r/'results/extra-summary.json').read_text())['status'].startswith('PASS_')", "assert json.loads((r/'results/release/registry-summary.json').read_text())['status'].startswith('PASS_')")
s=s.replace("assert json.loads((r/'results/tsan-summary.json').read_text())['status']=='PASS_SCOPED_PTHREAD_TSAN'", "assert json.loads((r/'results/hbm-mock/summary.json').read_text())['status'].startswith('PASS_')")
s=s.replace("assert json.loads((r/'results/pthread-sanitized/summary.json').read_text())['status'].startswith('PASS_')", "assert json.loads((r/'results/sanitized/cache-summary.json').read_text())['status'].startswith('PASS_')")
s=s.replace("choices=['omp','pthread']", "choices=['omp']")
s=s.replace("'EXP150 full CPU job/pool policy: one request per OMP/pthread mode, fixed profile,no optimization series'", "'EXP150 CPU registry/expert scratch/cache policy batch: one unchanged fixed Apex request,diagnostic,no performance series'")
s=s.replace("'checkpoint': '3e901fe plus isolated EXP150 CPU pool/job policy'", "'checkpoint': '2a0c4b7 plus isolated EXP150 registry/expert policy'")
s=s.replace("    print('Apex ready; sending", "    maps=Path(f'/proc/{server.pid}/maps').read_text();(out/(name+'-loaded-maps.txt')).write_text(maps)\n    assert str(bundle/'libggml-cpu.so.0.27.0') in maps\n    assert str(bundle/'libllama.so.0') in maps or str(bundle/'libllama.so.0.7.0') in maps\n    print('Apex ready; sending")
# Explicitly assert loader aliases and bind all unresolved symbols before any model load.
bundle=A/'variants/cpu-backend/run'
for stem in ['libggml-cpu','libggml-base']:
 assert all((bundle/(stem+suffix)).resolve()==bundle/(stem+'.so.0.27.0') for suffix in ['.so','.so.0'])
for path in [A/'producer.json',A/'source/cpu-backend/producer.json']:
 d=json.loads(path.read_text());d['frozen']=str(A/'compiler/stage3-f849233b999c');path.write_text(json.dumps(d,indent=2)+'\n')
(A/'model_check.py').write_text(s)
ld=subprocess.run(['ldd','-r',str(bundle/'llama-server')],env=dict(os.environ,LD_LIBRARY_PATH=str(bundle),LD_BIND_NOW='1'),capture_output=True,text=True)
(A/'results/loader-precheck.log').write_text(ld.stdout+ld.stderr);assert ld.returncode==0 and 'undefined symbol:' not in ld.stdout+ld.stderr
print('PASS loader aliases and relocation checks; one model request prepared')
