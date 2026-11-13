#ifndef FG162_BUFFERS_BRIDGE_H
#define FG162_BUFFERS_BRIDGE_H
#include "buffers-ffi.h"
#include <atomic>
#include <cinttypes>
#include <new>
#include <type_traits>
static_assert(sizeof(uintptr_t)==8&&sizeof(size_t)==8,"64-bit buffer ABI");
static_assert(std::is_trivially_copyable<ggml_tensor>::value,"Tensor snapshot ABI");
static std::atomic<int64_t> fg162_heaps{0};
static std::atomic<uint64_t> fg162_seen{0};
static thread_local uint64_t fg162_thread_seen;
#ifdef FG162_TESTING
static std::atomic<int> fg162_fail_at{0},fg162_attempt{0};
static bool fg162_injected(){int n=fg162_fail_at.load();return n>0&&fg162_attempt.fetch_add(1)+1==n;}
#else
static bool fg162_injected(){return false;}
#endif
void fg162_mark(int64_t i){uint64_t bit=UINT64_C(1)<<i;if(!(fg162_thread_seen&bit)){fg162_thread_seen|=bit;fg162_seen.fetch_or(bit,std::memory_order_relaxed);}}
uint64_t fg162_arg(int64_t p,int64_t i){return ((const fg162_packet*)(uintptr_t)p)->v[i];}
void fg162_arg_put(int64_t p,int64_t i,uint64_t v){((fg162_packet*)(uintptr_t)p)->v[i]=v;}
uint64_t fg162_constant(int64_t k){switch(k){case 0:return GGML_BACKEND_BUFFER_USAGE_ANY;case 1:return (uint64_t)(int64_t)GGML_STATUS_SUCCESS;case 2:return sizeof(ggml_backend_buft_alloc_buffer_n_plan_item);case 3:return sizeof(ggml_tensor);default:GGML_ABORT("Invalid buffer constant");}}
int64_t fg162_heap(int64_t kind,uint64_t n){if(fg162_injected())return 0;void*p=nullptr;switch(kind){case 0:p=new(std::nothrow)ggml_backend_buffer;break;case 1:p=new(std::nothrow)ggml_backend_multi_buffer_context{};break;case 2:p=new(std::nothrow)ggml_backend_buffer_t[(size_t)n];break;case 3:p=new(std::nothrow)ggml_backend_buft_alloc_buffer_n_plan_item[(size_t)n];break;case 4:p=new(std::nothrow)ggml_tensor[(size_t)n];break;default:GGML_ABORT("Invalid buffer heap kind");}if(p)fg162_heaps.fetch_add(1);return(int64_t)(uintptr_t)p;}
void fg162_drop(int64_t kind,int64_t p){if(!p)return;fg162_heaps.fetch_sub(1);switch(kind){case 0:delete(ggml_backend_buffer*)(uintptr_t)p;return;case 1:delete(ggml_backend_multi_buffer_context*)(uintptr_t)p;return;case 2:delete[](ggml_backend_buffer_t*)(uintptr_t)p;return;case 3:delete[](ggml_backend_buft_alloc_buffer_n_plan_item*)(uintptr_t)p;return;case 4:delete[](ggml_tensor*)(uintptr_t)p;return;default:GGML_ABORT("Invalid buffer heap release");}}
uint64_t fg162_get(int64_t p,int64_t kind,int64_t k,int64_t i){if(kind==0){auto*b=(const ggml_backend_buffer*)(uintptr_t)p;switch(k){case 0:return(uintptr_t)b->buft;case 1:return(uintptr_t)b->context;case 2:return b->size;case 3:return b->usage;}}if(kind==1){auto*c=(const ggml_backend_multi_buffer_context*)(uintptr_t)p;return k==0?(uintptr_t)c->buffers:c->n_buffers;}if(kind==2){auto*t=(const ggml_tensor*)(uintptr_t)p;switch(k){case 0:return(uintptr_t)t->buffer;case 1:return(uintptr_t)t->data;case 2:return t->op;case 3:return(uintptr_t)t->view_src;case 4:return t->view_offs;}}if(kind==3){auto*v=(const ggml_backend_buft_alloc_buffer_n_plan_item*)(uintptr_t)p;switch(k){case 0:return v[i].size;case 1:return(uint64_t)(int64_t)v[i].first;case 2:return(uint64_t)(int64_t)v[i].last;}}if(kind==4)return(uintptr_t)((ggml_backend_buffer_type*)(uintptr_t)p)->device;GGML_ABORT("Invalid buffer projection");}
void fg162_put(int64_t p,int64_t kind,int64_t k,int64_t i,uint64_t v){if(kind==0){auto*b=(ggml_backend_buffer*)(uintptr_t)p;switch(k){case 0:b->buft=(ggml_backend_buffer_type_t)(uintptr_t)v;return;case 1:b->context=(void*)(uintptr_t)v;return;case 2:b->size=v;return;case 3:b->usage=(ggml_backend_buffer_usage)v;return;}}if(kind==1){auto*c=(ggml_backend_multi_buffer_context*)(uintptr_t)p;if(k==0)c->buffers=(ggml_backend_buffer_t*)(uintptr_t)v;else c->n_buffers=v;return;}if(kind==2){auto*t=(ggml_tensor*)(uintptr_t)p;if(k==0)t->buffer=(ggml_backend_buffer_t)(uintptr_t)v;else if(k==1)t->data=(void*)(uintptr_t)v;else GGML_ABORT("Invalid tensor binding field");return;}if(kind==3){auto*vectors=(ggml_backend_buft_alloc_buffer_n_plan_item*)(uintptr_t)p;switch(k){case 0:vectors[i].size=v;return;case 1:vectors[i].first=(int)v;return;case 2:vectors[i].last=(int)v;return;}}GGML_ABORT("Invalid buffer projection write");}
uint64_t fg162_callback(int64_t p,int64_t kind,int64_t k){if(kind==0){auto&f=((ggml_backend_buffer_type*)(uintptr_t)p)->iface;switch(k){case 0:return(uintptr_t)f.get_name;case 1:return(uintptr_t)f.alloc_buffer;case 2:return(uintptr_t)f.alloc_buffer_n;case 3:return(uintptr_t)f.get_alignment;case 4:return(uintptr_t)f.get_max_size;case 5:return(uintptr_t)f.get_alloc_size;case 6:return(uintptr_t)f.get_alloc_size_n;case 7:return(uintptr_t)f.is_host;}}else{auto&f=((ggml_backend_buffer*)(uintptr_t)p)->iface;switch(k){case 0:return(uintptr_t)f.free_buffer;case 1:return(uintptr_t)f.get_base;case 2:return(uintptr_t)f.init_tensor;case 3:return(uintptr_t)f.clear;case 4:return(uintptr_t)f.reset;case 5:return(uintptr_t)f.cpy_tensor;}}GGML_ABORT("Invalid buffer callback slot");}
uint64_t fg162_buft_call(int64_t which,int64_t p,int64_t t,uint64_t n){auto*b=(ggml_backend_buffer_type*)(uintptr_t)p;switch(which){case 0:return(uintptr_t)b->iface.get_name(b);case 1:try{return(uintptr_t)b->iface.alloc_buffer(b,n);}catch(const std::bad_alloc&){return 0;}case 2:return(uintptr_t)b->iface.alloc_buffer_n(b,(ggml_tensor**)(uintptr_t)t,(int)n);case 3:return b->iface.get_alignment(b);case 4:return b->iface.get_max_size(b);case 5:return b->iface.get_alloc_size(b,(const ggml_tensor*)(uintptr_t)t);case 6:return b->iface.get_alloc_size_n(b,(ggml_tensor**)(uintptr_t)t,(int)n);case 7:return b->iface.is_host(b);default:GGML_ABORT("Invalid buft callback");}}
uint64_t fg162_buffer_call(int64_t which,int64_t p,int64_t t,uint64_t v){auto*b=(ggml_backend_buffer*)(uintptr_t)p;switch(which){case 0:b->iface.free_buffer(b);return 0;case 1:return(uintptr_t)b->iface.get_base(b);case 2:try{return(uint64_t)(int64_t)b->iface.init_tensor(b,(ggml_tensor*)(uintptr_t)t);}catch(const std::bad_alloc&){return(uint64_t)(int64_t)GGML_STATUS_ALLOC_FAILED;}case 3:b->iface.clear(b,(uint8_t)v);return 0;case 4:b->iface.reset(b);return 0;case 5:return b->iface.cpy_tensor(b,(const ggml_tensor*)(uintptr_t)t,(ggml_tensor*)(uintptr_t)v);default:GGML_ABORT("Invalid buffer callback");}}
void fg162_iface_copy(int64_t p,int64_t iface){((ggml_backend_buffer*)(uintptr_t)p)->iface=*(const ggml_backend_buffer_i*)(uintptr_t)iface;}
int64_t fg162_workspace(int64_t p,int64_t which){auto*q=(fg162_packet*)(uintptr_t)p;return(int64_t)(uintptr_t)(which==0?(void*)&q->prototype:(void*)&q->empty_iface);}
int64_t fg162_multi_iface(void){return(int64_t)(uintptr_t)&ggml_backend_multi_buffer_i;}
uint64_t fg162_multi_free_marker(void){return(uintptr_t)&ggml_backend_multi_buffer_free_buffer;}
uint64_t fg162_nbytes(int64_t t){return ggml_nbytes((const ggml_tensor*)(uintptr_t)t);}
int64_t fg162_quantized(int64_t t){return ggml_is_quantized(((const ggml_tensor*)(uintptr_t)t)->type);}
int64_t fg162_is_meta(int64_t p){return ggml_backend_buffer_is_meta((ggml_backend_buffer_t)(uintptr_t)p);}
void fg162_meta_usage(int64_t p,int64_t usage){ggml_backend_meta_buffer_set_usage((ggml_backend_buffer_t)(uintptr_t)p,(ggml_backend_buffer_usage)usage);}
void fg162_tallocr_init(int64_t p,int64_t b){((fg162_packet*)(uintptr_t)p)->tallocr=ggml_tallocr_new((ggml_backend_buffer_t)(uintptr_t)b);}
int64_t fg162_tallocr_step(int64_t p,int64_t t){return ggml_tallocr_alloc(&((fg162_packet*)(uintptr_t)p)->tallocr,(ggml_tensor*)(uintptr_t)t);}
void fg162_snapshot(int64_t p,int64_t i,int64_t t,int64_t restore){auto*a=(ggml_tensor*)(uintptr_t)p;auto*b=(ggml_tensor*)(uintptr_t)t;if(restore)memcpy(b,a+i,sizeof(*b));else memcpy(a+i,b,sizeof(*b));}
uint64_t fg162_ptr(int64_t p,int64_t i){uintptr_t v;memcpy(&v,(const unsigned char*)(uintptr_t)p+(size_t)i*sizeof(v),sizeof(v));return v;}
void fg162_ptr_put(int64_t p,int64_t i,uint64_t v){uintptr_t q=(uintptr_t)v;memcpy((unsigned char*)(uintptr_t)p+(size_t)i*sizeof(q),&q,sizeof(q));}
void fg162_fail(int64_t code){GGML_ABORT("Goose backend buffer validation failed:code=%" PRId64,code);}
static uint64_t fg162_run(fg162_packet*p){uint64_t bit=UINT64_C(1)<<p->v[0];if(!(fg162_thread_seen&bit)){fg162_thread_seen|=bit;fg162_seen.fetch_or(bit,std::memory_order_relaxed);}return fg162_goose_entry((int64_t)(uintptr_t)p);}
__attribute__((constructor)) static void fg162_config(void){const char*v=getenv("GGML_GOOSE_BACKEND_BUFFERS");fg162_enabled=!(v&&v[0]=='0'&&v[1]==0);}
extern "C" __attribute__((visibility("default"))) uint64_t fg162_buffers_seen(void){return fg162_seen.load(std::memory_order_relaxed);}
extern "C" __attribute__((visibility("default"))) int64_t fg162_buffer_heap_live(void){return fg162_heaps.load();}
#ifdef FG162_TESTING
extern "C" __attribute__((visibility("default"))) void fg162_fail_allocation_at(int n){fg162_attempt.store(0);fg162_fail_at.store(n);}
extern "C" __attribute__((visibility("default"))) int fg162_allocation_attempts(void){return fg162_attempt.load();}
#endif
__attribute__((destructor)) static void fg162_report(void){auto m=fg162_buffers_seen();if(m)fprintf(stderr,"FG162_BUFFERS modes=0x%" PRIx64 ",live_buffer_heaps=%" PRId64 "\n",m,fg162_buffer_heap_live());}
#endif
