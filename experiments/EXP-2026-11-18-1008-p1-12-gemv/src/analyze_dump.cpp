#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include "ggml.h"
extern "C" void ggml_vec_dot_q4_K_q8_K(int, float*, size_t, const void*, size_t, const void*, size_t, int);
int main() {
    FILE * f = fopen("/tmp/mmid_dump.bin", "rb");
    if (!f) return 2;
    int64_t ne00, ne01, nb01, nbw1, cne1, nb0, nb1, nb2, r0, r1;
    fread(&ne00,8,1,f); fread(&ne01,8,1,f); fread(&nb01,8,1,f); fread(&nbw1,8,1,f);
    fread(&cne1,8,1,f); fread(&nb0,8,1,f); fread(&nb1,8,1,f); fread(&nb2,8,1,f);
    fread(&r0,8,1,f); fread(&r1,8,1,f);
    std::vector<uint8_t> src0((size_t)(nb01*ne01)), act((size_t)(nbw1*cne1));
    fread(src0.data(),1,src0.size(),f); fread(act.data(),1,act.size(),f);
    std::vector<int32_t> routes((size_t)(2*cne1)); fread(routes.data(),4,routes.size(),f);
    long cnt = (long)((r1-r0)*cne1);
    std::vector<float> nat((size_t)cnt), goo((size_t)cnt);
    for (long i=0;i<cnt;i++) { fread(&nat[i],4,1,f); fread(&goo[i],4,1,f); }
    fclose(f);
    long m_nat=0,m_goo=0,m_none=0, n=0;
    for (int64_t r=r0;r<r1;r++) for (int64_t m=0;m<cne1;m++) {
        float vd=0; ggml_vec_dot_q4_K_q8_K((int)ne00,&vd,0,src0.data()+r*nb01,0,act.data()+m*nbw1,0,1);
        const long i=(long)((r-r0)*cne1+m);
        const bool en = memcmp(&vd,&nat[i],4)==0, eg = memcmp(&vd,&goo[i],4)==0;
        m_nat += en; m_goo += eg; if(!en&&!eg) m_none++; n++;
        if (n<=5) printf("r=%lld m=%lld vecdot=%.9g native=%.9g goose=%.9g  vd==nat:%d vd==goose:%d\n",
                         (long long)r,(long long)m,(double)vd,(double)nat[i],(double)goo[i],en,eg);
    }
    printf("TOTAL %ld  vecdot==native %ld  vecdot==goose %ld  match neither %ld\n", n, m_nat, m_goo, m_none);
    long h_off0=0, h_offr0=0;
    for (int64_t r=r0;r<r1;r++) for (int64_t m=0;m<cne1;m++) {
        const long i=(long)((r-r0)*cne1+m);
        float a=0,b=0;
        ggml_vec_dot_q4_K_q8_K((int)ne00,&a,0,src0.data()+0*nb01,0,act.data()+m*nbw1,0,1);
        ggml_vec_dot_q4_K_q8_K((int)ne00,&b,0,src0.data()+(r-r0)*nb01,0,act.data()+m*nbw1,0,1);
        if (!memcmp(&a,&goo[i],4)) h_off0++;
        if (!memcmp(&b,&goo[i],4)) h_offr0++;
    }
    printf("goose==vecdot(row 0) %ld/%ld   goose==vecdot(row r-r0) %ld/%ld\n", h_off0, n, h_offr0, n);
    return 0;
}
