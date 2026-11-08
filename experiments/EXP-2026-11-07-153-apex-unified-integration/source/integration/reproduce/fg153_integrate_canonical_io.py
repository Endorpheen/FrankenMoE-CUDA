from pathlib import Path
import json,subprocess,shutil,re,os
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';S=A/'source/canonical-io';B=A/'build/canonical-io';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);records=[]
if not S.exists():shutil.copytree(O/'worker-3/canonical-io',S)
def run(label,args):
 p=subprocess.run([str(x) for x in args],cwd=S,capture_output=True,text=True,timeout=150);records.append({'label':label,'argv':[str(x) for x in args],'cwd':str(S),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6000:]);raise SystemExit(p.returncode)
 return p
rename={x:'fgc_i64_'+x[4:] for x in ['fgc_record_free','fgc_vec_free','fgc_vec_data','fgc_map_free','fgc_map_get','fgc_set_free']}
for p in S.glob('*'):
 if p.suffix in ['.c','.h','.goose']:
  t=p.read_text()
  for old,new in rename.items():t=re.sub(r'\b'+old+r'\b',new,t)
  p.write_text(t)
p=S/'canonical-io.goose';t=p.read_text()
# Keep previous cache geometry intact until the new group has been compared.
t=t.replace('let a=rs(g,1,rg(g,1)+1); let b=rs(g,2,rg(g,2)+size);','let a=rs(g,7,rg(g,7)+1); let b=rs(g,8,rg(g,8)+size);')
t=t.replace('var i:i64=0;\n while i<n {let e=vat(l,i); let a=dev_group_add', 'var ri:i64=0; while ri<vl(gv) {let g=vat(gv,ri);rs(g,7,0);rs(g,8,0);ri=ri+1;}\n var i:i64=0;\n while i<n {let e=vat(l,i); let a=dev_group_add',1)
t=t.replace('let cnt=rg(g,1); let tot=rg(g,2);','let cnt=rg(g,7); let tot=rg(g,8);\n  if cnt>0 {',1)
t=t.replace('let oldbuf=raw(30,g,0,0,0);','let oldbuf=rg(g,4);')
t=t.replace('if need_new!=0 {\n    let oldbuf=rg(g,4);','if need_new!=0 {\n    let oldbuf=rg(g,4); if oldbuf!=0 {raw(38,oldbuf,0,0,0);}',1)
t=t.replace('  gi=gi+1;\n }\n let c1=vr(l,0); return 0;','  }\n  gi=gi+1;\n }\n var fi:i64=0;while fi<n {let e=vat(l,fi);rf(e);fi=fi+1;}\n let c1=vr(l,0); return 0;',1)
old=' let c1=vr(l,0); return 0;\n}\nfn flush';new=' let c1=vr(l,0);vf(tv);return 0;\n}\nfn flush';assert old in t;t=t.replace(old,new)
p.write_text(t)
p=S/'services.c';t=p.read_text();t=re.sub(r'^extern void ggml_backend_tensor_(get|set)\([^\n]*\);\n','',t,flags=re.M);t=t.replace('(const void *)(intptr_t) a, (void *)(intptr_t) b, (size_t) c, (size_t) d','(const ggml_tensor *)(intptr_t) a, (void *)(intptr_t) b, (size_t) c, (size_t) d').replace('(void *)(intptr_t) a, (const void *)(intptr_t) b, (size_t) c, (size_t) d','(ggml_tensor *)(intptr_t) a, (const void *)(intptr_t) b, (size_t) c, (size_t) d');t=t.replace('ggml_backend_tensor_get((const void *)(intptr_t) b,','ggml_backend_tensor_get((const ggml_tensor *)(intptr_t) b,').replace('ggml_backend_tensor_set((void *)(intptr_t) b,','ggml_backend_tensor_set((ggml_tensor *)(intptr_t) b,');p.write_text(t)
# Source-level prototypes match generated integer externs; no shared stack reset.
(S/'unit.c').write_text('''#include "canonical-io.h"
int64_t fgc_set_new(void);
int64_t fgc_set_insert(int64_t,int64_t);
int64_t fgc_set_contains(int64_t,int64_t);
int64_t fgc_i64_set_free(int64_t);
#define main fg153_canonical_io_unused_main
#include "generated.c"
#undef main
int64_t fg153_goose_canonical_entry(int64_t m,int64_t a,int64_t b,int64_t c,int64_t d,int64_t e) {
 return gg_function_1(m,a,b,c,d,e,0);
}
''')
(S/'set-abi.c').write_text('''#include "../canonical-storage-composed/foundation/canonical-storage.h"
int64_t fgc_i64_set_free(int64_t h) { fgc_set_free(h);return 0; }
''')
producer=A/'compiler/stage3-f849233b999c';run('io-emit',[producer,'-o',S/'generated.c',S/'canonical-io.goose'])
incs=['-I'+str(S),'-I'+str(U/'src'),'-I'+str(U/'include'),'-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')]
for src,name in [(S/'unit.c','unit'),(S/'abi.c','abi'),(S/'set-abi.c','set-abi')]:run('io-build-'+name,['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O2','-fPIC','-fexceptions','-Werror=implicit-function-declaration',*incs,'-c',src,'-o',B/(name+'.o')])
run('io-build-real-effects',['/usr/bin/g++-13','-x','c++','-std=c++17','-D_GNU_SOURCE','-O2','-fPIC',*incs,'-c',S/'services.c','-o',B/'real-effects.o'])
(S/'real-fixture.cpp').write_text('''#include "canonical-io.h"
#include "ggml.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
#include "ggml-alloc.h"
#include <cstdio>
#include <cstring>
#include <cstdint>
extern "C" int64_t fgc_live_allocs(void);
extern "C" int64_t fgc_live_bytes(void);
#define C(x) do {if (!(x)) {std::fprintf(stderr,"FAIL %d %s\\n",__LINE__,#x);return 1;}}while(0)
static int64_t h(void *p) {return (int64_t)(intptr_t)p;}
int main() {
 ggml_cpu_init();
 ggml_init_params ip={16384,nullptr,true};auto ctx=ggml_init(ip);C(ctx);
 auto src=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,4);auto dst=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,4);
 auto s0=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);auto s1=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);
 auto d0=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);auto d1=ggml_new_tensor_1d(ctx,GGML_TYPE_F32,2);
 auto buf=ggml_backend_alloc_ctx_tensors_from_buft(ctx,ggml_backend_cpu_buffer_type());C(buf);
 float in[4]={1.25f,-2.5f,3.75f,9.0f};ggml_backend_tensor_set(src,in,0,sizeof in);ggml_backend_tensor_set(s0,in,0,8);ggml_backend_tensor_set(s1,in+2,0,8);
 int64_t w=fgcio_create(5,0,0,0);C(w);
 for (int variant=0;variant<4;variant++) {
   if (variant==2) {C(fgcio_write_tensor(w,h(s0),8,0)==0);C(fgcio_write_tensor(w,h(s1),8,0)==0);}
   else C(fgcio_write_tensor(w,h(src),16,0)==0);
   C(fgcio_flush(w)==0);
   int64_t r=fgcio_create(6,0,0,w);C(r);
   if (variant==1 || variant==3) {C(fgcio_read_tensor(r,h(d0),8,0)==0);C(fgcio_read_tensor(r,h(d1),8,0)==0);}
   else C(fgcio_read_tensor(r,h(dst),16,0)==0);
   C(fgcio_flush(r)==0);float out[4]={};
   if (variant==1 || variant==3) {ggml_backend_tensor_get(d0,out,0,8);ggml_backend_tensor_get(d1,out+2,0,8);}else ggml_backend_tensor_get(dst,out,0,16);
   C(!std::memcmp(in,out,16));C(fgcio_dispose(r)==0);
 }
 C(fgcio_dispose(w)==0);C(fgc_live_allocs()==0 && fgc_live_bytes()==0);
 ggml_backend_buffer_free(buf);ggml_free(ctx);
 std::puts("PASS real GGML canonical device IO repeated/split/geometry-change and zero owned allocations");return 0;
}
''')
run('io-real-fixture-build',['/usr/bin/g++-13','-std=c++17','-O1',*incs,'-c',S/'real-fixture.cpp','-o',B/'real-fixture.o'])
run('io-real-link',['/usr/bin/g++-13',B/'real-fixture.o',B/'unit.o',B/'abi.o',B/'set-abi.o',B/'real-effects.o',A/'build/canonical-storage-composed/libfgc-apex-storage.a','-L'+str(R),'-Wl,-rpath,'+str(R),'-Wl,--gc-sections','-lggml-cpu','-lggml-base','-lm','-lpthread','-ldl','-o',B/'real-fixture'])
p=run('io-real-device-boundary',[B/'real-fixture'])
summary={'status':'PASS_CANONICAL_IO_REAL_GGML_AND_GOOSE_STORAGE','source':str(S),'stdout':p.stdout,'fixes':['writer queued entry records freed after flush','reader temporary buft vector freed','typed i64 foundation ABI bridges','old buffer read from group field instead of miscasting group record','old cached buffer freed on reallocation','pending versus committed geometry kept separately','real ggml effects use exact native tensor prototypes;no phantom C++ overload','no shared generated stack reset'],'model_loads':0,'production_changed':False,'remaining':'canonical IO consumption by normal context/shared runtime and failure/OOM closure'};(A/'results/canonical-io-real.json').write_text(json.dumps(summary,indent=2)+'\n')
print('PASS canonical IO real backend and actual Goose storage')
