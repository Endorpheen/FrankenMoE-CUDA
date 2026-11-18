#include <cstdio>
#include <cstdint>
#include <map>
#include <vector>
#include <algorithm>
extern "C" int64_t g_trydraft_call(const int64_t*,const int64_t*,const int64_t*,int64_t,int64_t,int64_t,int64_t*);
// native try_draft body, transcribed verbatim for this single level
static int64_t ref(const int64_t*tok,const int64_t*cp,const int64_t*cs,int n,int64_t ms,int64_t mp){
    int64_t best=-1, max_cp=0, max_cs=0, sum=0;
    for(int i=0;i<n;i++){
        int64_t w = cs[i]>=0 ? 100*cs[i] : 1;
        if(cp[i]*w > max_cp*max_cs){ best=tok[i]; max_cp=cp[i]; max_cs=w; }
        sum+=cp[i];
    }
    if(sum<ms) return -1;
    if(100*max_cp < mp*sum) return -1;
    return best;
}
struct C{const char*name;int64_t t[4],c[4],s[4];int cnt;int64_t ms,mp,want;};
int main(){
    const C cs[]={
      {"plain max wins",        {10,11,12,0},{5,3,1,0},{-1,-1,-1,0},3, 1,1, 10},
      {"static weight flips",   {10,11,12,0},{5,5,1,0},{-1,2,-1,0},3, 1,1, 11},
      {"static zero weight",    {10,11,12,0},{5,1,1,0},{-1,0,-1,0},3, 1,1, 10},
      {"min sample rejects",    {10,11,12,0},{5,3,1,0},{-1,-1,-1,0},3, 10,1, -1},
      {"min percent boundary",  {10,11,12,0},{5,5,0,0},{-1,-1,-1,0},2, 1,50, 10},
      {"min percent rejects",   {10,11,12,0},{5,5,1,0},{-1,-1,-1,0},3, 1,50, -1},
      {"single token",          {7,0,0,0},{1,0,0,0},{-1,0,0,0},1, 1,1, 7},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int i=0;i<n;++i){int64_t o[2]={0,0};
        int64_t r=g_trydraft_call(cs[i].t,cs[i].c,cs[i].s,cs[i].cnt,cs[i].ms,cs[i].mp,o);
        int64_t e=ref(cs[i].t,cs[i].c,cs[i].s,cs[i].cnt,cs[i].ms,cs[i].mp);
        if(r!=cs[i].want||r!=e){fails++;printf("MISMATCH %s goose=%lld ref=%lld want=%lld\n",cs[i].name,(long long)r,(long long)e,(long long)cs[i].want);}}
    printf("ngram cache level: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
