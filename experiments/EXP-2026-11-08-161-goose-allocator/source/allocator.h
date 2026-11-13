#ifndef FG161_ALLOCATOR_H
#define FG161_ALLOCATOR_H
#include <stdatomic.h>
#include <inttypes.h>
_Static_assert(sizeof(uintptr_t)==8&&sizeof(size_t)==8,"64-bit allocator ABI");
static atomic_int_fast64_t fg161_heaps;
static atomic_uint_fast64_t fg161_seen;
static _Thread_local uint64_t fg161_thread_seen;
#ifdef FG161_TESTING
static atomic_int fg161_fail_at;
static atomic_int fg161_attempt;
static bool fg161_should_fail(void){int n=atomic_load(&fg161_fail_at);return n>0&&atomic_fetch_add(&fg161_attempt,1)+1==n;}
#else
static bool fg161_should_fail(void){return false;}
#endif
static inline uint64_t fg161_arg(int64_t p,int64_t i){return ((const uint64_t*)(intptr_t)p)[i];}
static inline uint64_t fg161_constant(int64_t k){switch(k){case 0:return sizeof(struct ggml_tallocr);case 1:return sizeof(struct ggml_dyn_tallocr);case 2:return sizeof(struct tallocr_chunk);case 3:return sizeof(struct vbuffer);case 4:return sizeof(struct ggml_gallocr);case 5:return sizeof(struct hash_node);case 6:return sizeof(struct tensor_alloc);case 7:return sizeof(struct node_alloc);case 8:return sizeof(struct leaf_alloc);case 10:return GGML_VBUFFER_MAX_CHUNKS;case 11:return MAX_FREE_BLOCKS;case 12:return GGML_MAX_SRC;case 13:return GGML_TENSOR_FLAG_INPUT;case 14:return GGML_TENSOR_FLAG_OUTPUT;case 15:return GGML_BACKEND_BUFFER_USAGE_COMPUTE;case 16:return GGML_OP_NONE;default:GGML_ABORT("Invalid allocator constant");}}
static inline uint64_t fg161_get(int64_t p,int64_t kind,int64_t k,int64_t i){
 if(kind==0){const struct ggml_tallocr*a=(const void*)(intptr_t)p;switch(k){case 0:return (uintptr_t)a->buffer;case 1:return (uintptr_t)a->base;case 2:return a->alignment;case 3:return a->offset;}}
 if(kind==1){const struct ggml_dyn_tallocr*a=(const void*)(intptr_t)p;switch(k){case 0:return a->alignment;case 1:return a->max_chunk_size;case 2:return (uint64_t)(int64_t)a->n_chunks;case 3:return (uintptr_t)a->chunks[i];}}
 if(kind==2){const struct tallocr_chunk*a=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)(int64_t)a->n_free_blocks;case 1:return a->max_size;case 2:return a->free_blocks[i].offset;case 3:return a->free_blocks[i].size;}}
 if(kind==3)return (uintptr_t)((const struct vbuffer*)(intptr_t)p)->chunks[i];
 if(kind==4){const struct ggml_gallocr*a=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)(int64_t)a->n_buffers;case 1:return i<0?(uintptr_t)a->bufts:(uintptr_t)a->bufts[i];case 2:return i<0?(uintptr_t)a->buffers:(uintptr_t)a->buffers[i];case 3:return i<0?(uintptr_t)a->buf_tallocs:(uintptr_t)a->buf_tallocs[i];case 4:return i<0?(uintptr_t)a->buffer_growth_limits:a->buffer_growth_limits[i];case 5:return (uintptr_t)&a->hash_set;case 6:return (uintptr_t)a->hash_values;case 7:return (uint64_t)(int64_t)a->n_nodes;case 8:return (uintptr_t)a->node_allocs;case 9:return (uint64_t)(int64_t)a->n_leafs;case 10:return (uintptr_t)a->leaf_allocs;}}
 if(kind==5){const struct hash_node*a=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)(int64_t)a->n_children;case 1:return (uint64_t)(int64_t)a->n_views;case 2:return (uint64_t)(int64_t)a->buffer_id;case 3:return (uint64_t)(int64_t)a->addr.chunk;case 4:return a->addr.offset;case 5:return a->allocated;}}
 if(kind==6){const struct tensor_alloc*a=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)(int64_t)a->buffer_id;case 1:return (uint64_t)(int64_t)a->addr.chunk;case 2:return a->addr.offset;case 3:return a->size_max;}}
 if(kind==7){const struct ggml_tensor*a=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)a->ne[i];case 1:return a->nb[i];case 2:return a->type;case 3:return a->op;case 4:return (uint64_t)a->flags;case 5:return (uintptr_t)a->buffer;case 6:return (uintptr_t)a->data;case 7:return (uintptr_t)a->view_src;case 8:return a->view_offs;case 9:return (uintptr_t)a->src[i];}}
 if(kind==8){const struct ggml_cgraph*a=(const void*)(intptr_t)p;switch(k){case 0:return (uint64_t)(int64_t)a->n_nodes;case 1:return (uint64_t)(int64_t)a->n_leafs;case 2:return (uintptr_t)a->nodes[i];case 3:return (uintptr_t)a->leafs[i];}}
 if(kind==9){const struct ggml_hash_set*a=(const void*)(intptr_t)p;switch(k){case 0:return a->size;case 1:return (uintptr_t)a->used;case 2:return (uintptr_t)a->keys;}}
 if(kind==10){const struct buffer_address*a=(const void*)(intptr_t)p;return k==0?(uint64_t)(int64_t)a->chunk:a->offset;}
 GGML_ABORT("Invalid allocator projection");}
static inline void fg161_put(int64_t p,int64_t kind,int64_t k,int64_t i,uint64_t v){
 if(kind==0){struct ggml_tallocr*a=(void*)(intptr_t)p;switch(k){case 0:a->buffer=(void*)(uintptr_t)v;return;case 1:a->base=(void*)(uintptr_t)v;return;case 2:a->alignment=v;return;case 3:a->offset=v;return;}}
 if(kind==1){struct ggml_dyn_tallocr*a=(void*)(intptr_t)p;switch(k){case 0:a->alignment=v;return;case 1:a->max_chunk_size=v;return;case 2:a->n_chunks=(int)v;return;case 3:a->chunks[i]=(void*)(uintptr_t)v;return;}}
 if(kind==2){struct tallocr_chunk*a=(void*)(intptr_t)p;switch(k){case 0:a->n_free_blocks=(int)v;return;case 1:a->max_size=v;return;case 2:a->free_blocks[i].offset=v;return;case 3:a->free_blocks[i].size=v;return;}}
 if(kind==3){((struct vbuffer*)(intptr_t)p)->chunks[i]=(void*)(uintptr_t)v;return;}
 if(kind==4){struct ggml_gallocr*a=(void*)(intptr_t)p;switch(k){case 0:a->n_buffers=(int)v;return;case 1:if(i<0)a->bufts=(void*)(uintptr_t)v;else a->bufts[i]=(void*)(uintptr_t)v;return;case 2:if(i<0)a->buffers=(void*)(uintptr_t)v;else a->buffers[i]=(void*)(uintptr_t)v;return;case 3:if(i<0)a->buf_tallocs=(void*)(uintptr_t)v;else a->buf_tallocs[i]=(void*)(uintptr_t)v;return;case 4:if(i<0)a->buffer_growth_limits=(void*)(uintptr_t)v;else a->buffer_growth_limits[i]=v;return;case 6:a->hash_values=(void*)(uintptr_t)v;return;case 7:a->n_nodes=(int)v;return;case 8:a->node_allocs=(void*)(uintptr_t)v;return;case 9:a->n_leafs=(int)v;return;case 10:a->leaf_allocs=(void*)(uintptr_t)v;return;}}
 if(kind==5){struct hash_node*a=(void*)(intptr_t)p;switch(k){case 0:a->n_children=(int)v;return;case 1:a->n_views=(int)v;return;case 2:a->buffer_id=(int)v;return;case 3:a->addr.chunk=(int)v;return;case 4:a->addr.offset=v;return;case 5:a->allocated=v!=0;return;}}
 if(kind==6){struct tensor_alloc*a=(void*)(intptr_t)p;switch(k){case 0:a->buffer_id=(int)v;return;case 1:a->addr.chunk=(int)v;return;case 2:a->addr.offset=v;return;case 3:a->size_max=v;return;}}
 if(kind==9){struct ggml_hash_set*a=(void*)(intptr_t)p;switch(k){case 0:a->size=v;return;case 1:a->used=(void*)(uintptr_t)v;return;case 2:a->keys=(void*)(uintptr_t)v;return;}}
 if(kind==10){struct buffer_address*a=(void*)(intptr_t)p;if(k==0)a->chunk=(int)v;else a->offset=v;return;}
 GGML_ABORT("Invalid allocator projection write");}
static inline int64_t fg161_record(int64_t p,int64_t kind,int64_t i,int64_t sub){struct ggml_gallocr*g=(void*)(intptr_t)p;switch(kind){case 0:return (int64_t)(intptr_t)&g->hash_values[i];case 1:return (int64_t)(intptr_t)(sub<0?&g->node_allocs[i].dst:&g->node_allocs[i].src[sub]);case 2:return (int64_t)(intptr_t)&g->leaf_allocs[i].leaf;case 5:return (int64_t)(intptr_t)&((struct hash_node*)(intptr_t)p)->addr;default:GGML_ABORT("Invalid allocator record");}}
static inline int64_t fg161_heap(uint64_t n,int64_t clear){void*p=fg161_should_fail()?NULL:(clear?calloc(1,(size_t)n):malloc((size_t)n));if(p)atomic_fetch_add(&fg161_heaps,1);return (int64_t)(intptr_t)p;}
static inline void fg161_release(int64_t p){if(p){atomic_fetch_sub(&fg161_heaps,1);free((void*)(intptr_t)p);}}
static inline void fg161_zero(int64_t p,uint64_t n){if(n)memset((void*)(intptr_t)p,0,(size_t)n);}
static inline void fg161_copy(int64_t d,int64_t s,uint64_t n){if(n)memmove((void*)(intptr_t)d,(const void*)(intptr_t)s,(size_t)n);}
static inline uint64_t fg161_ptr(int64_t p,int64_t i){uintptr_t v;memcpy(&v,(const unsigned char*)(intptr_t)p+(size_t)i*sizeof(v),sizeof(v));return v;}
static inline void fg161_ptr_put(int64_t p,int64_t i,uint64_t v){uintptr_t q=(uintptr_t)v;memcpy((unsigned char*)(intptr_t)p+(size_t)i*sizeof(q),&q,sizeof(q));}
static inline int64_t fg161_i32(int64_t p,int64_t i){int32_t v;memcpy(&v,(const unsigned char*)(intptr_t)p+(size_t)i*sizeof(v),sizeof(v));return v;}
static inline void fg161_i32_put(int64_t p,int64_t i,int64_t v){int32_t q=(int32_t)v;memcpy((unsigned char*)(intptr_t)p+(size_t)i*sizeof(q),&q,sizeof(q));}
static inline uint64_t fg161_buffer(int64_t p,int64_t what,int64_t t){ggml_backend_buffer_t b=(void*)(intptr_t)p;switch(what){case 0:return (uintptr_t)ggml_backend_buffer_get_base(b);case 1:return ggml_backend_buffer_get_alignment(b);case 2:return ggml_backend_buffer_get_size(b);case 3:return ggml_backend_buffer_get_alloc_size(b,(const void*)(intptr_t)t);default:GGML_ABORT("Invalid buffer effect");}}
static inline uint64_t fg161_buft(int64_t p,int64_t what,int64_t t){ggml_backend_buffer_type_t b=(void*)(intptr_t)p;switch(what){case 0:return ggml_backend_buft_get_alignment(b);case 1:return ggml_backend_buft_get_max_size(b);case 2:return ggml_backend_buft_get_alloc_size(b,(const void*)(intptr_t)t);default:GGML_ABORT("Invalid buft effect");}}
static inline int64_t fg161_alloc_buffer(int64_t p,uint64_t n){return (int64_t)(intptr_t)ggml_backend_buft_alloc_buffer((void*)(intptr_t)p,(size_t)n);}
static inline void fg161_free_buffer(int64_t p){ggml_backend_buffer_free((void*)(intptr_t)p);}
static inline void fg161_usage(int64_t p,int64_t usage){ggml_backend_buffer_set_usage((void*)(intptr_t)p,(enum ggml_backend_buffer_usage)usage);}
static inline void fg161_reset_buffer(int64_t p){ggml_backend_buffer_reset((void*)(intptr_t)p);}
static inline int64_t fg161_tensor_alloc(int64_t b,int64_t t,uint64_t addr){return ggml_backend_tensor_alloc((void*)(intptr_t)b,(void*)(intptr_t)t,(void*)(uintptr_t)addr);}
static inline void fg161_view_init(int64_t p){ggml_backend_view_init((void*)(intptr_t)p);}
static inline uint64_t fg161_hash_size(uint64_t n){return ggml_hash_size((size_t)n);}
static inline uint64_t fg161_hash_slot(int64_t p,int64_t t){struct ggml_gallocr*g=(void*)(intptr_t)p;return ggml_hash_find_or_insert(&g->hash_set,(void*)(intptr_t)t);}
static inline void fg161_hash_reset(int64_t p){ggml_hash_set_reset(&((struct ggml_gallocr*)(intptr_t)p)->hash_set);}
static inline int64_t fg161_first(int64_t p){return (int64_t)(intptr_t)ggml_get_first_tensor((void*)(intptr_t)p);}
static inline int64_t fg161_next(int64_t p,int64_t t){return (int64_t)(intptr_t)ggml_get_next_tensor((void*)(intptr_t)p,(void*)(intptr_t)t);}
static inline int64_t fg161_no_alloc(int64_t p){return ggml_get_no_alloc((void*)(intptr_t)p);}
static inline uint64_t fg161_bulk(int64_t b,int64_t tensors,int64_t n,int64_t size_only){return size_only?ggml_backend_buft_get_alloc_size_n((void*)(intptr_t)b,(void*)(intptr_t)tensors,(int)n):(uintptr_t)ggml_backend_buft_alloc_buffer_n((void*)(intptr_t)b,(void*)(intptr_t)tensors,(int)n);}
static inline int64_t fg161_default_buft(int64_t p){return (int64_t)(intptr_t)ggml_backend_get_default_buffer_type((void*)(intptr_t)p);}
static inline void fg161_fail(int64_t code){GGML_ABORT("Goose allocator validation failed:code=%" PRId64,code);}
#define FG161_ENTRY fg161_gg_function_1
#define main fg161_unused_generated_main
#include "allocator-qualified.c"
#undef main
static int fg161_enabled=1;
__attribute__((constructor)) static void fg161_config(void){const char*v=getenv("GGML_GOOSE_ALLOCATOR");fg161_enabled=!(v&&v[0]=='0'&&v[1]==0);
#ifdef GGML_ALLOCATOR_DEBUG
fg161_enabled=0;
#endif
}
static inline uint64_t fg161_run(const uint64_t*a){uint64_t bit=UINT64_C(1)<<a[0];if(!(fg161_thread_seen&bit)){fg161_thread_seen|=bit;atomic_fetch_or_explicit(&fg161_seen,bit,memory_order_relaxed);}return FG161_ENTRY((int64_t)(intptr_t)a,1);}
__attribute__((visibility("default"))) uint64_t fg161_allocator_seen(void);
uint64_t fg161_allocator_seen(void){return atomic_load_explicit(&fg161_seen,memory_order_relaxed);}
__attribute__((visibility("default"))) int64_t fg161_allocator_heap_live(void);
int64_t fg161_allocator_heap_live(void){return atomic_load(&fg161_heaps);}
#ifdef FG161_TESTING
__attribute__((visibility("default"))) void fg161_fail_allocation_at(int n);
void fg161_fail_allocation_at(int n){atomic_store(&fg161_attempt,0);atomic_store(&fg161_fail_at,n);}
#endif
__attribute__((destructor)) static void fg161_report(void){uint64_t m=fg161_allocator_seen();if(m)fprintf(stderr,"FG161_ALLOCATOR modes=0x%" PRIx64 ",live_allocator_heaps=%" PRId64 "\n",m,fg161_allocator_heap_live());}
#endif
