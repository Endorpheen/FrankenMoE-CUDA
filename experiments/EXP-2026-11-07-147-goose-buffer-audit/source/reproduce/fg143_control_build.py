from pathlib import Path
import hashlib
import json
import re
import shutil
import subprocess

r = Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107')
s = r / 'source/cpu-backend'
c = s / 'control'
c.mkdir(exist_ok=True)
b = r / 'variants/cpu-backend/build'
native = (r / 'source/cpu-tiled-all/ggml-cpu-apex-tiled.c').read_text()
original_tasks = native[native.index('static int ggml_get_n_tasks('):native.index('\nstatic thread_ret_t ggml_graph_compute_secondary_thread')]
original_tasks = re.sub(r'/\*.*?\*/|//[^\n]*', '', original_tasks, flags=re.S)
original_dispatch = native[native.index('static void ggml_compute_forward(struct'):native.index('// Android\'s libc implementation')]
producer = json.loads((s / 'producer.json').read_text())
reference = r / 'variants/cpu-backend-control-reference/run'
if not reference.exists():
    reference.mkdir(parents=True)
    for path in (r / 'variants/cpu-backend/run').iterdir():
        dest = reference / path.name
        if path.name == 'libggml-cpu.so.0.27.0':
            shutil.copy2(path, dest)
        elif path.name.startswith('libggml-cpu.so'):
            dest.symlink_to('libggml-cpu.so.0.27.0')
        else:
            dest.symlink_to(path.resolve())
    shutil.copytree(s, r / 'diagnostics/b1-278-before-cpu-control/source')
    for path in (r / 'generated').glob('cpu-*.c'):
        shutil.copy2(path, r / 'diagnostics/b1-278-before-cpu-control' / path.name)

special = {'UNARY', 'GLU', 'SOFT_MAX', 'RWKV_WKV6', 'RWKV_WKV7', 'GATED_LINEAR_ATTN',
           'MAP_CUSTOM1', 'MAP_CUSTOM2', 'MAP_CUSTOM3', 'CUSTOM', 'COUNT'}
single, parallel, pending = [], [], []
task_cases = list(re.finditer(r'case GGML_OP_([A-Z0-9_]+):', original_tasks))
for i, match in enumerate(task_cases):
    pending.append(match[1])
    body = original_tasks[match.end():task_cases[i+1].start() if i+1 < len(task_cases) else len(original_tasks)]
    if not body.strip():
        continue
    if set(pending) & special:
        assert all(x in special for x in pending)
    elif re.search(r'n_tasks\s*=\s*n_threads\s*;', body):
        parallel.extend(pending)
    elif re.search(r'n_tasks\s*=\s*1\s*;', body):
        single.extend(pending)
    else:
        raise AssertionError((pending, body))
    pending = []
assert not pending
mapping = json.loads((s / 'operation-map.json').read_text())
known = {key[3:] for key in mapping if key.startswith('OP_')}
assert set(single) | set(parallel) | special == known

leaves = {}
dispatch_cases = list(re.finditer(r'case GGML_OP_([A-Z0-9_]+):', original_dispatch))
for i, match in enumerate(dispatch_cases):
    body = original_dispatch[match.end():dispatch_cases[i+1].start() if i+1 < len(dispatch_cases) else len(original_dispatch)]
    call = re.search(r'\b(ggml_compute_forward_[a-z0-9_]+)\(', body)
    if call:
        leaves[match[1]] = call[1]
noops = ['NONE', 'RESHAPE', 'VIEW', 'TRANSPOSE', 'PERMUTE']
assert set(leaves) | set(noops) | {'COUNT'} == known
text = Path('/tmp/fg143_control.goose').read_text()
text = text.replace('SINGLE_THREAD_OPS', '||'.join('op==OP_'+op for op in single))
text = text.replace('PARALLEL_OPS', '||'.join('op==OP_'+op for op in parallel))
declarations = []
branches = []
primitive_c = []
primitive_headers = []
for op, target in leaves.items():
    symbol = 'fg143_ctl_leaf_' + op.lower()
    masked = op == 'FLASH_ATTN_BACK'
    extra = ',masked:i64' if masked else ''
    declarations.append(f'extern "{symbol}" fn leaf_{op.lower()}(context:i64{extra});')
    call = f'leaf_{op.lower()}(c'+(',mask' if masked else '')+');'
    code = ('let mask=meta(c,0,8);assert(mask==0||mask==1);' if masked else '')+call
    branches.append(('if' if not branches else 'else if') + f' op==OP_{op} '+'{'+code+'}')
    signature = f'void {symbol}(int64_t context'+(',int64_t masked' if masked else '')+')'
    primitive_headers.append(signature+';')
    args = 'x->params,' + ('masked!=0,' if masked else '') + 'x->node'
    primitive_c.append(signature+'{struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;'+target+'('+args+');}')
branches += ['else if op==OP_RESHAPE||op==OP_VIEW||op==OP_TRANSPOSE||op==OP_PERMUTE {}', 'else {assert(false);}']
text = text.replace('DISPATCH_DECLARATIONS', '\n'.join(declarations)).replace('DISPATCH_BODY', '\n    '.join(branches))
(c / 'kernel-template.goose').write_text(text)
text = re.sub(r'\b(?:OP_|UNARY_OP_)[A-Z0-9_]+\b', lambda m: str(mapping[m[0]]), text)
(c / 'kernel.goose').write_text(text)
(c / 'scope.json').write_text(json.dumps({'batch': 'B1', 'status': 'IMPLEMENTATION_GATE_PENDING',
    'previous_turn': 'PROGRESS:278 differential/sanitizer cases plus one diagnostic and66 Apex contract cases preserved',
    'scope': 'CPU graph planning,task-count policy,scratch arithmetic,operation selection; native backend-extra/tiled scratch providers and unfinished compute/OS leaves remain explicit',
    'native_sha256': hashlib.sha256(native.encode()).hexdigest(), 'producer': producer,
    'single_thread_ops': single, 'parallel_ops': parallel, 'special_ops': sorted(special),
    'dispatch_leaves': leaves, 'model_equivalence': 'OPEN:existing Apex response drift,not cleared by control transfer',
    'no_model_run': True, 'no_microbenchmarks': True, 'production_changed': False}, indent=2)+'\n')

services = r'''
struct fg143_ctl_context {const struct ggml_cgraph*graph;struct ggml_tensor*node;struct ggml_threadpool*pool;struct ggml_compute_params*params;};
static bool fg143_control_enabled=true;
__attribute__((constructor))static void fg143_control_config(void){const char*v=getenv("GGML_GOOSE_CPU_CONTROL");fg143_control_enabled=!v||strcmp(v,"0");}
bool fg143_control_is_enabled(void){return fg143_control_enabled;}
extern void fg143_cpu_control(int64_t,int64_t,int64_t,int64_t,int64_t*);
static struct ggml_tensor*fg143_control_node(struct fg143_ctl_context*x,int64_t index){if(x->graph){GGML_ASSERT(index>=0&&index<x->graph->n_nodes);return x->graph->nodes[index];}GGML_ASSERT(index==0&&x->node);return x->node;}
int64_t fg143_ctl_meta(int64_t context,int64_t index,int64_t field){
 struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;
 if(field>=50){switch(field){case 50:return x->pool?x->pool->n_threads:0;case 51:return GGML_DEFAULT_N_THREADS;case 52:
#if (defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_PTHREADS__)) || defined(__wasi__)
 return 1;
#else
 return 0;
#endif
 case 53:return CACHE_LINE_SIZE;case 54:return sizeof(struct mmid_row_mapping);case 55:return GGML_FA_TILE_Q;case 56:return GGML_FA_TILE_KV;case 57:return GGML_SOFT_MAX_UNROLL;case 58:return GGML_IM2COL_WORK_SIZE;case 59:return GGML_GLU_OP_COUNT;case 60:return GGML_UNARY_OP_COUNT;default:GGML_ABORT("invalid Goose CPU constant");}}
 const struct ggml_tensor*t=fg143_control_node(x,index);const struct ggml_tensor*a=t->src[0];
 if(field>=10&&field<14)return t->ne[field-10];
 if(field>=20&&field<32){int source=(field-20)/4,axis=(field-20)%4;return t->src[source]?t->src[source]->ne[axis]:0;}
 if(field>=2&&field<=4)return t->src[field-2]?t->src[field-2]->type:-1;
 switch(field){case 0:return t->op;case 1:return t->type;case 5:return ggml_is_quantized(t->type);case 6:return a?ggml_is_quantized(a->type):0;case 8:return ggml_get_op_params_i32(t,0);case 40:return type_traits_cpu[a->type].vec_dot_type;case 41:return ggml_type_size(type_traits_cpu[a->type].vec_dot_type);case 42:return ggml_blck_size(type_traits_cpu[a->type].vec_dot_type);case 43:return ggml_type_size(t->type);case 44:return a?ggml_type_size(a->type):0;
 case 7:switch(t->op){case GGML_OP_MAP_CUSTOM1:{struct ggml_map_custom1_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}case GGML_OP_MAP_CUSTOM2:{struct ggml_map_custom2_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}case GGML_OP_MAP_CUSTOM3:{struct ggml_map_custom3_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}case GGML_OP_CUSTOM:{struct ggml_custom_op_params p;memcpy(&p,t->op_params,sizeof(p));return p.n_tasks;}default:GGML_ABORT("invalid custom task metadata");}
 default:GGML_ABORT("invalid Goose CPU metadata");}
}
int64_t fg143_ctl_extra_work(int64_t context,int64_t index,int64_t threads){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;size_t size=0;if(!ggml_cpu_extra_work_size((int)threads,fg143_control_node(x,index),&size))return -1;GGML_ASSERT(size<=INT64_MAX);return(int64_t)size;}
int64_t fg143_ctl_tiled_work(int64_t context,int64_t index,int64_t tasks){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;size_t size=ggml_tiled_wdata_size((int)tasks,fg143_control_node(x,index));GGML_ASSERT(size<=INT64_MAX);return(int64_t)size;}
int64_t fg143_ctl_goose_work(int64_t context,int64_t index,int64_t tasks){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;size_t size=fg143_extra_workspace(fg143_control_node(x,index),(int)tasks);GGML_ASSERT(size<=INT64_MAX);return(int64_t)size;}
uint8_t fg143_ctl_extra_compute(int64_t context){struct fg143_ctl_context*x=(struct fg143_ctl_context*)(uintptr_t)context;return ggml_cpu_extra_compute_forward(x->params,x->node);}
void fg143_control_dispatch(struct ggml_compute_params*p,struct ggml_tensor*t){struct fg143_ctl_context context={0};context.node=t;context.params=p;fg143_cpu_control(2,(int64_t)(uintptr_t)&context,0,0,NULL);}
int fg143_control_tasks(struct ggml_tensor*t,int threads){struct fg143_ctl_context context={0};context.node=t;int64_t out[2]={0};fg143_cpu_control(1,(int64_t)(uintptr_t)&context,0,threads,out);GGML_ASSERT(out[0]>0&&out[0]<=INT_MAX);return(int)out[0];}
struct ggml_cplan fg143_control_plan(const struct ggml_cgraph*g,int threads,struct ggml_threadpool*pool){struct fg143_ctl_context context={0};context.graph=g;context.pool=pool;int64_t out[2]={0};fg143_cpu_control(0,(int64_t)(uintptr_t)&context,g->n_nodes,threads,out);GGML_ASSERT(out[0]>=0&&out[1]>0&&out[1]<=INT_MAX);struct ggml_cplan plan={0};plan.threadpool=pool;plan.n_threads=(int)out[1];plan.work_size=(size_t)out[0];return plan;}
'''
(c / 'services.c').write_text(services+'\n'+'\n'.join(primitive_c)+'\n')
header = '''extern bool fg143_control_is_enabled(void);
extern void fg143_control_dispatch(struct ggml_compute_params*,struct ggml_tensor*);
extern int fg143_control_tasks(struct ggml_tensor*,int);
extern struct ggml_cplan fg143_control_plan(const struct ggml_cgraph*,int,struct ggml_threadpool*);
'''
(c / 'declarations.h').write_text(header)
cmd = [producer['frozen'], '-o', str(r/'generated/cpu-control.c'), str(c/'kernel.goose')]
p = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
(c/'generation.log').write_text(p.stdout+p.stderr)
(c/'generation-command.json').write_text(json.dumps(cmd, indent=2)+'\n')
print('CPU control generation', p.returncode, (p.stdout+p.stderr)[-1500:], flush=True)
if p.returncode:
    raise SystemExit(p.returncode)

unit = '''#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
int64_t fg143_ctl_meta(int64_t,int64_t,int64_t);
int64_t fg143_ctl_extra_work(int64_t,int64_t,int64_t);
int64_t fg143_ctl_tiled_work(int64_t,int64_t,int64_t);
int64_t fg143_ctl_goose_work(int64_t,int64_t,int64_t);
uint8_t fg143_ctl_extra_compute(int64_t);
int64_t fg143_job_get(int64_t,int64_t);
void fg143_job_set(int64_t,int64_t,int64_t);
void fg143_job_create(int64_t,int64_t);
void fg143_job_free(int64_t);
void fg143_job_team(int64_t,int64_t);
void fg143_job_barrier(void);
void fg143_job_place(int64_t,int64_t);
void fg143_job_compute(int64_t,int64_t);
void fg143_job_clear_affinity(void);
static atomic_uint_fast64_t control_calls[5];
static void fg143_ctl_witness(int64_t kind){if(kind<0||kind>4)__builtin_trap();atomic_fetch_add_explicit(control_calls+kind,1,memory_order_relaxed);}
'''+ '\n'.join(primitive_headers)+'''
#define main fg143_control_unused_main
#include "../../../generated/cpu-control.c"
#undef main
void fg143_cpu_control(int64_t mode,int64_t context,int64_t count,int64_t threads,int64_t*out){gg_function_1(mode,context,count,threads,gg_view(out,out?2:0),0);}
__attribute__((destructor))static void fg143_control_report(void){fprintf(stderr,"FG143_CONTROL plans=%llu task_queries=%llu dispatches=%llu jobs=%llu worker_jobs=%llu\\n",(unsigned long long)atomic_load(control_calls),(unsigned long long)atomic_load(control_calls+1),(unsigned long long)atomic_load(control_calls+2),(unsigned long long)atomic_load(control_calls+3),(unsigned long long)atomic_load(control_calls+4));}
'''
(c/'unit.c').write_text(unit)
cmd = ['/usr/bin/gcc-13', '-O3', '-march=native', '-fPIC', '-fno-strict-aliasing',
       '-ffp-contract=off', '-c', str(c/'unit.c'), '-o', str(b/'control.o')]
p = subprocess.run(cmd, capture_output=True, text=True)
(b/'control-build.log').write_text(p.stdout+p.stderr)
(b/'control-command.json').write_text(json.dumps(cmd, indent=2)+'\n')
print('CPU control build', p.returncode, (p.stdout+p.stderr)[-1500:], flush=True)
if p.returncode:
    raise SystemExit(p.returncode)
print('Task cases', len(single), len(parallel), 'special', len(special), 'dispatch leaves', len(leaves), flush=True)
