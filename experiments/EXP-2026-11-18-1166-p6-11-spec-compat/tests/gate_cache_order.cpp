#include <cstdio>
#include <cstdint>
extern "C" int64_t g_order_call(const int64_t*,int64_t,int64_t,int64_t,int64_t*);
struct C{const char*n;int64_t m[5];int64_t nm,mn,mx;int64_t wr,wlvl,wsrc;};
int main(){
    const C cs[]={
      {"highest level first",  {0,0,0,4,0},5,1,3, 1,3,2},
      {"context before dyn",   {0,3,0,0,0},5,1,3, 1,1,0},
      {"dynamic before static",{0,6,0,0,0},5,1,3, 1,1,1},
      {"static only",          {0,4,0,0,0},5,1,3, 1,1,2},
      {"fall to lower level",  {0,0,1,0,0},5,1,3, 1,2,0},
      {"min level wins",       {0,2,2,0,0},5,2,3, 1,2,1},
      {"none",                 {0,0,0,0,0},5,1,3, 0,-1,-1},
      {"all levels",           {0,4,4,4,4},5,1,4, 1,4,2},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int i=0;i<n;++i){int64_t o[2]={-9,-9};
        int64_t r=g_order_call(cs[i].m,cs[i].nm,cs[i].mn,cs[i].mx,o);
        if(r!=cs[i].wr||(r>0&&(o[0]!=cs[i].wlvl||o[1]!=cs[i].wsrc))){fails++;
            printf("MISMATCH %s r=%lld lvl=%lld src=%lld\n",cs[i].n,(long long)r,(long long)o[0],(long long)o[1]);}}
    printf("cache order: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
