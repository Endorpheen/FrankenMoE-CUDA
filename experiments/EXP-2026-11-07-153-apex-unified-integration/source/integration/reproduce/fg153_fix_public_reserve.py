from pathlib import Path
import json,subprocess,hashlib,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/batch-allocr';B=A/'build/batch-reserve-fix';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-4500:]);raise SystemExit(p.returncode)
 return p
p=D/'llama-batch-copy.cpp';s=p.read_text();start=s.index('llama_ubatch llama_batch_allocr::fg153_goose_ubatch_reserve(');end=s.index('\nvoid llama_batch_allocr::fg153_install_test(',start);part=s[start:end];assert part.count('    fg153_import_state(s);')==1;part=part.replace('    fg153_import_state(s);','''    clear();
    fg153_ba_put(s,1,0,0);fg153_ba_put(s,2,0,0);
    fg153_ba_put(s,3,0,n_seqs);fg153_ba_put(s,4,0,n_pos_per_embd);
    fg153_ba_put(s,9,0,1);fg153_ba_put(s,10,0,0);fg153_ba_put(s,11,0,0);''');s=s[:start]+part+s[end:];p.write_text(s)
incs=['-I'+str(D/'shim'),'-I'+str(D),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')];run('reserve-fix-cpp',['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-DFG153_BATCH_ALLOCR_GOOSE',*incs,'-c',str(p),'-o',str(B/'batch.o')])
rows=json.loads((A/'build/graph-result/commands.json').read_text());r=next(x for x in rows if x['label']=='result-cohort-link');args=r['argv'].copy();old=str(A/'build/batch-allocr/batch.o');assert args.count(old)==1;args=[str(B/'batch.o') if x==old else x for x in args];fingerprint=hashlib.sha256(b''.join((Path(x) if Path(x).is_absolute() else Path(r['cwd'])/x).read_bytes() for x in args if x.endswith('.o'))).hexdigest()[:12];out=R/('libllama-apex-reserve-'+fingerprint+'.so.0.7.0');args[args.index('-o')+1]=str(out);run('reserve-fix-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
probe=B/'fixture.cpp';probe.write_text('''#include "llama-batch.h"
#include <cstdio>
#include <cstdlib>
int main(){for(int enabled:{0,1}){setenv("GGML_GOOSE_BATCH_ALLOCR",enabled?"1":"0",1);for(unsigned npos:{1u,4u}){llama_batch_allocr a(npos);auto u=a.ubatch_reserve(8,2);if(u.n_tokens!=16||u.n_seq_tokens!=8||u.n_seqs!=2||u.n_seqs_unq!=2||u.n_pos!=npos||!u.token||!u.pos||u.embd||u.embd_state||u.seq_id_unq[0]!=0||u.seq_id_unq[1]!=1||u.seq_idx[0]!=0||u.seq_idx[1]!=1)return 2;printf("RESERVE npos=%u tokens=%u seqs=%u\\n",npos,u.n_tokens,u.n_seqs);}}}
''');run('reserve-fix-fixture-build',['/usr/bin/g++-13','-std=c++17','-O1','-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),str(probe),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'fixture')]);env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R);p=run('reserve-fix-fixture',[str(B/'fixture')],env=env)
summary={'status':'ACCEPTED_PUBLIC_UBATCH_RESERVE_FIX','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'issue':'Fresh allocr public reserve must not import uninitialized batch fields before clear','fix':'Initialize only actual reserve metadata from arguments/constant n_pos;force token buffer contract','fixture':p.stdout,'model_requests':0,'production_changed':False};(A/'results/accepted-public-reserve-fix.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Fixed normal public reserve used by sequence state restore;native/Goose agree.')
