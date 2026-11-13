#ifndef FG159_TEST_EXPORTS_H
#define FG159_TEST_EXPORTS_H
#define TEST_API __attribute__((visibility("default")))
TEST_API void fg159_probe_hash_new(struct ggml_hash_set*out,size_t n);
void fg159_probe_hash_new(struct ggml_hash_set*out,size_t n){*out=ggml_hash_set_new(n);}
TEST_API void fg159_probe_hash_reset(struct ggml_hash_set*h);
void fg159_probe_hash_reset(struct ggml_hash_set*h){ggml_hash_set_reset(h);}
TEST_API void fg159_probe_hash_free(struct ggml_hash_set*h);
void fg159_probe_hash_free(struct ggml_hash_set*h){ggml_hash_set_free(h);}
TEST_API size_t fg159_probe_hash_size(size_t n);
size_t fg159_probe_hash_size(size_t n){return ggml_hash_size(n);}
TEST_API size_t fg159_probe_hash_op(int mode,struct ggml_hash_set*h,struct ggml_tensor*k);
size_t fg159_probe_hash_op(int mode,struct ggml_hash_set*h,struct ggml_tensor*k){switch(mode){case 0:return ggml_hash_find(h,k);case 1:return ggml_hash_insert(h,k);case 2:return ggml_hash_find_or_insert(h,k);case 3:return ggml_hash_contains(h,k);case 4:return ggml_hash(k);default:abort();}}
TEST_API size_t fg159_probe_bits(int mode,ggml_bitset_t*p,size_t i);
size_t fg159_probe_bits(int mode,ggml_bitset_t*p,size_t i){switch(mode){case 0:return ggml_bitset_size(i);case 1:return ggml_bitset_get(p,i);case 2:ggml_bitset_set(p,i);return 0;case 3:ggml_bitset_clear(p,i);return 0;default:abort();}}
TEST_API void fg159_probe_graph_view(struct ggml_cgraph*out,struct ggml_cgraph*g,int lo,int hi);
void fg159_probe_graph_view(struct ggml_cgraph*out,struct ggml_cgraph*g,int lo,int hi){*out=ggml_graph_view(g,lo,hi);}
TEST_API size_t fg159_probe_visit(struct ggml_cgraph*g,struct ggml_tensor*t,bool compute);
size_t fg159_probe_visit(struct ggml_cgraph*g,struct ggml_tensor*t,bool compute){return ggml_visit_parents_graph(g,t,compute);}
TEST_API size_t fg159_probe_graph_nbytes(size_t n,bool grads);
size_t fg159_probe_graph_nbytes(size_t n,bool grads){return ggml_graph_nbytes(n,grads);}
#endif
