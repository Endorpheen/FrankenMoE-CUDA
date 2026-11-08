from pathlib import Path
import subprocess,json,os,hashlib
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/recurrent-memory';B=A/'build/recurrent-memory';R=A/'variants/cpu-backend/run';T=B/'run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda')
text=(A/'source/hybrid-inputs/fixture.cpp').read_text();text='#include "bridge.h"\n'+text;needle=' puts("PASS real CPU hybrid buffers, split/apply/sequence/checkpoint/failure cleanup; no model inference");}'
assert needle in text
checks=' for(auto point:std::vector<std::pair<int,int>>{{0,10},{0,11},{1,1},{1,6},{1,7},{1,32},{2,0},{2,1}}){auto n=fg153_recurrent_witness(point.first,point.second);CHECK(n>0);fprintf(stderr,"FG153_RECURRENT family=%d mode=%d calls=%llu\\n",point.first,point.second,(unsigned long long)n);}'
text=text.replace(needle,checks+needle)
old='setenv("GGML_GOOSE_HYBRID_INPUT",goose?"1":"0",1);';assert text.count(old)==1;text=text.replace(old,old+'setenv("FG153_GOOSE_HOOKS",goose?"1":"0",1);');file=D/'hybrid-fixture.cpp';file.write_text(text)
records=[]
for label,args in [('recurrent-hybrid-fixture-build',['/usr/bin/g++-13','-std=c++17','-O1','-I'+str(D/'patched'),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),str(file),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'hybrid-fixture')]),('recurrent-hybrid-fixture',[str(B/'hybrid-fixture')])]:
 env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R);p=subprocess.run(args,env=env,capture_output=True,text=True,timeout=90);records.append({'label':label,'argv':args,'exit_code':p.returncode});(B/'hybrid-commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6000:]);raise SystemExit(p.returncode)
for family in [0,1,2]:assert f'FG153_RECURRENT family={family} ' in p.stderr,family
lib=T/'libllama.so';summary={'status':'PASS_REAL_HYBRID_RECURRENT_GOOSE_COHORT','library':str(lib.resolve()),'sha256':hashlib.sha256(lib.read_bytes()).hexdigest(),'stdout':p.stdout,'witness':p.stderr,'model_requests':0,'production_changed':False,'scope':'Actual Goose constructor allocation,prepare/find_slot,direct hybrid batch context,next/apply and state IO integrated with qualified hybrid and graph inputs','pending':['native ABI object/container storage remains until canonical Goose cleanup','no Apex inference/performance gate yet'],'rollback':'FG153_GOOSE_HOOKS=0'}
(A/'results/recurrent-hybrid-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(lib.resolve())
print('Actual hybrid/recurrent cohort correctness passed; explicit remaining native paths retained in evidence.')
