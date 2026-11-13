// Differential fixture for the schema-shaping decisions (EXP472).
#include <cstdio>
#include <cstdint>
#include <vector>
#include <random>
#include <climits>
#include "schema-gen.h"
extern "C" { void fg472_st_i64(uint64_t,uint64_t,int64_t); }
void fg472_st_i64(uint64_t p,uint64_t o,int64_t v){ *(int64_t*)((char*)(uintptr_t)p+o)=v; }
static int cases=0,checks=0,fails=0;
static void bad(const char*w,int64_t g,int64_t e){++fails;if(fails<=10)printf("FAIL %s got=%lld want=%lld\n",w,(long long)g,(long long)e);}

// transcription of build_repetition's branch selection
static int native_kind(int64_t mn,int64_t mx,bool sep){
 const bool has_max = mx!=INT_MAX;
 if(mx==0) return 0;
 if(mn==0&&mx==1) return 1;
 if(!sep){ if(mn==1&&!has_max) return 2; if(mn==0&&!has_max) return 3; }
 else return 5;
 return 4;
}
int main(void){ goose_init(0,0);
 const int64_t maxes[]={0,1,2,5,INT_MAX};
 for(int64_t mn: (int64_t[]){0,1,2,3,5}) for(int64_t mx: maxes) for(int sep=0;sep<2;++sep){
  int64_t o[1]={-1}; repetition_kind(mn,mx,sep,(uint64_t)(uintptr_t)o);
  int want=native_kind(mn,mx,sep!=0);
  ++cases;++checks; if(o[0]!=want) bad("repetition_kind",o[0],want);
  if(sep && want==5){
   int64_t s[2]={-1,-1}; separator_form(mn,(uint64_t)(uintptr_t)s);
   const int64_t want_inner = mn==0?0:mn-1;
   const int64_t want_wrap = mn==0?1:0;
   ++cases;++checks; if(s[0]!=want_inner||s[1]!=want_wrap) bad("separator_form",s[0],want_inner);
  }
 }
 // object shape vs the native required/optional split
 std::mt19937 rng(472001);
 for(int round=0;round<300;++round){
  const int n=1+(int)(rng()%8);
  std::vector<int64_t> flags(n);
  int64_t req=0,opt=0;
  for(int i=0;i<n;++i){ flags[i]=(rng()%2); if(flags[i])++req; else ++opt; }
  const int add=(rng()%2);
  if(add) ++opt;
  sl_i64 f{flags.data(),(int64_t)n};
  int64_t o[4]={-1,-1,-1,-1};
  object_shape(f,add,(uint64_t)(uintptr_t)o);
  ++cases;++checks; if(o[0]!=req) bad("req",o[0],req);
  ++checks; if(o[1]!=opt) bad("opt",o[1],opt);
  ++checks; if(o[2]!=opt) bad("branches",o[2],opt);
  ++checks; if(o[3]!=((req==0&&opt==0)?1:0)) bad("empty",o[3],(req==0&&opt==0)?1:0);
  int64_t p[1]={-1}; optional_group_prefix(req,opt,(uint64_t)(uintptr_t)p);
  ++checks; if(p[0]!=((req>0&&opt>0)?1:0)) bad("prefix",p[0],(req>0&&opt>0)?1:0);
 }
 printf("FG472_SCHEMA cases=%d values=%d failures=%d\n",cases,checks,fails);
 return fails?1:0; }
