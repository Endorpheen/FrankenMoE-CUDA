from pathlib import Path
import json,shutil,subprocess,hashlib,os,runpy,re
A=Path('/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107');O=A/'helpers/omo5';S=A/'source/cpu-kdot-numa';B=A/'build/cpu-kdot-numa';R=A/'variants/cpu-backend/run';U=Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda');S.mkdir(exist_ok=True);B.mkdir(exist_ok=True);records=[]
def run(label,args,env=None,cwd=A):
 p=subprocess.run([str(x) for x in args],cwd=cwd,env=env,capture_output=True,text=True,timeout=180)
 records.append({'label':label,'argv':[str(x) for x in args],'cwd':str(cwd),'exit_code':p.returncode});(B/'commands.json').write_text(json.dumps(records,indent=2)+'\n');(B/(label+'.stdout')).write_text(p.stdout);(B/(label+'.stderr')).write_text(p.stderr);print(label,p.returncode,flush=True)
 if p.returncode:print((p.stdout+p.stderr)[-6500:]);raise SystemExit(p.returncode)
 return p
for src,dst in [(O/'worker-6/cpu-closure',S/'kdot'),(O/'worker-8/threadpool',S/'numa')]:
 if not dst.exists():shutil.copytree(src,dst)
p=S/'kdot/kquant-unit.c';t=p.read_text();t=t.replace('#include <stdatomic.h>','#include <stdatomic.h>\n#include <stdlib.h>\nstatic int fg006_kdot_enabled=1;\n__attribute__((constructor)) static void fg006_kdot_config(void) {\n const char *v=getenv("GGML_GOOSE_CPU_KDOT");fg006_kdot_enabled=(!v || strcmp(v,"0")!=0);\n}\nvoid ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);\nvoid ggml_vec_dot_q6_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);')
for kind in ['q4','q6']:
 old=f'    (void) bs; (void) bx; (void) by; (void) nrc;\n    *s = fg006_dot_{kind}_K_q8_K(n, vx, vy);'
 new=f'''    if (!fg006_kdot_enabled) {{ ggml_vec_dot_{kind}_K_q8_K(n,s,bs,vx,bx,vy,by,nrc);return; }}
    if (n < 0 || n % 256 || nrc != 1 || !s) __builtin_trap();
    *s = fg006_dot_{kind}_K_q8_K(n, vx, vy);'''
 assert old in t;t=t.replace(old,new)
p.write_text(t)
producer=A/'compiler/stage3-f849233b999c';run('kdot-emit',[producer,'-o',S/'kdot/kquant-generated.c',S/'kdot/kquant.goose']);run('numa-emit',[producer,'-o',S/'numa/generated/numa-init.c',S/'numa/numa-init.goose'])
core=(A/'source/cpu-provider/native-cpu/ggml-cpu-integrated.c').read_text()
for kind in ['q4','q6']:
 old=f'.vec_dot                  = ggml_vec_dot_{kind}_K_q8_K,';new=f'.vec_dot                  = fg006_vec_dot_{kind}_K_q8_K,';assert core.count(old)==1;core=core.replace(old,new)
protos='\nvoid fg006_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);\nvoid fg006_vec_dot_q6_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);\n'
core=core.replace('#include <stdint.h>','#include <stdint.h>'+protos,1)
v=runpy.run_path(str(S/'numa/replay-numa-init.py'));sig=v['SIGNATURE'];pos=core.index(sig);brace=core.index('{',pos);core=core[:pos]+v['INCLUDE']+'\n_Static_assert(GGML_NUMA_MAX_NODES==FG153_NUMA_MAX_NODES,"NUMA node capacity");\n_Static_assert(GGML_NUMA_MAX_CPUS==FG153_NUMA_MAX_CPUS,"NUMA CPU capacity");\n'+core[pos:brace+1]+'\n'+v['DISPATCH']+core[brace+1:];(S/'ggml-cpu-integrated.c').write_text(core)
rows=json.loads((A/'build/cpu-provider/commands.json').read_text());r=next(x for x in rows if x['label']=='provider-core');args=r['argv'].copy();args[args.index('-c')+1]=str(S/'ggml-cpu-integrated.c');args[args.index('-o')+1]=str(B/'core.o');args+=['-I'+str(S/'numa'),'-I'+str(A/'source/cpu-provider/native-cpu')];run('kdot-numa-core',args,cwd=Path(r['cwd']))
common=['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O3','-march=native','-fPIC','-fexceptions','-fno-strict-aliasing','-ffp-contract=off','-Werror=implicit-function-declaration']
for src,name in [(S/'kdot/kquant-unit.c','kdot'),(S/'numa/numa-init-unit.c','numa-unit'),(S/'numa/numa-init-services.c','numa-services')]:
 run('build-'+name,[*common,'-DW8_WITNESS','-c',src,'-o',B/(name+'.o')])
r=next(x for x in rows if x['label']=='provider-cohort-link');args=r['argv'].copy();old=str(A/'build/cpu-provider/core.o');assert args.count(old)==1;args=[str(B/'core.o') if x==old else x for x in args];args +=[str(B/(x+'.o')) for x in ['kdot','numa-unit','numa-services']]
out=R/('libggml-cpu-kdot-numa-'+hashlib.sha256(b''.join((B/(x+'.o')).read_bytes() for x in ['core','kdot','numa-unit','numa-services'])).hexdigest()[:12]+'.so.0.27.0');args[args.index('-o')+1]=str(out);run('kdot-numa-cohort-link',args,cwd=Path(r['cwd']))
T=B/'run';T.mkdir(exist_ok=True)
for name in ['libggml-base.so','libggml-base.so.0','libggml-base.so.0.27.0','libggml.so','libggml.so.0','libggml.so.0.27.0','libllama.so','libllama.so.0','libllama.so.0.7.0','libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0']:
 q=T/name
 if q.is_symlink():q.unlink()
 assert not q.exists();q.symlink_to(out if name.startswith('libggml-cpu.') else (R/name).resolve())
p=S/'backend-kquant-fixture.c';t=(S/'kdot/backend-kquant-fixture.c').read_text().replace('ggml_cpu_init();','ggml_cpu_init();\n    ggml_numa_init(GGML_NUMA_STRATEGY_DISABLED);',1);p.write_text(t)
incs=['-I'+str(U/'ggml/include'),'-I'+str(U/'ggml/src')];run('kdot-numa-fixture-build',['/usr/bin/gcc-13','-std=c11','-D_GNU_SOURCE','-O1',*incs,p,'-L'+str(T),'-Wl,-rpath,'+str(T),'-lggml-cpu','-lggml-base','-lm','-lpthread','-o',B/'backend-fixture'])
arms=[]
for flag in ['0','1']:
 env=os.environ.copy();env.update(LD_LIBRARY_PATH=str(T)+':'+str(R),GGML_GOOSE_CPU_KDOT=flag,GGML_GOOSE_CPU_NUMA=flag);dest=B/('backend-'+flag+'.jsonl');p=run('kdot-numa-backend-'+flag,[B/'backend-fixture',dest],env=env);lines=[json.loads(x) for x in dest.read_text().splitlines()];assert lines[-1]['failures']==0;arms.append({x['name']:x['hash'] for x in lines if 'name' in x})
assert arms[0]==arms[1],[(k,arms[0].get(k),v) for k,v in arms[1].items() if arms[0].get(k)!=v]
err=(B/'kdot-numa-backend-1.stderr').read_text();assert 'FG006_KQUANT mode=0' in err and 'FG006_KQUANT mode=1' in err and 'FG153_NUMA mode=' in err
summary={'status':'ACCEPTED_COMPOSED_KDOT_NUMA_CPU_BOUNDARY','source':str(S),'library':str(out),'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'normal_backend_cases':len(arms[0]),'bit_exact_native_differential':True,'rollback':['GGML_GOOSE_CPU_KDOT=0','GGML_GOOSE_CPU_NUMA=0'],'production_changed':False,'model_loads':0,'pending':'Active Q4_Kx8 repack closure and full Batch A Apex gate'};(A/'results/accepted-cpu-kdot-numa.json').write_text(json.dumps(summary,indent=2)+'\n')
for name in ['libggml-cpu.so','libggml-cpu.so.0','libggml-cpu.so.0.27.0']:
 q=R/name;assert q.is_symlink();q.unlink();q.symlink_to(out.name)
print('Accepted actual type-trait Q4_K/Q6_K Goose dot + NUMA core hooks;old provider/control preserved.')
