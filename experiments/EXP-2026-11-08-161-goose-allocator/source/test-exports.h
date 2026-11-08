#ifndef FG161_TEST_EXPORTS
#define FG161_TEST_EXPORTS
#define TEST_API __attribute__((visibility("default")))
TEST_API void *fg161_probe_dyn_new(size_t align,size_t max);
void *fg161_probe_dyn_new(size_t align,size_t max){return ggml_dyn_tallocr_new(align,max);}
TEST_API void fg161_probe_dyn_free(void*p);
void fg161_probe_dyn_free(void*p){ggml_dyn_tallocr_free(p);}
TEST_API void fg161_probe_dyn_reset(void*p);
void fg161_probe_dyn_reset(void*p){ggml_dyn_tallocr_reset(p);}
TEST_API void fg161_probe_dyn_alloc(void*p,size_t n,uint64_t*out);
void fg161_probe_dyn_alloc(void*p,size_t n,uint64_t*out){struct buffer_address a=ggml_dyn_tallocr_alloc(p,n,NULL);out[0]=(uint64_t)(int64_t)a.chunk;out[1]=a.offset;}
TEST_API void fg161_probe_dyn_release(void*p,int chunk,size_t off,size_t n);
void fg161_probe_dyn_release(void*p,int chunk,size_t off,size_t n){struct buffer_address a={chunk,off};ggml_dyn_tallocr_free_bytes(p,a,n);}
static size_t fg161_probe_append_dyn(const struct ggml_dyn_tallocr*a,uint64_t*out,size_t at,size_t cap){
#define EMIT(v) do{GGML_ASSERT(at<cap);out[at++]=(uint64_t)(v);}while(0)
 EMIT(a->alignment);EMIT(a->max_chunk_size);EMIT(a->n_chunks);for(int c=0;c<a->n_chunks;++c){const struct tallocr_chunk*ch=a->chunks[c];EMIT(ch->max_size);EMIT(ch->n_free_blocks);for(int i=0;i<ch->n_free_blocks;++i){EMIT(ch->free_blocks[i].offset);EMIT(ch->free_blocks[i].size);}}
#undef EMIT
 return at;}
TEST_API size_t fg161_probe_dyn_snapshot(void*p,uint64_t*out,size_t cap);
size_t fg161_probe_dyn_snapshot(void*p,uint64_t*out,size_t cap){return fg161_probe_append_dyn(p,out,0,cap);}
TEST_API size_t fg161_probe_graph_snapshot(ggml_gallocr_t g,struct ggml_cgraph*graph,uint64_t*out,size_t cap);
size_t fg161_probe_graph_snapshot(ggml_gallocr_t g,struct ggml_cgraph*graph,uint64_t*out,size_t cap){size_t at=0;
#define EMIT(v) do{GGML_ASSERT(at<cap);out[at++]=(uint64_t)(v);}while(0)
 EMIT(g->n_buffers);EMIT(g->n_nodes);EMIT(g->n_leafs);
 for(int i=0;i<g->n_buffers;++i){int owner=i,bowner=i;for(int j=0;j<i;++j){if(g->buf_tallocs[j]==g->buf_tallocs[i]){owner=j;break;}}for(int j=0;j<i;++j){if(g->buffers[j]==g->buffers[i]){bowner=j;break;}}EMIT(owner);EMIT(bowner);EMIT(g->buffer_growth_limits[i]);at=fg161_probe_append_dyn(g->buf_tallocs[i],out,at,cap);}
 for(int group=0;group<2;++group){int n=group?graph->n_leafs:graph->n_nodes;for(int i=0;i<n;++i){struct ggml_tensor*t=group?graph->leafs[i]:graph->nodes[i];size_t pos=ggml_hash_find(&g->hash_set,t);if(pos==GGML_HASHSET_FULL||!ggml_bitset_get(g->hash_set.used,pos)){EMIT(SIZE_MAX);continue;}struct hash_node*h=&g->hash_values[pos];EMIT(h->n_children);EMIT(h->n_views);EMIT(h->buffer_id);EMIT(h->addr.chunk);EMIT(h->addr.offset);EMIT(h->allocated);}}
#undef EMIT
 return at;}
#ifdef FG161_ALLOCATOR_H
TEST_API int fg161_probe_allocation_attempts(void);
int fg161_probe_allocation_attempts(void){return atomic_load(&fg161_attempt);}
#endif
#endif
