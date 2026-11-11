#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
extern "C" void fg256s_coeff(int64_t,int64_t,int64_t,int64_t,int64_t[2]);
static int32_t bits(float f){int32_t u;memcpy(&u,&f,4);return u;}
static float from(int32_t u){float f;memcpy(&f,&u,4);return f;}
int main(){
    std::mt19937 rng(256); int cases=0, failures=0;
    for (int t=0;t<20000;t++){
        int32_t ax=(int32_t)rng(), ay=(int32_t)rng(), bx=(int32_t)rng(), by=(int32_t)rng();
        int64_t out[2]={0,0};
        fg256s_coeff(ax,ay,bx,by,out);
        float rx = from(ax)*from(bx), ry = from(ay)*from(by);
        ++cases;
        if ((int32_t)out[0] != bits(rx) || (int32_t)out[1] != bits(ry)) {
            if (failures<3) printf("DIFF ax=%08x bx=%08x\n",(unsigned)ax,(unsigned)bx);
            ++failures;
        }
    }
    printf("FG256S_GATE cases=%d failures=%d\n",cases,failures);
    return failures?1:0;
}
