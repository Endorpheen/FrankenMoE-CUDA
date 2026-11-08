#include <stdint.h>
struct ggml_tensor;
struct fg153_shape {int64_t ne[4],nb[4],type_size,block_size;};
int64_t fg153_shape_entry(int64_t,const struct fg153_shape*,const struct fg153_shape*,int64_t,int64_t);
int fg153_shapes_enabled(void);
int64_t fg153_shape_query(int64_t,const struct ggml_tensor*,const struct ggml_tensor*,int64_t,int64_t);
