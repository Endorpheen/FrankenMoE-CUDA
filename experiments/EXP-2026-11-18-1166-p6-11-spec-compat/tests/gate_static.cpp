#include <cstdio>
#include <cstdint>
extern "C" int64_t g_thr_call(int64_t,int64_t*);
extern "C" int64_t g_static_call(const int64_t*,const int64_t*,int64_t);
int main(){
    int fails=0;
    // threshold tables, verbatim from ngram-cache.cpp
    const int64_t s_lax[4]={2,2,1,1}, p_lax[4]={66,50,50,50};
    for(int i=0;i<4;++i){int64_t o[2]={-1,-1};
        g_thr_call(i,o);
        if(o[0]!=s_lax[i]||o[1]!=p_lax[i]){fails++;printf("THR MISMATCH lvl %d got %lld/%lld want %lld/%lld\n",i,(long long)o[0],(long long)o[1],(long long)s_lax[i],(long long)p_lax[i]);}}
    struct D{const char*n;int64_t t[3],c[3];int64_t cnt,want;};
    const D ds[]={
      {"plain max",        {10,11,12},{5,3,1},3, 10},
      {"percent ok",       {10,11,12},{2,1,1},3, 10},
      {"percent reject",   {10,11,12},{2,2,2},3, -1},
      {"sample reject",    {10,0,0},{1,0,0},1, -1},
      {"tie keeps first",  {10,11,12},{5,5,0},3, 10},
      {"single strong",    {7,0,0},{2,0,0},1, 7},
    };
    int n=(int)(sizeof(ds)/sizeof(ds[0]));
    for(int i=0;i<n;++i){
        int64_t r=g_static_call(ds[i].t,ds[i].c,ds[i].cnt);
        if(r!=ds[i].want){fails++;printf("STATIC MISMATCH %s got=%lld want=%lld\n",ds[i].n,(long long)r,(long long)ds[i].want);}
    }
    printf("static ngram: cases=%d fails=%d\n",n+4,fails);
    return fails?1:0;
}
