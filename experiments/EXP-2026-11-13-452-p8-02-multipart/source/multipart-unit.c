#include <stdint.h>
#include "multipart-qualified.c"
int64_t fg452_decode(uint64_t a,int64_t b,uint64_t c,int64_t d){return fg452_function_1(a,b,c,d,0);}
int64_t fg452_actions(int64_t n,const char *keys,int64_t klen,const int64_t *koffs,const int64_t *klens,int64_t *out){
  fg452_slice s; s.data=(void*)keys; s.len=klen;
  fg452_slice so; so.data=(void*)koffs; so.len=n;
  fg452_slice sl; sl.data=(void*)klens; sl.len=n;
  fg452_slice sr; sr.data=(void*)out;   sr.len=n;
  return fg452_function_5(n,s,so,sl,sr,0);}
int64_t fg452_source(int64_t a,int64_t b){return fg452_function_8(a,b,0);}
