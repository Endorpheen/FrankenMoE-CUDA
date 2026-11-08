from pathlib import Path
import subprocess,json,shutil,hashlib,os,shlex
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';S=A/'source/cpu-repack';B=A/'build/cpu-repack';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');B.mkdir(exist_ok=True);records=[]
if not S.exists():shutil.copytree(O/'worker-6/cpu-final',S)
def run(label,args,cwd=S,env=None):
 p=subprocess.run([str(x) for x in args],cwd=cwd,env=env,capture_output=True,text=True,timeout=180);records.append({'label':label,'argv':[str(x) for x in args],'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6000:]);raise SystemExit(p.returncode)
 return p
p=S/'repack-unit.c';t=p.read_text();a=t.index('__attribute__((weak)) int fg006_repack_enabled');b=t.index('int fg006_repack_q4_Kx8',a)
t=t[:a]+'''static int fg006_repack_selected=1;
__attribute__((constructor)) static void fg006_repack_config(void) {
 const char *v=getenv("GGML_GOOSE_CPU_REPACK"),*legacy=getenv("FG006_REPACK_ROLLBACK");
 fg006_repack_selected=(!v || strcmp(v,"0")!=0) && (!legacy || strcmp(legacy,"1")!=0);
}
int fg006_repack_enabled(void) {return fg006_repack_selected;}
'''+t[b:]
t=t.replace('int64_t s_len = (int64_t) nr * nr + nc;','int64_t s_len = (int64_t)(nr-1) * nr + nc;');t+='\n__attribute__((destructor)) static void fg006_repack_finish(void) {fg006_repack_report();}\n';p.write_text(t)
run('repack-emit',[A/'compiler/stage3-f849233b999c','-o',S/'repack-generated.c',S/'repack.goose'])
proto='#include "repack-provider.h"\n'
t=(U/'ggml/src/ggml-cpu/repack.cpp').read_text();sig='static int repack_q4_K_to_q4_K_8_bl(';a=t.index(sig);b=t.index('\n    if (t->ne[1]',a);t=t[:b]+'''\n    static_assert(sizeof(block_q4_K)==144 && sizeof(block_q4_Kx8)==1152,"actual Q4_K repack ABI");
    if (fg006_repack_enabled() && interleave_block==8) {
        if (t->ne[1]%nrows_interleaved || t->ne[0]%8) return -1;
        return fg006_repack_q4_Kx8(t->data,data,nrow,nblocks);
    }
'''+t[b:];(S/'repack.cpp').write_text(proto+t)
t=(U/'ggml/src/ggml-cpu/arch/x86/repack.cpp').read_text()
for name,call in [('ggml_quantize_mat_q8_K_4x8','if (fg006_repack_enabled()) { GGML_ASSERT(fg006_quantize_mat_q8_K_4x8(x,vy,k)==0);return; }'),('ggml_gemv_q4_K_8x8_q8_K','if (fg006_repack_enabled()) {fg006_gemv_q4_K_8x8_q8_K(n,s,bs,vx,vy,nr,nc);return;}'),('ggml_gemm_q4_K_8x8_q8_K','if (fg006_repack_enabled()) {fg006_gemm_q4_K_8x8_q8_K(n,s,bs,vx,vy,nr,nc);return;}')]:
 a=t.index('void '+name+'(');b=t.index('{',a);t=t[:b+1]+'\n    static_assert(sizeof(block_q8_Kx4)==1168,"actual Q8_Kx4 ABI");\n    '+call+'\n'+t[b+1:]
(S/'x86-repack.cpp').write_text(proto+t)
commands=json.loads((U/'build-cuda/compile_commands.json').read_text())
for suffix,src,obj in [('/ggml-cpu/repack.cpp',S/'repack.cpp',B/'repack.o'),('/arch/x86/repack.cpp',S/'x86-repack.cpp',B/'x86-repack.o')]:
 row=next(x for x in commands if x['file'].endswith(suffix));args=shlex.split(row['command']);args[args.index('-c')+1]=str(src);args[args.index('-o')+1]=str(obj);args+=['-I'+str(S),'-I'+str(U/'ggml/src/ggml-cpu/arch/x86')];run('build-'+obj.stem,args,Path(row['directory']))
run('build-repack-unit',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O3','-march=native','-fPIC','-fexceptions','-ffp-contract=off','-fno-strict-aliasing','-I'+str(S),'-c',S/'repack-unit.c','-o',B/'unit.o'])
rows=json.loads((A/'build/cpu-kdot-numa/commands.json').read_text());r=next(x for x in rows if x['label']=='kdot-numa-cohort-link');args=r['argv'].copy();repl={str(U/'build-cuda/ggml/src/CMakeFiles/ggml-cpu.dir/ggml-cpu/repack.cpp.o'):str(B/'repack.o'),str(U/'build-cuda/ggml/src/CMakeFiles/ggml-cpu.dir/ggml-cpu/arch/x86/repack.cpp.o'):str(B/'x86-repack.o')};assert all(args.count(k)==1 for k in repl);args=[repl.get(x,x) for x in args];args+=[str(B/'unit.o')];out=R/('libggml-cpu-repack-'+hashlib.sha256(b''.join((B/(x+'.o')).read_bytes() for x in ['repack','x86-repack','unit'])).hexdigest()[:12]+'.so.0.27.0');args[args.index('-o')+1]=str(out);run('repack-cohort-link',args,Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0','libggml.so','libggml.so.0','libggml.so.0.27.0','libllama.so','libllama.so.0','libllama.so.0.7.0','libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0']:
 q=T/name
 if q.is_symlink():q.unlink()
 assert not q.exists();q.symlink_to(out if name.startswith('libggml-cpu.') else (R/name).resolve())
# Normal backend corpus allocates the weight tensor through the repack buft.
t=(O/'worker-6/cpu-closure/backend-kquant-fixture.c').read_text();t=t.replace('#include "ggml-cpu/quants.h"','#include "ggml-cpu/quants.h"\n#include "ggml-cpu/repack.h"\n#include "ggml-alloc.h"');t=t.replace('float * row = malloc((size_t) K * sizeof(float));','float * row = malloc((size_t) K * sizeof(float));\n    uint8_t *raw=malloc(ggml_nbytes(w));if(!raw)abort();');t=t.replace('(uint8_t *) w->data + (size_t) m * rowbytes','raw + (size_t) m * rowbytes');t=t.replace('    free(row);','    ggml_backend_tensor_set(w,raw,0,ggml_nbytes(w));free(raw);\n    free(row);',1);t=t.replace('const enum ggml_type types[] = { GGML_TYPE_Q4_K, GGML_TYPE_Q6_K };','const enum ggml_type types[] = { GGML_TYPE_Q4_K };').replace('ti < 2','ti < 1');t=t.replace('M = 17','M = 16');old='struct ggml_tensor * w = ggml_new_tensor_2d(c, types[ti], K, M);';new='''struct ggml_init_params wp={ggml_tensor_overhead(),NULL,true};
                struct ggml_context *wc=ggml_init(wp);if(!wc)abort();
                struct ggml_tensor * w = ggml_new_tensor_2d(wc, types[ti], K, M);
                struct ggml_backend_buffer *wb=ggml_backend_alloc_ctx_tensors_from_buft(wc,ggml_backend_cpu_repack_buffer_type());if(!wb)abort();''';assert old in t;t=t.replace(old,new);t=t.replace('                ggml_free(c);','                ggml_free(c);ggml_backend_buffer_free(wb);ggml_free(wc);');p=S/'normal-backend-fixture.c';p.write_text(t)
run('repack-fixture-build',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1','-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src'),p,'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-cpu','-lggml-base','-lm','-lpthread','-o',B/'normal-backend-fixture'])
arms=[]
for flag in ['0','1']:
 env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(T)+':'+str(R),GGML_GOOSE_CPU_REPACK=flag);env.pop('FG006_REPACK_ROLLBACK',None);dest=B/('backend-'+flag+'.jsonl');p=run('repack-backend-'+flag,[B/'normal-backend-fixture',dest],env=env);items=[json.loads(x) for x in dest.read_text().splitlines()];assert items[-1]['failures']==0;arms.append({x['name']:x['hash'] for x in items if 'name' in x})
assert arms[0]==arms[1],[(k,arms[0].get(k),v) for k,v in arms[1].items() if arms[0].get(k)!=v]
err=(B/'repack-backend-1.stderr').read_text();assert all('FG006_REPACK mode='+str(i) in err for i in [0,1,2,3]),err[-3500:]
summary={'status':'ACCEPTED_SOURCE_LINKED_Q4K_REPACK_NUMERICS','library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'normal_backend_cases':len(arms[0]),'bit_exact':True,'source':str(S),'default_on':True,'rollback':'GGML_GOOSE_CPU_REPACK=0','provider_control_disabled_for_test':False,'preload':False,'production_changed':False,'model_loads':0,'remaining':'Repack provider/traits control closure and full Batch A Apex gate'};(A/'results/accepted-cpu-repack.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('PASS real CPU_REPACK source integration;normal graph exact native/Goose;full backend control retained.')
