from pathlib import Path
import subprocess,json,hashlib,os,re
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');D=A/'source/common-memory-io';B=A/'build/common-memory-io';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);records=[]
def run(label,args,cwd=A,env=None):
 p=subprocess.run(args,cwd=cwd,env=env,capture_output=True,text=True,timeout=120);records.append({'label':label,'argv':args,'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(A/'results'/f'{label}.stdout').write_text(p.stdout);(A/'results'/f'{label}.stderr').write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-5000:]);raise SystemExit(p.returncode)
 return p
p=D/'fg153-common-hooks.cpp';s=p.read_text();a=s.index('    int64_t vh =' if '    int64_t vh =' in s else '    std::unique_ptr<fg153_buffer_visitor> vis',s.index('fg153_common_hook_get_buffers'));b=s.index('\n    return res;',a)
s=s[:a]+'''    std::unique_ptr<fg153_buffer_visitor> vis((fg153_buffer_visitor *)(intptr_t) fg153_cs(22,0,0,0));
    mem->state_write(*vis);
    auto release=[](void *p){fg153_cs(21,(int64_t)(intptr_t)p,0,0);};
    std::unique_ptr<void,decltype(release)> owned((void *)(intptr_t) fg153_goose_common_entry(4,(int64_t)(intptr_t)vis.get(),0,0,0,0),release);
    int64_t out=(int64_t)(intptr_t)owned.get();
    int64_t n=fg153_cg(out,11,0);
    std::vector<ggml_backend_buffer_t> res;
    res.reserve((size_t)n);
    for(int64_t i=0;i<n;++i)res.push_back((ggml_backend_buffer_t)(intptr_t)fg153_cg(out,10,i));'''+s[b:];p.write_text(s)
p=D/'services.cpp';s=p.read_text();a=s.index('struct fg153_cbox');b=s.index('// fg153_buffer_visitor',a)
s=s[:a]+'''struct fg153_cbox {
    uint64_t magic=FG153_BOX_MAGIC;
    std::vector<int64_t> values;
    int64_t count=0,total=0;
    explicit fg153_cbox(size_t n):values(n) {}
};

'''+s[b:];a=s.index('int64_t fg153_cg(');b=s.index('static std::atomic<uint64_t>',a)
s=s[:a]+'''int64_t fg153_cg(int64_t p,int64_t k,int64_t i) {
    if(!p)return 0;
    auto *b=(fg153_cbox *)(intptr_t)p;if(b->magic!=FG153_BOX_MAGIC)std::abort();
    if(k==11)return b->count;if(k==12)return b->total;
    if(k!=10||i<0||(size_t)i>=b->values.size())std::abort();return b->values[(size_t)i];
}
void fg153_cp(int64_t p,int64_t k,int64_t i,int64_t v) {
    if(!p)std::abort();auto *b=(fg153_cbox *)(intptr_t)p;if(b->magic!=FG153_BOX_MAGIC)std::abort();
    if(k==11){b->count=v;return;}if(k==12){b->total=v;return;}
    if(k!=10||i<0||(size_t)i>=b->values.size())std::abort();b->values[(size_t)i]=v;
}

'''+s[b:];s=s.replace('new fg153_cbox((size_t)(a > 0 ? a : 8))','new fg153_cbox((size_t)a)')
if 'uint64_t fg153_common_witness(' not in s:s+='\nextern "C" uint64_t fg153_common_witness(int mode){return mode>=0&&mode<64?g_wit[mode].load(std::memory_order_relaxed):0;}\n'
p.write_text(s)
p=D/'common-memory.goose';s=p.read_text().replace('let out=cs(20,64,0,0);','let out=cs(20,n,0,0);');p.write_text(s)
p=D/'replay.py';s=p.read_text().replace('IROOT = "/inputs"','IROOT = '+repr(str(A/'helpers/omo5/inputs')));p.write_text(s);run('common-replay',['python3',str(p),'emit'])
run('common-emit',[str(A/'compiler/stage3-f849233b999c'),'-o',str(B/'common.c'),str(D/'common-memory.goose')]);gen=(B/'common.c').read_text();tail=gen[gen.rfind('static void gg_function_0('):];assert re.search(r'gg_function_1\(',tail)
unit=D/'unit.c';unit.write_text('#include "bridge.h"\n#define main fg153_common_unused_main\n#include "'+str(B/'common.c')+'"\n#undef main\nint64_t fg153_goose_common_entry(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f){return gg_function_1(a,b,c,d,e,f,0);}\n')
assert not re.search(r'gg_stacks\[[^\]]+\]\.',gen[gen.find('static int64_t gg_function_1('):gen.rfind('static void gg_function_0(')]),'Hot shared stack state requires an actual per-call frame'
incs=['-I'+str(D),'-I'+str(D/'patched'),'-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
objects=[]
run('common-unit',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-Werror=implicit-function-declaration',*incs,'-c',str(unit),'-o',str(B/'unit.o')]);objects.append(B/'unit.o')
for source,name in [('patched/llama-memory.cpp','memory'),('patched/llama-io.cpp','io'),('services.cpp','services'),('fg153-common-hooks.cpp','hooks')]:
 obj=B/(name+'.o');run('common-cpp-'+name,['/usr/bin/g++-13','-std=c++17','-O2','-fPIC','-fno-strict-aliasing',*incs,'-c',str(D/source),'-o',str(obj)]);objects.append(obj)
rows=json.loads((A/'build/recurrent-memory/integration-commands.json').read_text());r=next(x for x in rows if x['label']=='recurrent-integrated-link');args=r['argv'].copy();replacement={'src/CMakeFiles/llama.dir/llama-memory.cpp.o':str(B/'memory.o'),'src/CMakeFiles/llama.dir/llama-io.cpp.o':str(B/'io.o')};assert all(args.count(k)==1 for k in replacement);args=[replacement.get(x,x) for x in args];args += [str(x) for x in objects if x.name not in ['memory.o','io.o']];out=R/('libllama-apex-common-'+hashlib.sha256(b''.join(x.read_bytes() for x in objects)).hexdigest()[:12]+'.so.0.7.0');args[args.index('-o')+1]=str(out);run('common-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for f in R.iterdir():
 if f.is_file() or f.is_symlink():
  q=T/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise SystemExit('Refusing '+str(q))
  q.symlink_to(out if f.name in ['libllama.so','libllama.so.0','libllama.so.0.7.0'] else f.resolve())
env=os.environ.copy();env['LD_LIBRARY_PATH']=str(T)+':'+str(R)
run('common-fixture-build',['/usr/bin/g++-13','-std=c++17','-O1',*incs,str(D/'fixture.cpp'),'-L'+str(T),'-Wl,-rpath,'+str(T),'-lllama','-lggml','-lggml-base','-lm','-o',str(B/'fixture')]);p=run('common-fixture',[str(B/'fixture')],env=env);assert 'FG153_RESULT failures=0' in p.stdout
p=run('common-witness',['python3','-c','import ctypes;L=ctypes.CDLL('+repr(str(out))+');getattr(L,"_Z27llama_memory_status_combine19llama_memory_statusS_")(0,1);assert L.fg153_common_witness(0)>0;print("PASS actual public status hook reaches Goose")'],env=env)
summary={'status':'ACCEPTED_COMMON_MEMORY_IO_LOCAL_COHORT','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'source_worker':3,'minor_parent_fixes':['RAII visitor/result on vector exceptions','preallocated typed output storage avoids allocation exceptions inside Goose loops','removed shared gg_stacks reset'],'model_requests':0,'production_changed':False,'rollback':'FG153_GOOSE_COMMON=0','pending':'Batch A full Apex gate'};(A/'results/accepted-common-memory-io.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libllama.so','libllama.so.0','libllama.so.0.7.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted common IO/status and buffer collection into actual shared runtime.')
