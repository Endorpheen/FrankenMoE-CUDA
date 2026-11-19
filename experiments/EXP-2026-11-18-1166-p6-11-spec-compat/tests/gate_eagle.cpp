#include <cstdio>
#include <cstdint>
extern "C" int64_t g_eagle_call(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t*);
struct C{const char*n;int64_t seq,nseq,rows,nacc,vpf;int64_t wret,wrow,wpos;};
int main(){
    const C cs[]={
      {"mid reject",      0,1,4,2,100, 2,2,102},
      {"all accepted",    0,1,4,4,100, 2,3,103},
      {"none accepted",   0,1,4,0,100, 2,0,100},
      {"clamp above",     0,1,4,9,100, 2,3,103},
      {"single row",      0,1,1,5,50,  2,0,50},
      {"no rows",         0,1,0,2,100, 0,0,0},
      {"seq too high",    3,3,4,2,100, 0,0,0},
      {"seq negative",   -1,1,4,2,100, 0,0,0},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int i=0;i<n;++i){int64_t o[2]={-1,-1};
        int64_t r=g_eagle_call(cs[i].seq,cs[i].nseq,cs[i].rows,cs[i].nacc,cs[i].vpf,o);
        if(r!=cs[i].wret||(r>0&&(o[0]!=cs[i].wrow||o[1]!=cs[i].wpos))){fails++;
            printf("MISMATCH %s r=%lld row=%lld pos=%lld\n",cs[i].n,(long long)r,(long long)o[0],(long long)o[1]);}}
    printf("eagle accept: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
