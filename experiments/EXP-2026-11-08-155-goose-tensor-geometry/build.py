from pathlib import Path
import hashlib,json,os,re,shutil,subprocess
F=Path('/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108');E=F/'experiments/EXP-2026-11-08-155-goose-tensor-geometry';S=E/'source';B=F/'runtime/exp155/build';R=F/'runtime/exp155/run';B.mkdir(parents=True,exist_ok=True);R.mkdir(parents=True,exist_ok=True)
P=Path('/home/end0/AI/LlamaServer/franken-goose-exp149-cpu-control-20261107');L=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');compiler=Path('/home/end0/AI/Fried-Goose/.cache/exp301-function-state/key-final-bootstrap-v1/stage3');assert hashlib.sha256(compiler.read_bytes()).hexdigest()=='f849233b999ca9e931ca40dcbee51e292ec9e553d66bed32e2a4af27d46bdf3a'
C=F/'runtime/compiler/stage3-f849233b999c';C.parent.mkdir(exist_ok=True);shutil.copyfile(compiler,C);C.chmod(0o755);commands=[]
def run(label,args,cwd=B):
 args=list(map(str,args));q=subprocess.run(args,cwd=cwd,capture_output=True,text=True,timeout=180);commands.append(dict(label=label,argv=args,cwd=str(cwd),exit_code=q.returncode));(B/'commands.json').write_text(json.dumps(commands,indent=2)+'\n');(B/(label+'.stdout')).write_text(q.stdout);(B/(label+'.stderr')).write_text(q.stderr);print(label,q.returncode,flush=True)
 if q.returncode:print(q.stderr[-6000:]);raise SystemExit(q.returncode)
run('selfhosted-emit',[C,'-o',S/'geometry-generated.c',S/'geometry.goose'])
generated=(S/'geometry-generated.c').read_text();main=generated[generated.rfind('static void gg_function_0('):];entry=re.search(r'(gg_function_\d+)\(t\d+, t\d+, t\d+, t\d+, base',main)
if entry is None:
 print(main[-1800:]);raise SystemExit('Identify actual generated entry before compiling')
entry=entry.group(1)
header='''#ifndef FG155_GEOMETRY_H
#define FG155_GEOMETRY_H
#include <stdatomic.h>
#include <stdlib.h>
#include <inttypes.h>
static inline int64_t fg155_ne(int64_t p,int64_t i){return ((const struct ggml_tensor *)(intptr_t)p)->ne[i];}
static inline uint64_t fg155_nb(int64_t p,int64_t i){return ((const struct ggml_tensor *)(intptr_t)p)->nb[i];}
static inline int64_t fg155_tensor_block(int64_t p){return ggml_blck_size(((const struct ggml_tensor *)(intptr_t)p)->type);}
static inline uint64_t fg155_tensor_bytes(int64_t p){return ggml_type_size(((const struct ggml_tensor *)(intptr_t)p)->type);}
static inline int64_t fg155_type_block(int64_t t){return ggml_blck_size((enum ggml_type)t);}
static inline uint64_t fg155_type_bytes(int64_t t){return ggml_type_size((enum ggml_type)t);}
static inline uint64_t fg155_alignment(void){return GGML_MEM_ALIGN;}
static inline void fg155_fail(void){GGML_ABORT("Invalid Goose tensor geometry");}
static int fg155_enabled=1;
static atomic_uint_fast64_t fg155_seen;
static _Thread_local uint64_t fg155_thread_seen;
__attribute__((constructor)) static void fg155_config(void){const char*p=getenv("GGML_GOOSE_TENSOR_GEOMETRY");fg155_enabled=!(p&&p[0]=='0'&&p[1]==0);}
#define main fg155_unused_generated_main
#include "geometry-generated.c"
#undef main
static inline int64_t fg155_geometry(int64_t mode,int64_t a,int64_t b,int64_t c){
 uint64_t bit=UINT64_C(1)<<mode;
 if(!(fg155_thread_seen&bit)){fg155_thread_seen|=bit;atomic_fetch_or_explicit(&fg155_seen,bit,memory_order_relaxed);}
 return FG155_ENTRY(mode,a,b,c,1);
}
__attribute__((visibility("default"))) uint64_t fg155_geometry_seen(void);
uint64_t fg155_geometry_seen(void){return atomic_load_explicit(&fg155_seen,memory_order_relaxed);}
__attribute__((destructor)) static void fg155_report(void){uint64_t m=fg155_geometry_seen();if(m)fprintf(stderr,"FG155_GEOMETRY goose_modes=0x%" PRIx64 "\\n",m);}
#endif
'''.replace('FG155_ENTRY',entry);(S/'geometry.h').write_text(header)
original=(L/'source/ggml/src/ggml.c').read_text();source=original
specs=[('ggml_nelements',0,'tensor','0','0'),('ggml_nrows',1,'tensor','0','0'),('ggml_nbytes',2,'tensor','0','0'),('ggml_nbytes_pad',3,'tensor','0','0'),('ggml_row_size',4,'type','ne','0'),('ggml_is_scalar',5,'tensor','0','0'),('ggml_is_vector',6,'tensor','0','0'),('ggml_is_matrix',7,'tensor','0','0'),('ggml_is_3d',8,'tensor','0','0'),('ggml_n_dims',9,'tensor','0','0'),('ggml_is_contiguous_m_n',10,'tensor','m','n'),('ggml_is_contiguously_allocated',11,'tensor','0','0'),('ggml_is_transposed',12,'tensor','0','0'),('ggml_is_permuted',13,'tensor','0','0'),('ggml_is_contiguous_channels',14,'tensor','0','0'),('ggml_is_contiguous_rows',15,'tensor','0','0'),('ggml_is_padded_1d',16,'tensor','0','0'),('ggml_is_empty',17,'tensor','0','0'),('ggml_are_same_shape',18,'t0','t1','0'),('ggml_are_same_stride',19,'t0','t1','0'),('ggml_can_repeat',20,'t0','t1','0'),('ggml_can_repeat_rows',21,'t0','t1','0'),('ggml_can_mul_mat',22,'t0','t1','0'),('ggml_can_out_prod',23,'t0','t1','0')]
for name,mode,a,b,c in specs:
 pat=r'(?m)^(?:static inline |static )?(?:int64_t|size_t|bool|int)\s+'+name+r'\s*\([^;{}]*\)\s*\{';m=re.search(pat,source);assert m,name
 pos=m.end();prefix='\n    if(fg155_enabled) return fg155_geometry('+str(mode)+',(int64_t)(intptr_t)('+a+'),(int64_t)(intptr_t)('+b+'),(int64_t)(intptr_t)('+c+'));\n'
 source=source[:pos]+prefix+source[pos:]
anchor='int64_t ggml_nelements(';pos=source.index(anchor);source=source[:pos]+'#include "geometry.h"\n\n'+source[pos:];(S/'ggml-geometry.c').write_text(source)
recipes=json.loads((P/'variants/cpu-backend/build/commands.json').read_text());r=next(r for r in recipes if r['label']=='base-ggml');args=r['argv'].copy();args[args.index('-c')+1]=str(S/'ggml-geometry.c');obj=B/'ggml-geometry.o';args[args.index('-o')+1]=str(obj);run('geometry-shared-core-build',args,Path(r['cwd']))
r=next(r for r in recipes if r['label']=='link-base');reference=r['argv'].copy();reference_out=B/'native-reference-relink.so.0.27.0';reference[reference.index('-o')+1]=str(reference_out);run('unchanged-baseline-relink-integrity',reference,Path(r['cwd']));expected=hashlib.sha256((F/'runtime/run/libggml-base.so').read_bytes()).hexdigest();assert hashlib.sha256(reference_out.read_bytes()).hexdigest()==expected,'Wrong inherited baseline link objects';args=r['argv'].copy();old=str(P/'variants/cpu-backend/build/base-ggml.o');assert args.count(old)==1;args=[str(obj) if x==old else x for x in args];out=R/'libggml-base-geometry-exp150base.so.0.27.0';assert not out.exists();args[args.index('-o')+1]=str(out);args+=['-Wl,-z,defs'];run('geometry-shared-base-link',args,Path(r['cwd']))
for f in (F/'runtime/run').iterdir():
 if f.is_file() or f.is_symlink():
  q=R/f.name
  if q.is_symlink():q.unlink()
  elif q.exists():raise RuntimeError(str(q))
  q.symlink_to(out if f.name.startswith('libggml-base') else f.resolve())
(S/'manifest.json').write_text(json.dumps(dict(native_source=str(L/'source/ggml/src/ggml.c'),native_source_sha256=hashlib.sha256(original.encode()).hexdigest(),compiler_sha256=hashlib.sha256(C.read_bytes()).hexdigest(),compiler='Latest completed full self-hosted compiler checkpoint;newer EXP302 observer is not a replacement compiler',entry=entry,transferred_functions=[x[0] for x in specs],public_contiguous_wrappers='Seven public contiguous helpers reach transferred shared m/n predicate',library=str(out),library_sha256=hashlib.sha256(out.read_bytes()).hexdigest(),native_rollback='GGML_GOOSE_TENSOR_GEOMETRY=0',production_changed=False),indent=2)+'\n');print('LINKED_ISOLATED_GOOSE_GEOMETRY',out)
