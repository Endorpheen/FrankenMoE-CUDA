from pathlib import Path
import subprocess,json,shutil,os,hashlib,re
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';D=A/'source/batch-allocr';B=A/'build/batch-allocr';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');D.mkdir(exist_ok=True);B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
 return p
for f in (O/'worker-5/batch-allocr').iterdir():
 if f.is_file() and f.suffix in {'.cpp','.h','.c','.goose','.py','.json'}:shutil.copyfile(f,D/f.name)
p=D/'llama-batch-copy.cpp';s=p.read_text();needle='const int64_t s = (int64_t) (uintptr_t) fg153_ba_state_new();';assert s.count(needle)==4;s=s.replace(needle,'std::unique_ptr<fg153_ba_state,decltype(&fg153_ba_state_free)> owned(fg153_ba_state_new(),&fg153_ba_state_free);\n    const int64_t s=(int64_t)(uintptr_t)owned.get();');s=s.replace('    fg153_ba_state_free(reinterpret_cast<fg153_ba_state *>((uintptr_t) s));\n','');a=s.index('llama_ubatch llama_batch_allocr::fg153_goose_ubatch_reserve(');idx=s.index('    g_fg153_calls++;',a)+len('    g_fg153_calls++;');s=s[:idx]+'\n    fg153_import_state(s);'+s[idx:]
at=s.index('    std::unique_ptr<fg153_ba_state',s.index('bool llama_batch_allocr::fg153_goose_init('));s=s[:at]+'''    clear();
    this->vocab=&vocab;
    n_embd=batch_inp.n_embd>0?batch_inp.n_embd:batch_inp.n_embd_inp;
    n_embd_state=batch_inp.n_embd_state;
    n_seq_max=batch_inp.n_seq_max;
'''+s[at:]
at=s.index('    token_vec.assign',s.index('void llama_batch_allocr::fg153_export_state('));s=s[:at]+'''    n_embd=(uint32_t)fg153_ba_get(s,1,0);
    n_embd_state=(uint32_t)fg153_ba_get(s,2,0);
    n_seq_max=(uint32_t)fg153_ba_get(s,3,0);
    has_cpl=fg153_ba_get(s,7,0)!=0;
    is_embd_vec.resize((size_t)fg153_ba_call(70,s,23,0,0));
    for(size_t i=0;i<is_embd_vec.size();++i)is_embd_vec[i]=(int8_t)fg153_ba_call(71,s,23,(int64_t)i,0);
    decision_order.resize((size_t)fg153_ba_call(70,s,29,0,0));
    for(size_t i=0;i<decision_order.size();++i)decision_order[i]=(int32_t)fg153_ba_call(71,s,29,(int64_t)i,0);
'''+s[at:];p.write_text(s)
shim=D/'shim';shim.mkdir(exist_ok=True)
for h in (U/'src').glob('*.h'):
 p=shim/h.name
 if p.is_symlink():p.unlink()
 if not p.exists():p.symlink_to(D/'llama-batch-copy.h' if h.name=='llama-batch.h' else h)
run('batch-emit',[str(A/'compiler/stage3-f849233b999c'),'-o',str(D/'batch-allocr.c'),str(D/'batch-allocr.goose')]);gen=(D/'batch-allocr.c').read_text();tail=gen[gen.rfind('static void gg_function_0('):];assert 'gg_function_1(' in tail
incs=['-I'+str(shim),'-I'+str(D),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
run('batch-unit',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-Werror=implicit-function-declaration',*incs,'-c',str(D/'goose_unit.c'),'-o',str(B/'unit.o')])
for file,name in [('bridge.cpp','bridge'),('llama-batch-copy.cpp','batch')]:run('batch-cpp-'+name,['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-DFG153_BATCH_ALLOCR_GOOSE',*incs,'-c',str(D/file),'-o',str(B/(name+'.o'))])
rows=json.loads((A/'build/common-memory-io/commands.json').read_text());r=next(x for x in rows if x['label']=='common-cohort-link');args=r['argv'].copy();old='src/CMakeFiles/llama.dir/llama-batch.cpp.o';assert args.count(old)==1;args=[str(B/'batch.o') if x==old else x for x in args];args += [str(B/'bridge.o'),str(B/'unit.o')];out=R/('libllama-apex-batch-'+hashlib.sha256(b''.join((B/f).read_bytes() for f in ['batch.o','bridge.o','unit.o'])).hexdigest()[:12]+'.so.0.7.0');args[args.index('-o')+1]=str(out);run('batch-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
p=D/'harness.cpp';s=p.read_text().replace('/inputs/native/src/',str(U/'src')+'/');s=s.replace('const bool nok = native.init(ext, vocab, c.output_all);','setenv("GGML_GOOSE_BATCH_ALLOCR","0",1);\n    const bool nok = native.init(ext, vocab, c.output_all);\n    unsetenv("GGML_GOOSE_BATCH_ALLOCR");\n    llama_batch_allocr actual(1,false);\n    const bool actual_ok=actual.init(ext,vocab,c.output_all);\n    if(actual_ok!=nok || (nok && (actual.n_seq_max!=native.n_seq_max || actual.n_embd!=native.n_embd || actual.n_embd_state!=native.n_embd_state || actual.has_cpl!=native.has_cpl))){why="normal init metadata";return false;}');s=s.replace('const auto rn = run_native(an, c);','setenv("GGML_GOOSE_BATCH_ALLOCR","0",1);\n        const auto rn = run_native(an, c);\n        unsetenv("GGML_GOOSE_BATCH_ALLOCR");');p.write_text(s)
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
run('batch-fixture-build',['/usr/bin/g++-13','-std=c++17','-O1','-I'+str(D),'-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),str(p),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lggml-cpu','-lpthread','-ldl','-o',str(B/'fixture')]);p=run('batch-fixture',[str(B/'fixture')],env=env);assert 'OK all differential cases' in p.stdout
hybrid=A/'source/recurrent-memory/hybrid-fixture.cpp';inc=['-I'+str(A/'source/recurrent-memory/patched'),'-I'+str(U/'src'),'-I'+str(U/'src/models'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include')]
run('batch-real-hybrid-build',['/usr/bin/g++-13','-std=c++17','-O1',*inc,str(hybrid),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'hybrid-fixture')]);p=run('batch-real-hybrid',[str(B/'hybrid-fixture')],env=env)
summary={'status':'ACCEPTED_BATCH_ALLOCR_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':5,'minor_parent_fixes':['RAII per-call state prevents exception leaks','public reserve imports actual metadata'],'real_hybrid_stdout':p.stdout,'model_requests':0,'production_changed':False,'rollback':'GGML_GOOSE_BATCH_ALLOCR=0','residuals':['typed C++ storage projections and remaining batch_ext algorithms not yet canonical Goose','full Batch A Apex gate pending']};(A/'results/accepted-batch-allocr.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted actual batch class routing + hybrid shared cohort; no model run.')
