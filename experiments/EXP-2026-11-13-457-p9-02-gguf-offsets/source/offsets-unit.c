#include <stdint.h>
#include "offsets-qualified.c"
int64_t fg457_up(int64_t x,int64_t n){return fg457_function_1(x,n,0);}
int64_t fg457_ok(int64_t a){return fg457_function_2(a,0);}
int64_t fg457_nbytes(int64_t t,int64_t b,int64_t ne,int64_t *out){return fg457_function_3(t,b,ne,(uint64_t)(uintptr_t)out,0);}
int64_t fg457_layout(const int64_t *rel,const int64_t *nbv,int64_t n,int64_t align,int64_t data_start,int64_t *out){
  fg457_slice r; r.data=(void*)rel; r.len=n;
  fg457_slice b; b.data=(void*)nbv; b.len=n;
  fg457_slice o; o.data=(void*)out; o.len=n+1;
  return fg457_function_6(r,b,align,data_start,o,0);}
