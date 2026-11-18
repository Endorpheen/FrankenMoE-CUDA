// Reference: the native inner loop, transcribed, applied to the same table.
#include <cstdio>
#include <cstdint>
#include <algorithm>
extern "C" int64_t g_dp_call(int64_t*,int64_t,const int64_t*,const int64_t*,const int64_t*,int64_t,int64_t*,int64_t*,int64_t*,int64_t);
static const int64_t BIG = (int64_t)1 << 60;
static int64_t ref_step(int64_t*sc,int64_t at,const int64_t*pl,const int64_t*ps,const int64_t*pi,int np,
                        int64_t*ol,int64_t*ot,int64_t*on,int64_t c){
    int64_t chosen=-1;
    for(int i=0;i<np;++i){
        int64_t score=ps[i];
        if(score > -2){
            int64_t L=pl[i];
            int64_t s=sc[at+L]-score;
            if(s<sc[at]){
                chosen=i;
                sc[at]=s;
                ol[at]=L; ot[at]=pi[i];
                on[at]=on[at+L]+1;
                if(score==-1) on[at]+=(c>=0x80)+(c>=0x800)+(c>=0x10000);
            }
        }
        if(score==-1) break;
    }
    return chosen;
}
int main(){
    struct C{const char*n;int64_t pl[4],ps[4],pi[4];int np;int64_t at,c;};
    const C cs[]={
      {"better piece wins",   {1,2},{5,1},{10,11}, 2, 0, 97},
      {"longer is cheaper",   {1,2},{1,1},{10,11}, 2, 0, 97},
      {"tie keeps first",     {1,1},{5,5},{10,11}, 2, 0, 97},
      {"unknown last",        {1,2},{5,-1},{10,11},2, 0, 233},
      {"unknown only",        {3},{2},{10},        1, 0, 9601},
      {"ascii unknown",       {1},{3},{10},        1, 0, 97},
    };
    int fails=0,n=(int)(sizeof(cs)/sizeof(cs[0]));
    for(int k=0;k<n;++k){
        int64_t sc1[8],sc2[8],ol[8],ot[8],on[8],ol2[8],ot2[8],on2[8];
        for(int i=0;i<8;++i){sc1[i]=sc2[i]=BIG;ol[i]=ot[i]=on[i]=ol2[i]=ot2[i]=on2[i]=0;}
        sc1[6]=sc2[6]=0;  // pretend the string ends at 6
        if(cs[k].at+cs[k].pl[0]>6) { continue; }
        if(cs[k].np>1 && cs[k].at+cs[k].pl[1]>6) { continue; }
        int64_t gc=g_dp_call(sc1,cs[k].at,cs[k].pl,cs[k].ps,cs[k].pi,cs[k].np,ol,ot,on,cs[k].c);
        int64_t rc=ref_step(sc2,cs[k].at,cs[k].pl,cs[k].ps,cs[k].pi,cs[k].np,ol2,ot2,on2,cs[k].c);
        if(gc!=rc||sc1[cs[k].at]!=sc2[cs[k].at]||ol[cs[k].at]!=ol2[cs[k].at]||ot[cs[k].at]!=ot2[cs[k].at]||on[cs[k].at]!=on2[cs[k].at]){
            fails++;printf("MISMATCH %s goose(ch=%lld score=%lld len=%lld tok=%lld num=%lld) ref(ch=%lld score=%lld len=%lld tok=%lld num=%lld)\n",
                cs[k].n,(long long)gc,(long long)sc1[cs[k].at],(long long)ol[cs[k].at],(long long)ot[cs[k].at],(long long)on[cs[k].at],
                (long long)rc,(long long)sc2[cs[k].at],(long long)ol2[cs[k].at],(long long)ot2[cs[k].at],(long long)on2[cs[k].at]);}
    }
    printf("spm dp: cases=%d fails=%d\n",n,fails);
    return fails?1:0;
}
