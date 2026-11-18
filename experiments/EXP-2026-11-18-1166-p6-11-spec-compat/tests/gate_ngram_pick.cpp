#include <cstdio>
#include <cstdint>
extern "C" int64_t g_ngrampick_call(const int64_t*,const int64_t*,int64_t,int64_t,int64_t*);
struct C{const char*n;int64_t v[4],a[4],m;int64_t wr,ws,wl;};
int main(){
    const C cs[]={
      {"dominant value",   {5,1,0,0},{8,4,0,0},4, 1,0,4},
      {"dominant clamp",   {5,1,0,0},{2,4,0,0},4, 1,0,2},
      {"not 2x",           {3,2,1,0},{8,8,8,0},4, 0,0,0},
      {"exactly 2x ok",    {4,1,1,0},{8,8,8,0},4, 1,0,4},
      {"single slot",      {7,0,0,0},{3,0,0,0},4, 1,0,3},
      {"tie keeps first",  {5,5,0,0},{2,9,0,0},4, 0,0,0},
      {"tie with rest 0",  {5,0,0,0},{1,0,0,0},2, 1,0,1},
      {"all zero",         {0,0,0,0},{0,0,0,0},4, 1,0,0},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int i=0;i<n;++i){int64_t o[2]={-1,-1};
        int64_t r=g_ngrampick_call(cs[i].v,cs[i].a,cs[i].m,4,o);
        if(r!=cs[i].wr||(r>0&&(o[0]!=cs[i].ws||o[1]!=cs[i].wl))){fails++;
            printf("MISMATCH %s r=%lld slot=%lld len=%lld\n",cs[i].n,(long long)r,(long long)o[0],(long long)o[1]);}}
    printf("ngram pick: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
