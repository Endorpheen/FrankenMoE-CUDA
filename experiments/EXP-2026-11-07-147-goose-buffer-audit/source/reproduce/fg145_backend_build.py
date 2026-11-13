from pathlib import Path
import hashlib,json,re,shlex,shutil,subprocess

r=Path('/home/end0/AI/LlamaServer/franken-goose-live-20261107');parent=r/'source/cpu-backend';s=parent/'backend';s.mkdir(exist_ok=True);build=r/'variants/cpu-backend/build';producer=json.loads((parent/'producer.json').read_text())
native=(r/'source/ggml/src/ggml-cpu/ggml-cpu.cpp').read_text()
scope={'status':'IMPLEMENTATION_GATE_PENDING','batch':'B1','requirement':'P2-14 and backend graph/workspace ownership','producer':producer,
       'native_source_sha256':hashlib.sha256(native.encode()).hexdigest(),'scope':'CPU backend creation/release,plan and reusable workspace lifecycle,capability decisions,extra-buffer registration order,feature/procedure lookup and memory-report policy in Goose.',
       'remaining':'Native ABI records,physical allocation/delete primitives,extra-buffer implementations,OS CPU-description construction,CPU hardware queries,HBM/copy/alignment and other backend source remain explicit residuals.',
       'production_changed':False,'compiler_source_changed':False,'no_model_run_yet':True}
(s/'scope.json').write_text(json.dumps(scope,indent=2)+'\n')
reference=r/'variants/cpu-backend-api-reference/run'
if not reference.exists():
 reference.mkdir(parents=True)
 for path in (r/'variants/cpu-backend/run').iterdir():
  target=reference/path.name
  if path.name=='libggml-cpu.so.0.27.0':shutil.copy2(path,target)
  elif path.name.startswith('libggml-cpu.so'):target.symlink_to('libggml-cpu.so.0.27.0')
  else:target.symlink_to(path.resolve())

def body_bounds(text,name):
 pattern=r'(?m)^(?:static[ \t]+)?[A-Za-z_][^\n;{}(=]*?\b'+re.escape(name)+r'\s*\([^;{}]*?\)\s*\{'
 matches=list(re.finditer(pattern,text));assert len(matches)==1,(name,len(matches));start=matches[0].end();depth=1;end=start
 while depth:
  if text[end]=='{':depth+=1
  elif text[end]=='}':depth-=1
  end+=1
 return start,end-1
def prepend(text,name,code):
 start,end=body_bounds(text,name);return text[:start]+'\n'+code+'\n'+text[start:]

mapping=json.loads((parent/'operation-map.json').read_text());header=(r/'source/ggml/include/ggml.h').read_text();header=re.sub(r'/\*.*?\*/|//[^\n]*','',header,flags=re.S);types={name:int(value) for name,value in re.findall(r'GGML_(TYPE_[A-Z0-9_]+)\s*=\s*(\d+)',header)}
start,end=body_bounds(native,'ggml_backend_cpu_get_proc_address');proc=native[start:end];names=re.findall(r'strcmp\(name, "([^"]+)"\)',proc);symbols=[]
for name in names:
 segment=proc[proc.index('"'+name+'"'):];segment=segment[:segment.index('}')]
 match=re.search(r'fct\s*=\s*(\w+)|return \(void \*\)(\w+)',segment);assert match;symbols.append(match[1] or match[2])
lookup=[]
for index,name in enumerate(names):
 checks=[f'meta(c,43)=={len(name)}']+[f'meta(c,{300+i})=={b}' for i,b in enumerate(name.encode())]
 lookup.append('if '+'&&'.join(checks)+' {return action(c,17,'+str(index)+',0);}')
text=Path('/tmp/fg145_backend.goose').read_text().replace('PROC_LOOKUP','\n    '.join(lookup))
(s/'kernel-template.goose').write_text(text);allmap={**mapping,**types};text=re.sub(r'\b(?:OP_|TYPE_)[A-Z0-9_]+\b',lambda m:str(allmap[m[0]]),text);(s/'kernel.goose').write_text(text)

prefix=r'''
#include <new>
#include <cstdlib>
#include <cstring>
#include <cstdint>
struct ggml_backend_cpu_context;
struct ggml_backend_plan_cpu;
struct fg145_request {ggml_backend_t backend;ggml_backend_cpu_context*cpu;ggml_backend_plan_cpu*plan;ggml_cgraph*graph;ggml_backend_dev_t dev;const ggml_tensor*op;ggml_backend_buffer_type_t buft;const char*name;int64_t name_len;int64_t value;void*pointer;void*data;std::vector<ggml_backend_buffer_type_t>*extras;std::vector<ggml_backend_feature>*features;ggml_cplan temporary;size_t*free_mem;size_t*total_mem;};
extern "C" int64_t fg145_backend_api(int64_t,int64_t);
static bool fg145_enabled=true;
__attribute__((constructor))static void fg145_config(){const char*v=std::getenv("GGML_GOOSE_CPU_BACKEND_API");fg145_enabled=!v||std::strcmp(v,"0");}
static int64_t fg145_call(int64_t mode,fg145_request&q){return fg145_backend_api(mode,(int64_t)(uintptr_t)&q);}
static fg145_request fg145_request_backend(ggml_backend_t);
static std::vector<ggml_backend_buffer_type_t>&fg145_extras();
static ggml_backend_buffer_type_t*fg145_extra_copy();
static ggml_backend_feature*fg145_features();
'''
modified=native
hooks={
'ggml_backend_cpu_get_extra_buffer_types':'if(fg145_enabled)return fg145_extras();',
'ggml_backend_cpu_device_get_extra_buffers_type':'if(fg145_enabled)return fg145_extra_copy();',
'ggml_backend_cpu_is_extra_buffer_type':'if(fg145_enabled){fg145_request q={};q.buft=buft;return fg145_call(12,q)!=0;}',
'ggml_backend_cpu_init':'if(fg145_enabled){ggml_cpu_init();fg145_request q={};return(ggml_backend_t)(uintptr_t)fg145_call(0,q);}',
'ggml_backend_cpu_free':'if(fg145_enabled){auto q=fg145_request_backend(backend);fg145_call(1,q);return;}',
'ggml_backend_cpu_graph_plan_create':'if(fg145_enabled){auto q=fg145_request_backend(backend);q.graph=const_cast<ggml_cgraph*>(cgraph);return(ggml_backend_graph_plan_t)(uintptr_t)fg145_call(2,q);}',
'ggml_backend_cpu_graph_plan_free':'if(fg145_enabled){auto q=fg145_request_backend(backend);q.plan=(ggml_backend_plan_cpu*)plan;fg145_call(3,q);return;}',
'ggml_backend_cpu_graph_plan_compute':'if(fg145_enabled){auto q=fg145_request_backend(backend);q.plan=(ggml_backend_plan_cpu*)plan;return(enum ggml_status)fg145_call(4,q);}',
'ggml_backend_cpu_graph_compute':'if(fg145_enabled){auto q=fg145_request_backend(backend);q.graph=cgraph;return(enum ggml_status)fg145_call(5,q);}',
'ggml_backend_cpu_set_n_threads':'if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.value=n_threads;fg145_call(6,q);return;}',
'ggml_backend_cpu_set_threadpool':'if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.pointer=threadpool;fg145_call(7,q);return;}',
'ggml_backend_cpu_set_abort_callback':'if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.pointer=(void*)abort_callback;q.data=abort_callback_data;fg145_call(8,q);return;}',
'ggml_backend_cpu_set_use_ref':'if(fg145_enabled){auto q=fg145_request_backend(backend_cpu);q.value=use_ref;fg145_call(9,q);return;}',
'ggml_backend_cpu_device_supports_op':'if(fg145_enabled){fg145_request q={};q.dev=dev;q.op=op;return fg145_call(10,q)!=0;}',
'ggml_backend_cpu_device_supports_buft':'if(fg145_enabled){fg145_request q={};q.dev=dev;q.buft=buft;return fg145_call(11,q)!=0;}',
'ggml_backend_cpu_get_proc_address':'if(fg145_enabled){fg145_request q={};q.name=name;q.name_len=name?(int64_t)std::strlen(name):0;return(void*)(uintptr_t)fg145_call(15,q);}',
'ggml_backend_cpu_device_get_memory':'if(fg145_enabled){fg145_request q={};q.dev=dev;q.free_mem=free;q.total_mem=total;fg145_call(16,q);return;}',
'ggml_backend_cpu_reg_get_device_count':'if(fg145_enabled){fg145_request q={};return(size_t)fg145_call(17,q);}',
'ggml_backend_cpu_get_features':'if(fg145_enabled)return fg145_features();'}
for name,code in hooks.items():modified=prepend(modified,name,code)
marker='// ggml-backend interface';assert modified.count(marker)==1;modified=modified.replace(marker,prefix+'\n'+marker,1)

feature_start,feature_end=body_bounds(native,'ggml_backend_cpu_get_features');feature_body=native[feature_start:feature_end];feature_names=re.findall(r'features.push_back\(\{ "([^"]+)"',feature_body)
runtime_features={name:fn for fn,name in re.findall(r'if \((ggml_cpu_\w+)\(\)(?: > 0)?\)\s*\{.*?features.push_back\(\{ "([^"]+)"',feature_body,re.S)}
compile_features={'ACCELERATE':'GGML_USE_ACCELERATE','CPU_HBM':'GGML_USE_CPU_HBM','OPENMP':'GGML_USE_OPENMP','KLEIDIAI':'GGML_USE_CPU_KLEIDIAI','REPACK':'GGML_USE_CPU_REPACK'}
assert set(feature_names)==set(runtime_features)|set(compile_features)
feature_cases=[]
for index,name in enumerate(feature_names):
 if name in runtime_features:feature_cases.append(f'case {index}:return {runtime_features[name]}();')
 else:feature_cases.append(f'case {index}:\n#ifdef {compile_features[name]}\nreturn 1;\n#else\nreturn 0;\n#endif\n')

services=r'''
static fg145_request fg145_request_backend(ggml_backend_t backend){fg145_request q={};q.backend=backend;q.cpu=(ggml_backend_cpu_context*)backend->context;return q;}
static ggml_cplan&fg145_plan(fg145_request&r){return r.plan?r.plan->cplan:r.temporary;}
static std::vector<ggml_backend_buffer_type_t>&fg145_extras(){static std::vector<ggml_backend_buffer_type_t> value=[](){std::vector<ggml_backend_buffer_type_t> out;fg145_request q={};q.extras=&out;fg145_call(13,q);return out;}();return value;}
static ggml_backend_buffer_type_t*fg145_extra_copy(){static std::vector<ggml_backend_buffer_type_t> value=[](){std::vector<ggml_backend_buffer_type_t> out;fg145_request q={};q.extras=&out;fg145_call(14,q);return out;}();return value.data();}
static ggml_backend_feature*fg145_features(){static std::vector<ggml_backend_feature> value=[](){ggml_cpu_init();std::vector<ggml_backend_feature> out;fg145_request q={};q.features=&out;fg145_call(18,q);return out;}();return value.data();}
extern "C" int64_t fg145_meta(int64_t context,int64_t field){auto&r=*(fg145_request*)(uintptr_t)context;const ggml_tensor*t=r.op;const ggml_tensor*x=t?t->src[0]:nullptr;const ggml_tensor*y=t?t->src[1]:nullptr;
 if(field>=100&&field<104){auto*source=t->src[field-100];return source&&source->buffer?(int64_t)(uintptr_t)source->buffer->buft:0;}
 if(field>=200&&field<300){auto&v=fg145_extras();size_t i=(size_t)(field-200);GGML_ASSERT(i<v.size());return(int64_t)(uintptr_t)v[i];}
 if(field>=300&&field<400){GGML_ASSERT(field-300<r.name_len);return(uint8_t)r.name[field-300];}
 if(field>=400){switch(field-400){FEATURE_CASES default:GGML_ABORT("invalid CPU feature");}}
 switch(field){case 0:return ggml_backend_is_cpu(r.backend);case 1:return GGML_DEFAULT_N_THREADS;case 2:return r.cpu->n_threads;case 3:return(int64_t)(uintptr_t)r.cpu->threadpool;case 4:return(int64_t)(uintptr_t)r.cpu->work_data;case 5:GGML_ASSERT(r.cpu->work_size<=INT64_MAX);return(int64_t)r.cpu->work_size;case 6:return(int64_t)(uintptr_t)r.cpu->abort_callback;case 7:return(int64_t)(uintptr_t)r.cpu->abort_callback_data;case 8:return r.cpu->use_ref;case 9:return r.value;case 10:return(int64_t)(uintptr_t)r.pointer;case 11:return(int64_t)(uintptr_t)r.data;case 12:return(int64_t)(uintptr_t)r.plan;case 13:GGML_ASSERT(fg145_plan(r).work_size<=INT64_MAX);return(int64_t)fg145_plan(r).work_size;case 14:return(int64_t)(uintptr_t)fg145_plan(r).work_data;
 case 20:return t->op;case 21:return t->type;case 22:return x?x->type:-1;case 23:return y?y->type:-1;case 24:return ggml_get_type_traits_cpu(x->type)->vec_dot_type;case 25:return x?ggml_is_quantized(x->type):0;case 26:return x?ggml_is_contiguous(x):0;case 27:return ggml_get_op_params_i32(t,0);case 28:return ggml_get_op_params_i32(t,1);case 29:return GGML_HINT_SRC0_IS_HADAMARD;case 30:return t->src[3]?t->src[3]->ne[0]:0;case 31:return x?x->ne[2]:0;case 32:return y?y->ne[2]:0;case 33:return x?x->ne[3]:0;case 34:return y?y->ne[3]:0;case 40:return(int64_t)fg145_extras().size();case 41:return ggml_backend_buft_is_host(r.buft);case 42:return(int64_t)(uintptr_t)r.buft;case 43:return r.name_len;
 case 44:
#ifdef _WIN32
 return 1;
#else
 return 0;
#endif
 case 45:
#ifdef _WIN32
 {MEMORYSTATUSEX status;status.dwLength=sizeof(status);GlobalMemoryStatusEx(&status);return(int64_t)status.ullTotalPhys;}
#else
 return sysconf(_SC_PHYS_PAGES);
#endif
 case 46:
#ifdef _WIN32
 {MEMORYSTATUSEX status;status.dwLength=sizeof(status);GlobalMemoryStatusEx(&status);return(int64_t)status.ullAvailPhys;}
#else
 return sysconf(_SC_PAGE_SIZE);
#endif
 case 47:return GGML_STATUS_ALLOC_FAILED;case 51:return FEATURE_COUNT;default:GGML_ABORT("invalid CPU backend metadata");}
}
extern "C" float fg145_float(int64_t context,int64_t field){auto&r=*(fg145_request*)(uintptr_t)context;GGML_ASSERT(field==0);float result;std::memcpy(&result,(const char*)r.op->op_params+sizeof(float),sizeof(result));return result;}
extern "C" int64_t fg145_action(int64_t context,int64_t op,int64_t a,int64_t b){auto&r=*(fg145_request*)(uintptr_t)context;switch(op){
 case 0:r.cpu=new(std::nothrow)ggml_backend_cpu_context;return(int64_t)(uintptr_t)r.cpu;
 case 1:switch(a){case 0:GGML_ASSERT(b>=INT32_MIN&&b<=INT32_MAX);r.cpu->n_threads=(int)b;break;case 1:r.cpu->threadpool=(ggml_threadpool_t)(uintptr_t)b;break;case 2:r.cpu->work_data=(uint8_t*)(uintptr_t)b;break;case 3:GGML_ASSERT(b>=0);r.cpu->work_size=(size_t)b;break;case 4:r.cpu->abort_callback=(ggml_abort_callback)(uintptr_t)b;break;case 5:r.cpu->abort_callback_data=(void*)(uintptr_t)b;break;case 6:r.cpu->use_ref=b!=0;break;default:GGML_ABORT("invalid CPU context write");}return 0;
 case 2:{auto*backend=new(std::nothrow)ggml_backend{ggml_backend_cpu_guid(),ggml_backend_cpu_i,ggml_backend_reg_dev_get(ggml_backend_cpu_reg(),0),r.cpu};return(int64_t)(uintptr_t)backend;}
 case 3:delete r.cpu;r.cpu=nullptr;return 0;case 4:delete r.backend;r.backend=nullptr;return 0;
 case 5:GGML_ASSERT(a>=0);return(int64_t)(uintptr_t)new(std::nothrow)uint8_t[(size_t)a];
 case 6:delete[](uint8_t*)(uintptr_t)a;return 0;
 case 7:ggml_threadpool_pause((ggml_threadpool_t)(uintptr_t)a);return 0;
 case 8:r.plan=new(std::nothrow)ggml_backend_plan_cpu;return(int64_t)(uintptr_t)r.plan;
 case 9:delete r.plan;r.plan=nullptr;return 0;
 case 10:fg145_plan(r)=ggml_graph_plan(r.graph,r.cpu->n_threads,r.cpu->threadpool);return 0;
 case 11:r.plan->cgraph=*r.graph;return 0;
 case 12:switch(a){case 0:fg145_plan(r).work_data=(uint8_t*)(uintptr_t)b;break;case 1:fg145_plan(r).abort_callback=(ggml_abort_callback)(uintptr_t)b;break;case 2:fg145_plan(r).abort_callback_data=(void*)(uintptr_t)b;break;case 3:fg145_plan(r).use_ref=b!=0;break;default:GGML_ABORT("invalid CPU plan write");}return 0;
 case 13:GGML_ASSERT(a==0||a==1);return a?ggml_graph_compute(&r.plan->cgraph,&r.plan->cplan):ggml_graph_compute(r.graph,&r.temporary);
 case 14:{GGML_ASSERT(a>=0&&a<4);auto*extra=(ggml::cpu::extra_buffer_type*)r.op->src[a]->buffer->buft->context;return extra->supports_op(r.dev,r.op);}
 case 15:r.extras->push_back((ggml_backend_buffer_type_t)(uintptr_t)a);return 0;
 case 16:switch(a){case 0:
#if defined(__AMX_INT8__) && defined(__AVX512VNNI__)
 return(int64_t)(uintptr_t)ggml_backend_amx_buffer_type();
#else
 return 0;
#endif
 case 1:
#ifdef GGML_USE_CPU_RISCV64_SPACEMIT
 return(int64_t)(uintptr_t)ggml_backend_cpu_riscv64_spacemit_buffer_type();
#else
 return 0;
#endif
 case 2:
#ifdef GGML_USE_CPU_KLEIDIAI
 return(int64_t)(uintptr_t)ggml_backend_cpu_kleidiai_buffer_type();
#else
 return 0;
#endif
 case 3:
#ifdef GGML_USE_CPU_REPACK
 return(int64_t)(uintptr_t)ggml_backend_cpu_repack_buffer_type();
#else
 return 0;
#endif
 default:GGML_ABORT("invalid extra buffer provider");}
 case 17:{static void*const addresses[]={PROC_POINTERS};GGML_ASSERT(a>=0&&(size_t)a<sizeof(addresses)/sizeof(*addresses));return(int64_t)(uintptr_t)addresses[a];}
 case 18:GGML_ASSERT(b>=0);if(a==0)*r.free_mem=(size_t)b;else{GGML_ASSERT(a==1);*r.total_mem=(size_t)b;}return 0;
 case 19:{if(a==-1){r.features->push_back({nullptr,nullptr});return 0;}static const char*const names[]={FEATURE_NAMES};GGML_ASSERT(a>=0&&(size_t)a<sizeof(names)/sizeof(*names));const char*value="1";FEATURE_VALUES r.features->push_back({names[a],value});return 0;}
 default:GGML_ABORT("invalid CPU backend primitive");}}
'''
feature_values=[]
for name,variable,fn in [('SVE_CNT','sve_count','ggml_cpu_get_sve_cnt'),('RVV_VLEN','rvv_count','ggml_cpu_get_rvv_vlen')]:
 index=feature_names.index(name);feature_values.append(f'if(a=={index}){{static std::string {variable}=std::to_string({fn}());value={variable}.c_str();}}')
services=services.replace('FEATURE_CASES','\n'.join(feature_cases)).replace('FEATURE_COUNT',str(len(feature_names))).replace('PROC_POINTERS',','.join('(void*)'+name for name in symbols)).replace('FEATURE_NAMES',','.join(json.dumps(name) for name in feature_names)).replace('FEATURE_VALUES',''.join(feature_values))
(s/'services.cpp').write_text(services);(s/'ggml-cpu-backend.cpp').write_text(modified+'\n'+services)
(s/'interface-map.json').write_text(json.dumps({'hooked_methods':list(hooks),'proc_names':names,'proc_symbols':symbols,'features':feature_names},indent=2)+'\n')
cmd=[producer['frozen'],'-o',str(r/'generated/cpu-backend-api.c'),str(s/'kernel.goose')];p=subprocess.run(cmd,capture_output=True,text=True,timeout=120);(s/'generation.log').write_text(p.stdout+p.stderr);print('Backend Goose generation',p.returncode,(p.stdout+p.stderr)[-1400:],flush=True)
if p.returncode:raise SystemExit(p.returncode)
unit='''#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
int64_t fg145_meta(int64_t,int64_t);
float fg145_float(int64_t,int64_t);
int64_t fg145_action(int64_t,int64_t,int64_t,int64_t);
static atomic_uint_fast64_t api_calls[19];
static void fg145_witness(int64_t mode){if(mode<0||mode>=19)__builtin_trap();atomic_fetch_add_explicit(api_calls+mode,1,memory_order_relaxed);}
_Static_assert(sizeof(uintptr_t)==sizeof(int64_t),"opaque ABI needs64bits");
#define main fg145_unused_main
#include "../../../generated/cpu-backend-api.c"
#undef main
int64_t fg145_backend_api(int64_t mode,int64_t context){return gg_function_1(mode,context,0);}
__attribute__((destructor))static void fg145_report(void){for(int i=0;i<19;++i){uint64_t n=atomic_load(api_calls+i);if(n)fprintf(stderr,"FG145_BACKEND mode=%d calls=%llu\\n",i,(unsigned long long)n);}}
''';(s/'unit.c').write_text(unit)
unit_cmd=['/usr/bin/gcc-13','-O3','-march=native','-fPIC','-fno-strict-aliasing','-ffp-contract=off','-c',str(s/'unit.c'),'-o',str(build/'backend-goose.o')]
compile_db=json.loads(Path('/home/end0/AI/LlamaServer/llama.cpp-flint-cuda/build-cuda/compile_commands.json').read_text());row=next(x for x in compile_db if x['file'].endswith('/ggml-cpu/ggml-cpu.cpp'));cpp_cmd=shlex.split(row['command']);cpp_cmd[cpp_cmd.index('-c')+1]=str(s/'ggml-cpu-backend.cpp');cpp_cmd[cpp_cmd.index('-o')+1]=str(build/'backend-cpp.o');cpp_cmd += ['-I'+str(r/'source/ggml/src/ggml-cpu')]
for label,argv,cwd in [('backend-goose',unit_cmd,str(r)),('backend-cpp',cpp_cmd,row['directory'])]:
 p=subprocess.run(argv,cwd=cwd,capture_output=True,text=True);(build/(label+'.log')).write_text(p.stdout+p.stderr);(build/(label+'-command.json')).write_text(json.dumps({'argv':argv,'cwd':cwd},indent=2)+'\n');print(label,p.returncode,(p.stdout+p.stderr)[-1600:],flush=True)
 if p.returncode:raise SystemExit(p.returncode)
print('Backend API module built; integration and correctness pending',flush=True)
