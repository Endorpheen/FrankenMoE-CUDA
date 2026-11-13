// Differential fixture for response assembly / logging decisions (EXP470).
#include <cstdio>
#include <cstdint>
#include <string>

#include "response-gen.h"

extern "C" { void fg470_st_i64(uint64_t,uint64_t,int64_t); }

void fg470_st_i64(uint64_t p,uint64_t o,int64_t v){ *(int64_t*)((char*)(uintptr_t)p+o)=v; }

static int cases=0, checks=0, fails=0;
static void fail(const char*w,int64_t got,int64_t want){
 ++fails; if(fails<=10) printf("FAIL %s got=%lld want=%lld\n",w,(long long)got,(long long)want);
}

int main(void){
 goose_init(0,0);
 for(int s=0;s<2;++s){
  ++cases;++checks; if(body_mode(s)!=(s?1:0)) fail("body_mode",body_mode(s),s?1:0);
  ++checks; if(accel_override(s)!=(s?1:0)) fail("accel",accel_override(s),s?1:0);
 }
 // status: plain 200, error payload code
 for(int e=0;e<2;++e){
  const int64_t codes[]={400,401,404,500,501,503};
  for(int64_t c: codes){
   const int64_t want=e?c:200;
   ++cases;++checks; if(status_for(e,c)!=want) fail("status",status_for(e,c),want);
  }
 }
 // logging: categories 1..5 skipped, others logged
 for(int64_t cat=0;cat<=9;++cat){
  const int64_t want=(cat>=1&&cat<=5)?1:0;
  ++cases;++checks; if(log_skip(cat)!=want) fail("log_skip",log_skip(cat),want);
 }
 // content type precedence: stream value wins over handler value
 for(int s=0;s<2;++s) for(int h=0;h<2;++h){
  int64_t out[1]={-1};
  content_type(s,h,(uint64_t)(uintptr_t)out);
  const int64_t want=s?2:(h?1:0);
  ++cases;++checks; if(out[0]!=want) fail("content_type",out[0],want);
 }
 printf("FG470_RESPONSE cases=%d values=%d failures=%d\n",cases,checks,fails);
 return fails?1:0;
}
