#include <cstdio>
#include <cstdint>
#include "limits-gen.h"
extern "C" { void fg471_st_i64(uint64_t,uint64_t,int64_t); }
void fg471_st_i64(uint64_t p,uint64_t o,int64_t v){ *(int64_t*)((char*)(uintptr_t)p+o)=v; }
static int cases=0,checks=0,fails=0;
static void bad(const char*w,int64_t g,int64_t e){++fails;if(fails<=10)printf("FAIL %s got=%lld want=%lld\n",w,(long long)g,(long long)e);}
int main(void){ goose_init(0,0);
 for(int ex=0;ex<2;++ex) for(int64_t v: (int64_t[]){0,1,30,3600,7200}){
  int64_t o[1]={-1}; timeout_value(ex,v,(uint64_t)(uintptr_t)o);
  int64_t want = ex ? (v>0?v:3600) : 3600;
  ++cases;++checks; if(o[0]!=want) bad("timeout",o[0],want);
 }
 for(int rp=0;rp<2;++rp) for(int sup=0;sup<2;++sup){
  int64_t o[3]={-1,-1,-1}; socket_opts(rp,sup,(uint64_t)(uintptr_t)o);
  int64_t wrp = (rp&&sup)?1:0, warn=(rp&&!sup)?1:0;
  ++cases;++checks; if(o[0]!=1||o[1]!=wrp||o[2]!=warn) bad("socket",o[1],wrp);
 }
 for(int64_t st: (int64_t[]){200,400,404,500,503}){
  int64_t o[1]={-1}; error_404(st,(uint64_t)(uintptr_t)o);
  int64_t want = st==404?1:0;
  ++cases;++checks; if(o[0]!=want) bad("error404",o[0],want);
 }
 printf("FG471_LIMITS cases=%d values=%d failures=%d\n",cases,checks,fails);
 return fails?1:0; }
