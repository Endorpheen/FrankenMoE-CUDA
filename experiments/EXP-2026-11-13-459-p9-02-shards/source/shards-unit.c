#include <stdint.h>
#include "shards-qualified.c"
int64_t fg459_digits(int64_t v){return fg459_function_1(v,0);}
int64_t fg459_width(int64_t v){return fg459_function_2(v,0);}
int64_t fg459_split_path(const char *pre,int64_t pn,int64_t no,int64_t count,char *out,int64_t cap){
  fg459_slice p; p.data=(void*)pre; p.len=pn;
  fg459_slice o; o.data=(void*)out; o.len=cap;
  return fg459_function_3(p,no,count,o,0);}
int64_t fg459_first(int64_t i){return fg459_function_5(i,0);}
int64_t fg459_count_ok(int64_t a,int64_t b){return fg459_function_8(a,b,0);}
int64_t fg459_index_ok(int64_t a,int64_t b){return fg459_function_8(a,b,0);}
int64_t fg459_tensor_ok(int64_t a,int64_t b){return fg459_function_8(a,b,0);}
int64_t fg459_magic(int64_t v){return fg459_function_9(v,0);}
int64_t fg459_version(int64_t v){return fg459_function_10(v,0);}
