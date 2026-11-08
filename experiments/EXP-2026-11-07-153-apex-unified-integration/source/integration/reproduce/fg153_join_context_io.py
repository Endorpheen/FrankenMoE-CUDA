from pathlib import Path
import json,subprocess,shutil,os,re
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';S=A/'source/context-io';B=A/'build/context-io';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');R=A/'variants/cpu-backend/run'
S.mkdir(exist_ok=True);B.mkdir(exist_ok=True)
for f in (O/'worker-3/canonical-context-io-adapter').iterdir():
 if f.is_file():shutil.copyfile(f,S/f.name)
p=S/'fgcio-context.goose';s=p.read_text();start=s.index('fn map_get(');end=s.index('fn bind(',start);s=s[:start]+'''extern "fgcio_map_value" fn map_get(m:i64,k:i64)->i64;
'''+s[end:]
start=s.index('fn reg_new(');end=s.index('fn ctx_read(',start)
s=s[:start]+'''fn reg_new()->i64 {
 let r=rn(3200); if r==0 {return 0;}
 let m=mn(); if m==0 {rf(r);return 0;}
 if rs(r,0,m)!=0||rs(r,1,0)!=0 {mf(m);rf(r);return 0;}
 return r;
}
fn ctx_get(reg:i64,ctx:i64,seq:i64,dst:i64,size:i64)->i64 {
 if reg==0||ctx==0||dst<=0||size<0 {return 0;}
 let m=rg(reg,0); var cr=map_get(m,ctx);
 if cr==0 {
  cr=rn(3201); if cr==0 {return 0;}
  let sm=mn();if sm==0 {rf(cr);return 0;}
  let sv=vn(8);if sv==0 {mf(sm);rf(cr);return 0;}
  if rs(cr,0,sm)!=0||rs(cr,1,sv)!=0 {vf(sv);mf(sm);rf(cr);return 0;}
  if mput(m,ctx,cr)!=0 {vf(sv);mf(sm);rf(cr);return 0;}
  let c=rs(reg,1,rg(reg,1)+1);
 }
 let sm=rg(cr,0);var w=map_get(sm,seq);
 if w==0 {
  let sv=rg(cr,1);let n=vl(sv);if vr(sv,n+1)!=0 {return 0;}
  w=io_create(5,dst,size,0);
  if w==0 {vr(sv,n);return 0;}
  if mput(sm,seq,w)!=0 {io_dispose(w);vr(sv,n);return 0;}
  raw(8,vd(sv)+8*n,w,0,0);return w;
 }
 if bind(w,dst,size)!=0 {return 0;}
 return w;
}
fn ctx_has(reg:i64,ctx:i64,seq:i64)->i64 {
 if reg==0 {return 0;}let cr=map_get(rg(reg,0),ctx);
 if cr==0 {return 0;}if map_get(rg(cr,0),seq)!=0 {return 1;}return 0;
}
'''+s[end:];s=s.replace('if mode==4 {return reg_new();}','if mode==4 {return reg_new();}\n if mode==5 {return ctx_has(a,b,c);}')
p.write_text(s)
p=S/'abi_context.c';s=p.read_text();s+='''\nextern int64_t fgc_map_get(int64_t,int64_t,int64_t *);
int64_t fgcio_map_value(int64_t h,int64_t key) {
 int64_t value=0;fgc_map_get(h,key,&value);return value;
}
int64_t fgcio_context_has(int64_t ctx,int64_t seq) {
 pthread_mutex_lock(&g_mu);
 int64_t r=g_reg?fgcio_context_entry(5,g_reg,ctx,seq,0,0):0;
 pthread_mutex_unlock(&g_mu);return r;
}
''';p.write_text(s)
p=S/'fgcio-context-adapter.h';s=p.read_text().replace('int64_t fgcio_context_drop(int64_t ctx);','int64_t fgcio_context_drop(int64_t ctx);\nint64_t fgcio_context_has(int64_t ctx,int64_t seq);');p.write_text(s)
p=S/'fixture.cpp';s=p.read_text().replace('    {\n        llama_io_write_i * io =','    size_t written = 0;\n    {\n        llama_io_write_i * io =',1).replace('        fgcio_adapter_free_write(io);','        written = io->n_bytes();\n        fgcio_adapter_free_write(io);',1).replace('fgcio_context_read_device(key, seq, (int64_t)(intptr_t) buf, (int64_t) cap)','fgcio_context_read_device(key, seq, (int64_t)(intptr_t) buf, (int64_t) written)');p.write_text(s)
records=[]
def run(label,args,env=None):
 args=[str(x) for x in args];q=subprocess.run(args,cwd=S,capture_output=True,text=True,env=env,timeout=180);records.append(dict(label=label,argv=args,cwd=str(S),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print((q.stdout+q.stderr)[-6000:]);raise SystemExit(q.returncode)
 return q
run('ctxio-emit',[A/'compiler/stage3-f849233b999c','-o',S/'generated.c',S/'fgcio-context.goose'])
g=(S/'generated.c').read_text();assert not re.search(r'gg_stacks\[[^\]]+\]\.',g[:g.rfind('static void gg_function_0(')])
(S/'unit.c').write_text('#include "canonical-io.h"\nextern int64_t fgcio_map_value(int64_t,int64_t);\n#define main fgcio_context_unused_main\n#include "generated.c"\n#undef main\nint64_t fgcio_context_entry(int64_t a,int64_t b,int64_t c,int64_t d,int64_t e,int64_t f){return gg_function_1(a,b,c,d,e,f,0);}\n')
incs=['-I'+str(S),'-I'+str(A/'source/canonical-io'),'-I'+str(U/'ggml/include'),'-I'+str(U/'src'),'-I'+str(U/'include')];objects=[]
for name,src in [('unit','unit.c'),('abi','abi_context.c'),('adapter','fgcio-context-adapter.cpp'),('fixture','fixture.cpp')]:
 cpp=src.endswith('.cpp');obj=B/(name+'.o');run('ctxio-build-'+name,['/usr/bin/g++-13' if cpp else '/usr/bin/gcc-13','-std=c++17' if cpp else '-std=c11','-D_GNU_SOURCE','-O2','-g','-fPIC','-fno-strict-aliasing',*incs,'-c',S/src,'-o',obj]);objects.append(obj)
objects += [A/'build/canonical-io'/x for x in ['unit.o','abi.o','set-abi.o','real-effects.o']]+[A/'build/canonical-storage-composed/libfgc-apex-storage.a']
run('ctxio-link',['/usr/bin/g++-13','-o',B/'fixture',*objects,'-L'+str(R),'-Wl,-rpath,'+str(R),'-lllama','-lggml','-lggml-cpu','-lggml-base','-lm','-lpthread','-ldl'])
env=dict(os.environ,LD_LIBRARY_PATH=str(R));q=run('ctxio-real-device',[B/'fixture'],env)
(A/'results/context-io-real.json').write_text(json.dumps(dict(status='PASS_CONTEXT_CANONICAL_IO_REAL_BACKEND',stdout=q.stdout,source=str(S),fixes=['Exact ON_DEVICE metadata length in read fixture','Allocation-free typed canonical map lookup','Registry allocation/insert rollback','Persistent writer ledger reservation before publication','Context-key isolation','No shared generated stack reset'],model_loads=0,production_changed=False),indent=2)+'\n')
print(q.stdout,flush=True)
