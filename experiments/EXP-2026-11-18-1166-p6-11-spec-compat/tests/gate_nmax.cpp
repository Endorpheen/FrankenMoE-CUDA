#include <cstdio>
#include <cstdint>
extern "C" int64_t g_nmax_call(const int64_t*,const int64_t*,const int64_t*,int64_t);
struct C{const char*n;int64_t t[8],s[8],d[8];int64_t cnt,want;};
int main(){
    const C cs[]={
      {"none only",      {0},{0},{0},1, 0},
      {"mtp width",      {3},{0},{5},1, 5},
      {"negative width", {1},{0},{-3},1, 0},
      {"ngram simple 4", {6},{4},{0},1, 4},
      {"map k4v 7",      {8},{7},{0},1, 7},
      {"cache is 8",     {10},{0},{0},1, 8},
      {"max across mix", {3,6,9},{0,2,0},{5,0,11},3, 11},
      {"draft then ngram",{2,7},{0,9},{3,0},2, 9},
      {"count type",     {11},{0},{0},1, 0},
      {"zero types",     {0},{0},{0},0, 0},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int i=0;i<n;++i){
        int64_t r=g_nmax_call(cs[i].t,cs[i].s,cs[i].d,cs[i].cnt);
        if(r!=cs[i].want){fails++;printf("MISMATCH %s got=%lld want=%lld\n",cs[i].n,(long long)r,(long long)cs[i].want);}}
    printf("spec n_max: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
