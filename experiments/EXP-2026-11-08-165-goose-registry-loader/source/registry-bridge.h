#ifndef FG165_REGISTRY_BRIDGE_H
#define FG165_REGISTRY_BRIDGE_H
#include "registry-ffi.h"
#include <atomic>
#include <cinttypes>
#include <mutex>
#include <new>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>
struct fg165_storage{uintptr_t entries=0;size_t count=0,capacity=0;uintptr_t devices=0;size_t device_count=0,device_capacity=0;uint64_t initialized=0;};
static_assert(sizeof(uintptr_t)==8&&sizeof(size_t)==8,"Registry field/array ABI");
static std::atomic<int64_t>fg165_heaps{0},fg165_handles{0};static std::atomic<uint64_t>fg165_seen{0};static thread_local uint64_t fg165_thread_seen;
static bool fg165_enabled(){static const bool enabled=[](){const char*v=getenv("GGML_GOOSE_REGISTRY");return!(v&&v[0]=='0'&&v[1]==0);}();return enabled;}
static std::recursive_mutex&fg165_lock(){static std::recursive_mutex lock;return lock;}
static uint64_t fg165_call(int64_t mode,uint64_t a=0,uint64_t b=0,uint64_t c=0){std::lock_guard<std::recursive_mutex>guard(fg165_lock());uint64_t args[]={static_cast<uint64_t>(mode),a,b,c};fg165_mark(mode);return fg165_goose_entry((int64_t)(uintptr_t)args);}
struct fg165_owner{fg165_storage value{};~fg165_owner(){fg165_call(30,(uintptr_t)&value);fprintf(stderr,"FG165_REGISTRY modes=0x%" PRIx64 ",live_registry_heaps=%" PRId64 ",retained_plugin_handles=%" PRId64 "\n",fg165_seen.load(),fg165_heaps.load(),fg165_handles.load());}};
static fg165_owner&fg165_owner_get(){static fg165_owner owner;return owner;}
#ifdef FG165_TESTING
static std::atomic<int>fg165_fail_at{0},fg165_attempt{0};
extern "C" __attribute__((visibility("default"))) void fg165_fail_allocation_at(int n){fg165_attempt.store(0);fg165_fail_at.store(n);}
extern "C" __attribute__((visibility("default"))) int fg165_allocation_attempts(){return fg165_attempt.load();}
#endif
uint64_t fg165_arg(int64_t p,int64_t k){return((const uint64_t*)(uintptr_t)p)[k];}
int64_t fg165_state(){return(int64_t)(uintptr_t)&fg165_owner_get().value;}
uint64_t fg165_get(int64_t p,int64_t k){auto*s=(fg165_storage*)(uintptr_t)p;switch(k){case 0:return s->entries;case 1:return s->count;case 2:return s->capacity;case 3:return s->devices;case 4:return s->device_count;case 5:return s->device_capacity;case 6:return s->initialized;default:GGML_ABORT("Invalid registry field");}}
void fg165_put(int64_t p,int64_t k,uint64_t v){auto*s=(fg165_storage*)(uintptr_t)p;switch(k){case 0:s->entries=v;return;case 1:s->count=v;return;case 2:s->capacity=v;return;case 3:s->devices=v;return;case 4:s->device_count=v;return;case 5:s->device_capacity=v;return;case 6:s->initialized=v;return;default:GGML_ABORT("Invalid registry field write");}}
uint64_t fg165_ptr(int64_t p,uint64_t i){uintptr_t v;memcpy(&v,(const unsigned char*)(uintptr_t)p+i*sizeof(v),sizeof(v));return v;}
void fg165_ptr_put(int64_t p,uint64_t i,uint64_t v){uintptr_t q=v;memcpy((unsigned char*)(uintptr_t)p+i*sizeof(q),&q,sizeof(q));}
int64_t fg165_alloc(uint64_t n){
#ifdef FG165_TESTING
 int k=fg165_fail_at.load();if(k>0&&fg165_attempt.fetch_add(1)+1==k)return 0;
#endif
 void*p=malloc((size_t)n);if(p)fg165_heaps.fetch_add(1);return(int64_t)(uintptr_t)p;}
void fg165_free(int64_t p){if(p){fg165_heaps.fetch_sub(1);free((void*)(uintptr_t)p);}}
void fg165_copy(int64_t a,int64_t b,uint64_t n){if(n)memcpy((void*)(uintptr_t)a,(const void*)(uintptr_t)b,(size_t)n);}
uint64_t fg165_byte(int64_t p,uint64_t i){return((const unsigned char*)(uintptr_t)p)[i];}
void fg165_byte_put(int64_t p,uint64_t i,uint64_t v){((unsigned char*)(uintptr_t)p)[i]=(unsigned char)v;}
uint64_t fg165_lower(uint64_t c){return(uint64_t)std::tolower((unsigned char)c);}
int64_t fg165_builtin(int64_t i){switch(i){
#ifdef GGML_USE_CUDA
case 0:return(int64_t)(uintptr_t)ggml_backend_cuda_reg();
#endif
#ifdef GGML_USE_METAL
case 1:return(int64_t)(uintptr_t)ggml_backend_metal_reg();
#endif
#ifdef GGML_USE_SYCL
case 2:return(int64_t)(uintptr_t)ggml_backend_sycl_reg();
#endif
#ifdef GGML_USE_VULKAN
case 3:return(int64_t)(uintptr_t)ggml_backend_vk_reg();
#endif
#ifdef GGML_USE_WEBGPU
case 4:return(int64_t)(uintptr_t)ggml_backend_webgpu_reg();
#endif
#ifdef GGML_USE_ZDNN
case 5:return(int64_t)(uintptr_t)ggml_backend_zdnn_reg();
#endif
#ifdef GGML_USE_VIRTGPU_FRONTEND
case 6:return(int64_t)(uintptr_t)ggml_backend_virtgpu_reg();
#endif
#ifdef GGML_USE_OPENCL
case 7:return(int64_t)(uintptr_t)ggml_backend_opencl_reg();
#endif
#ifdef GGML_USE_ZENDNN
case 8:return(int64_t)(uintptr_t)ggml_backend_zendnn_reg();
#endif
#ifdef GGML_USE_HEXAGON
case 9:return(int64_t)(uintptr_t)ggml_backend_hexagon_reg();
#endif
#ifdef GGML_USE_CANN
case 10:return(int64_t)(uintptr_t)ggml_backend_cann_reg();
#endif
#ifdef GGML_USE_BLAS
case 11:return(int64_t)(uintptr_t)ggml_backend_blas_reg();
#endif
#ifdef GGML_USE_RPC
case 12:return(int64_t)(uintptr_t)ggml_backend_rpc_reg();
#endif
#ifdef GGML_USE_OPENVINO
case 13:return(int64_t)(uintptr_t)ggml_backend_openvino_reg();
#endif
#ifdef GGML_USE_ET
case 14:return(int64_t)(uintptr_t)ggml_backend_et_reg();
#endif
#ifdef GGML_USE_CPU
case 15:return(int64_t)(uintptr_t)ggml_backend_cpu_reg();
#endif
default:return 0;}}
int64_t fg165_literal(int64_t i){switch(i){case 0:return(int64_t)(uintptr_t)"ggml_backend_score";case 1:return(int64_t)(uintptr_t)"ggml_backend_init";case 2:return(int64_t)(uintptr_t)"./";case 3:
#ifdef GGML_BACKEND_DIR
 return(int64_t)(uintptr_t)GGML_BACKEND_DIR;
#else
 return 0;
#endif
case 4:return(int64_t)(uintptr_t)"libggml-";case 5:return(int64_t)(uintptr_t)"-";case 6:return(int64_t)(uintptr_t)".so";
case 10:return(int64_t)(uintptr_t)"blas";
case 11:return(int64_t)(uintptr_t)"zendnn";
case 12:return(int64_t)(uintptr_t)"cann";
case 13:return(int64_t)(uintptr_t)"cuda";
case 14:return(int64_t)(uintptr_t)"hip";
case 15:return(int64_t)(uintptr_t)"metal";
case 16:return(int64_t)(uintptr_t)"rpc";
case 17:return(int64_t)(uintptr_t)"sycl";
case 18:return(int64_t)(uintptr_t)"vulkan";
case 19:return(int64_t)(uintptr_t)"virtgpu";
case 20:return(int64_t)(uintptr_t)"opencl";
case 21:return(int64_t)(uintptr_t)"hexagon";
case 22:return(int64_t)(uintptr_t)"musa";
case 23:return(int64_t)(uintptr_t)"openvino";
case 24:return(int64_t)(uintptr_t)"cpu";
default:GGML_ABORT("Invalid registry literal");}}
uint64_t fg165_effect(int64_t op,int64_t a,int64_t b,int64_t){switch(op){
case 0:return(uintptr_t)ggml_backend_reg_name((ggml_backend_reg_t)(uintptr_t)a);
case 1:return ggml_backend_reg_dev_count((ggml_backend_reg_t)(uintptr_t)a);
case 2:return(uintptr_t)ggml_backend_reg_dev_get((ggml_backend_reg_t)(uintptr_t)a,(size_t)b);
case 3:return(uintptr_t)ggml_backend_dev_name((ggml_backend_dev_t)(uintptr_t)a);
case 4:return ggml_backend_dev_type((ggml_backend_dev_t)(uintptr_t)a);
case 5:return(uintptr_t)ggml_backend_dev_backend_reg((ggml_backend_dev_t)(uintptr_t)a);
case 6:return((ggml_backend_reg_t)(uintptr_t)a)->api_version;
case 7:return GGML_BACKEND_API_VERSION;
case 8:return(uintptr_t)ggml_backend_dev_init((ggml_backend_dev_t)(uintptr_t)a,(const char*)(uintptr_t)b);
case 10:{void*h=dlopen((const char*)(uintptr_t)a,RTLD_NOW|RTLD_LOCAL);if(h)fg165_handles.fetch_add(1);return(uintptr_t)h;}
case 11:return(uintptr_t)dlsym((void*)(uintptr_t)a,(const char*)(uintptr_t)b);
case 12:if(a){fg165_handles.fetch_sub(1);dlclose((void*)(uintptr_t)a);}return 0;
case 13:try{return(uint64_t)(int64_t)((ggml_backend_score_t)(uintptr_t)a)();}catch(...){return 0;}
case 14:try{return(uintptr_t)((ggml_backend_init_t)(uintptr_t)a)();}catch(...){return 0;}
case 20:return(uint64_t)(int64_t)readlink("/proc/self/exe",(char*)(uintptr_t)a,(size_t)b);
case 21:if(getcwd((char*)(uintptr_t)a,(size_t)b))return 1;return errno==ERANGE?UINT64_MAX:0;
case 22:return(uintptr_t)opendir((const char*)(uintptr_t)a);
case 23:{errno=0;auto*d=readdir((DIR*)(uintptr_t)a);return d?(uintptr_t)d->d_name:0;}
case 24:closedir((DIR*)(uintptr_t)a);return 0;
case 25:{struct stat st;return stat((const char*)(uintptr_t)a,&st)==0&&S_ISREG(st.st_mode);}
case 26:{struct stat st;return stat((const char*)(uintptr_t)a,&st)==0;}
case 30:return GGML_BACKEND_DEVICE_TYPE_GPU;case 31:return GGML_BACKEND_DEVICE_TYPE_IGPU;case 32:return GGML_BACKEND_DEVICE_TYPE_CPU;
default:GGML_ABORT("Invalid registry effect");}}
int64_t fg165_env(int64_t k){return(int64_t)(uintptr_t)getenv(k==0?"GGML_BACKEND_PATH":"GGML_DISABLE_VULKAN");}
void fg165_mark(int64_t k){uint64_t bit=UINT64_C(1)<<k;if(!(fg165_thread_seen&bit)){fg165_thread_seen|=bit;fg165_seen.fetch_or(bit,std::memory_order_relaxed);}}
void fg165_log(int64_t k,int64_t a,int64_t b){const char*p=(const char*)(uintptr_t)a;auto*r=(ggml_backend_reg_t)(uintptr_t)b;switch(k){case 0:{const char*e=dlerror();GGML_LOG_ERROR("Goose registry:failed to load %s:%s\n",p,e?e:"");break;}case 1:GGML_LOG_INFO("Goose registry:backend %s is not supported\n",p);break;case 2:GGML_LOG_ERROR("Goose registry:missing ggml_backend_init in %s\n",p);break;case 3:GGML_LOG_ERROR("Goose registry:invalid backend/API from %s:%d\n",p,r?r->api_version:-1);break;case 4:GGML_LOG_INFO("Goose registry:loaded %s backend from %s\n",ggml_backend_reg_name(r),p);break;case 5:GGML_LOG_INFO("Goose registry:missing ggml_backend_score in %s\n",p);break;default:GGML_ABORT("Invalid registry log");}}
void fg165_fail(int64_t k){GGML_ABORT("Goose registry validation failed:code=%" PRId64,k);}
extern "C" __attribute__((visibility("default"))) uint64_t fg165_registry_seen(){return fg165_seen.load();}
extern "C" __attribute__((visibility("default"))) int64_t fg165_registry_heap_live(){return fg165_heaps.load();}
extern "C" __attribute__((visibility("default"))) int64_t fg165_registry_handles_live(){return fg165_handles.load();}
#define FG165_A(x) ((uint64_t)(uintptr_t)(x))
#endif
