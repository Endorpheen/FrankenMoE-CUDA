from pathlib import Path
import json,subprocess,shlex,hashlib
r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');s=r/'source/moe-quantized';original=Path('/home/end0/AI/LlamaServer/franken-qwen38-v31/source');nativebuild=r/'variants/qwen839a-shared/build';v=r/'variants/qwen839a-moe-quantized';(v/'build').mkdir(parents=True,exist_ok=True);(v/'run').mkdir(exist_ok=True)
names=['q2_0','q5_0','iq4_nl','iq2_xxs','iq2_s'];dotnames=['ggml_vec_dot_'+n+('_q8_0' if i<3 else '_q8_K') for i,n in enumerate(names)];deqnames=['dequantize_row_'+n for n in names]
cpp='''#include "ggml-cpu/quants.h"
#include "matrix-bridge.h"
#include <stdlib.h>
#include <string.h>
static int fg139_math_active=1;
int fg139_math_enabled(void);
__attribute__((constructor))static void fg139_math_config(void){const char*v=getenv("GGML_GOOSE_MOE_MATH");fg139_math_active=!v||strcmp(v,"0");}
__attribute__((visibility("hidden")))int fg139_math_enabled(void){return fg139_math_active;}
extern void fg139_quantize(int,const float*,void*,int64_t);
void quantize_row_q8_0(const float*x,void*y,int64_t n){fg139_quantize(0,x,y,n);}
void quantize_row_q8_K(const float*x,void*y,int64_t n){fg139_quantize(1,x,y,n);}
'''
signature='int n,float*s,size_t bs,const void*x,size_t bx,const void*y,size_t by,int nrc'
for kind,name in enumerate(dotnames):
 cpp+='extern void fg139_native_'+name+'('+signature+');\n'
 cpp+='void '+name+'('+signature+'){if(!fg139_math_active||nrc!=1){fg139_native_'+name+'(n,s,bs,x,bx,y,by,nrc);return;}fg139_dot('+str(kind)+',n,x,y,s);}\n'
(s/'qwen-public-bridge.c').write_text(cpp)
deq='''#include "ggml-quants.h"
#include "matrix-bridge.h"
#include <stdlib.h>
#include <string.h>
static int fg139_dequant_active=1;
__attribute__((constructor))static void fg139_dequant_config(void){const char*v=getenv("GGML_GOOSE_MOE_MATH");fg139_dequant_active=!v||strcmp(v,"0");}
'''
for kind,name in enumerate(deqnames):
 block='block_'+names[kind]
 deq+=f'extern void fg139_native_{name}(const {block}*,float*,int64_t);\n'
 deq+=f'void {name}(const {block}*x,float*y,int64_t n)'+'{'+f'if(!fg139_dequant_active)fg139_native_{name}(x,y,n);else fg139_dequantize({kind},x,y,n);'+'}\n'
(s/'qwen-dequant-bridge.c').write_text(deq)
core=(original/'ggml/src/ggml-cpu/ggml-cpu.c').read_text()
services='''#include "matrix-bridge.h"
extern int fg139_math_enabled(void);
static int fg139_quant_kind(enum ggml_type type){
 switch(type){case GGML_TYPE_Q2_0:return 0;case GGML_TYPE_Q5_0:return 1;case GGML_TYPE_IQ4_NL:return 2;case GGML_TYPE_IQ2_XXS:return 3;case GGML_TYPE_IQ2_S:return 4;default:return -1;}
}
static bool fg139_try_chunk(int mode,struct ggml_tensor*dst,const struct ggml_tensor*src0,const struct ggml_tensor*src1,const struct ggml_tensor*ids,int64_t expert,int64_t first,int64_t last,int64_t cfirst,int64_t clast,const char*x,const struct mmid_row_mapping*matrix_rows,size_t row_size,bool src1_cont,const void*wdata){
 if(!fg139_math_enabled())return false;
 const int kind=fg139_quant_kind(src0->type);if(kind<0||src0->extra!=NULL||src0->ne[1]<=0)return false;
 const enum ggml_type ytype=type_traits_cpu[src0->type].vec_dot_type;
 if(ytype!=(kind<3?GGML_TYPE_Q8_0:GGML_TYPE_Q8_K))return false;
 const int64_t xsize=ggml_type_size(src0->type),ysize=ggml_type_size(ytype),qk=ggml_blck_size(src0->type);
 const bool linear=mode==1||src1_cont||src1->type!=ytype;
 if(src0->nb[1]%xsize||row_size%ysize||src0->ne[0]%qk||dst->nb[1]%4||dst->nb[2]%4)return false;
 if(!linear&&(src1->nb[1]%ysize||src1->nb[2]%ysize))return false;
 struct fg139_task p={0};p.mode=mode;p.kind=kind;p.blocks=src0->ne[0]/qk;p.rows=last-first;p.weight_stride=src0->nb[1]/xsize;p.first=first;p.cols=clast-cfirst;p.slots=src1->ne[1];p.dst_stride1=dst->nb[1]/4;p.dst_stride2=dst->nb[2]/4;p.route_first=cfirst;p.input_contiguous=linear;p.input_row_blocks=row_size/ysize;p.input_stride1=src1->nb[1]/ysize;p.input_stride2=src1->nb[2]/ysize;p.output_stride=1;
 const int64_t nx=(src0->ne[1]-1)*p.weight_stride+p.blocks;
 const int64_t ny=linear?(int64_t)(row_size*src1->ne[1]*src1->ne[2]*src1->ne[3]/ysize):(int64_t)(ggml_nbytes(src1)/ysize);
 const int64_t capacity=ids->ne[0]*ids->ne[1];
 fg139_execute(&p,x,nx,wdata,ny,(float*)dst->data,ggml_nbytes(dst)/4,matrix_rows+expert*capacity,capacity);
 return true;
}
'''
needle='void ggml_compute_forward_mul_mat_id_one_chunk('
assert core.count(needle)==1;core=core.replace(needle,services+'\n'+needle,1)
def inject(text,name,statement):
 start=text.index(name+'(');begin=text.index('{',start)
 return text[:begin+1]+'\n'+statement+'\n'+text[begin+1:]
core=inject(core,'ggml_compute_forward_mul_mat_id_one_chunk','    if(fg139_try_chunk(3,dst,src0,src1,ids,cur_a,ir0_start,ir0_end,ir1_start,ir1_end,src0_cur,matrix_rows,row_size,src1_cont,wdata))return;')
core=inject(core,'ggml_compute_forward_mul_mat_id_iq2s_one_chunk_e46','    if(fg139_try_chunk(1,dst,src0,src1,ids,cur_a,ir0_start,ir0_end,0,cne1,src0_cur,matrix_rows,row_size,true,wdata))return;')
(s/'ggml-cpu-qwen-quant.c').write_text(core)
internal=(s/'matrix-unit.c').read_text()
internal=internal.replace('#ifndef FG139_MATH_TRACE','#ifndef FG139_COMPONENT\n#define FG139_COMPONENT "standalone"\n#endif\n#ifndef FG139_MATH_TRACE',1)
internal=internal.replace('"FG139_MATH mode=%d kind=%d calls=%llu rows=%llu dot_blocks=%llu\\n",m,k,','"FG139_MATH component=%s mode=%d kind=%d calls=%llu rows=%llu dot_blocks=%llu\\n",FG139_COMPONENT,m,k,')
assert 'component=%s' in internal
(s/'matrix-internal-unit.c').write_text(internal)
compile_db=json.loads((nativebuild/'compile_commands.json').read_text());commands=[]
def native_compile(suffix,source,output,extra=[]):
 row=next(x for x in compile_db if x['file'].endswith('/'+suffix));cmd=shlex.split(row['command'])
 cmd[cmd.index('-c')+1]=str(source);cmd[cmd.index('-o')+1]=str(v/'build'/output);cmd+=extra;return cmd
def run(label,cmd,cwd=nativebuild):
 p=subprocess.run(cmd,cwd=cwd,capture_output=True,text=True);(v/'build'/f'{label}.log').write_text(p.stdout+p.stderr);print(label,p.returncode,(p.stdout+p.stderr)[-1800:],flush=True);p.check_returncode();commands.append({'label':label,'cwd':str(cwd),'argv':cmd})
dot_renames=['-D'+name+'=fg139_native_'+name for name in dotnames]
quant_renames=['-D'+name+'=fg139_native_'+name for name in ['quantize_row_q8_0','quantize_row_q8_K']]
run('native-arch-renamed',native_compile('ggml-cpu/arch/x86/quants.c',original/'ggml/src/ggml-cpu/arch/x86/quants.c','native-arch-renamed.o',dot_renames+quant_renames))
run('native-dequant-renamed',native_compile('ggml-quants.c',original/'ggml/src/ggml-quants.c','native-dequant-renamed.o',['-D'+name+'=fg139_native_'+name for name in deqnames]))
run('cpu-core',native_compile('ggml-cpu/ggml-cpu.c',s/'ggml-cpu-qwen-quant.c','cpu-core.o',['-I'+str(s)]))
run('cpu-bridge',native_compile('ggml-cpu/ggml-cpu.c',s/'qwen-public-bridge.c','cpu-bridge.o',['-I'+str(s)]))
run('base-bridge',native_compile('ggml-quants.c',s/'qwen-dequant-bridge.c','base-bridge.o',['-I'+str(s)]))
math=json.loads((r/'variants/moe-quantized/build/matrix-qwen-commands.json').read_text())[0]
for component in ['cpu','base']:
 cmd=[x.replace(str(s/'matrix-unit.c'),str(s/'matrix-internal-unit.c')) for x in math]+['-fvisibility=hidden','-DFG139_COMPONENT="'+component+'"']
 cmd[cmd.index('-o')+1]=str(v/'build'/('math-'+component+'.o'));run('math-'+component,cmd)
act=json.loads((r/'variants/moe-quantized/build/activation-commands.json').read_text())[0]+quant_renames+['-fvisibility=hidden'];act[act.index('-o')+1]=str(v/'build/activation.o');run('activation',act)
base_link=json.loads((r/'variants/qwen839a-goose/build/link-command.json').read_text())
base_link=[str(v/'run/libggml-base.so.0.27.0') if x==str(r/'variants/qwen839a-goose/run/libggml-base.so.0.27.0') else str(v/'build/native-dequant-renamed.o') if x.endswith('ggml-base.dir/ggml-quants.c.o') else '-Wl,--dependency-file='+str(v/'build/base-link.d') if x.startswith('-Wl,--dependency-file=') else x for x in base_link]
base_link+=[str(v/'build/base-bridge.o'),str(v/'build/math-base.o')];run('base-link',base_link)
p=subprocess.run(['ninja','-C',str(nativebuild),'-t','commands','bin/libggml-cpu.so.0.27.0'],capture_output=True,text=True,check=True)
line=next(line for line in reversed(p.stdout.splitlines()) if '-shared' in line and 'bin/libggml-cpu.so.0.27.0' in line)
cpu_link=shlex.split(line);assert cpu_link[:2]==[':','&&'] and cpu_link[-2:]==['&&',':'];cpu_link=cpu_link[2:-2];assert not any(x in ['&&',';','|'] for x in cpu_link)
cpu_link=[str(v/'run/libggml-cpu.so.0.27.0') if x=='bin/libggml-cpu.so.0.27.0' else str(v/'build/cpu-core.o') if x.endswith('ggml-cpu.dir/ggml-cpu/ggml-cpu.c.o') else str(v/'build/native-arch-renamed.o') if x.endswith('ggml-cpu.dir/ggml-cpu/arch/x86/quants.c.o') else str(v/'run/libggml-base.so.0.27.0') if x=='bin/libggml-base.so.0.27.0' else '-Wl,--dependency-file='+str(v/'build/cpu-link.d') if x.startswith('-Wl,--dependency-file=') else x.replace(str(nativebuild/'bin'),str(v/'run')) if x.startswith('-Wl,-rpath,') else x for x in cpu_link]
cpu_link+=[str(v/'build/cpu-bridge.o'),str(v/'build/math-cpu.o'),str(v/'build/activation.o')];run('cpu-link',cpu_link)
for path in (r/'variants/qwen839a-goose/run').iterdir():
 if path.name.startswith(('libggml-cpu.so','libggml-base.so')):continue
 target=v/'run'/path.name
 if not target.exists() and not target.is_symlink():target.symlink_to(path)
for stem in ['libggml-cpu','libggml-base']:
 for suffix in ['.so','.so.0']:
  p=v/'run'/(stem+suffix)
  if not p.exists() and not p.is_symlink():p.symlink_to(stem+'.so.0.27.0')
(v/'build/commands.json').write_text(json.dumps(commands,indent=2)+'\n')
(v/'build/manifest.json').write_text(json.dumps({'status':'BUILT_NOT_GRAPH_OR_MODEL_TESTED','source_cpu_sha256':hashlib.sha256(core.encode()).hexdigest(),'native_controller_copy_sha256':hashlib.sha256((original/'ggml/src/ggml-cpu/ggml-cpu.c').read_bytes()).hexdigest(),'cpu_sha256':hashlib.sha256((v/'run/libggml-cpu.so.0.27.0').read_bytes()).hexdigest(),'base_sha256':hashlib.sha256((v/'run/libggml-base.so.0.27.0').read_bytes()).hexdigest(),'preserved':['Original explicit target-verification IQ2S fused window/guard','Native tiled/repack dispatch','Cold mask/chunk/thread scheduling and expert-ready hook','Existing Qwen-native Goose graph/allocator objects','CUDA ring and shared MTP libraries unchanged'],'new_compute':'Goose five-family dot/matrix/dequant/routed/fused math plus common Q8 packing','working_runtime_changed':False},indent=2)+'\n');print('Isolated Qwen CPU/base compute integration linked')
