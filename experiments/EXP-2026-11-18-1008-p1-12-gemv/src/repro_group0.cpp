// Minimal repro: group 0 of the tiled Q4_K kernel is wrong, groups 1..7 are exact.
// usage: repro_group0 evidence-mmid/failing_case.bin
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>
#include "ggml.h"
typedef struct { const uint8_t* src0; int64_t ne00,ne01,nb01; const uint8_t* act; int64_t nbw1;
                 const int32_t* routes; int64_t cne1; int64_t nb0,nb1,nb2; float* dst;
                 int32_t ith,nth; } args_t;
extern "C" int32_t fg_cpu_simd_mmid_tiled_q4_k(const args_t*, uint8_t*);
extern "C" void ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);

// Reference: the q4_K x q8_K dot straight from the block format, with the same
// float order the kernel uses (d*s1, fmaf(-dmin,s2), fmaf(d_act,res,acc)).
// ggml_vec_dot_q4_K_q8_K and its _generic entry both return 0 for external
// callers in this build, so they cannot serve as the reference here.
static float f16b(const uint8_t * p) {
    uint32_t h = (uint32_t) p[0] | ((uint32_t) p[1] << 8);
    uint32_t sign = (h & 0x8000u) << 16, e = (h >> 10) & 31u, f = h & 1023u, bits;
    if (e == 31u) bits = sign | 2139095040u | (f << 13);
    else if (e == 0u) {
        if (f == 0u) bits = sign;
        else { int32_t pwr = -14; while ((f & 1024u) == 0u) { f <<= 1; pwr -= 1; } f &= 1023u;
               bits = sign | (((uint32_t) (pwr + 127)) << 23) | (f << 13); }
    } else bits = sign | ((e + 112u) << 23) | (f << 13);
    float v; memcpy(&v, &bits, 4); return v;
}
static uint32_t u32b(const uint8_t * p) {
    return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}
static float ref_dot(const uint8_t * w, const uint8_t * a, int64_t k) {
    const uint32_t km1 = 0x3F3F3F3Fu, km2 = 0x0F0F0F0Fu, km3 = 0x03030303u;
    float total = 0.0f;
    for (int64_t kb = 0; kb < k / 256; kb++) {
        const uint8_t * blk = w + kb * 144;
        const float d = f16b(blk), dmin = f16b(blk + 2);
        uint32_t u0 = u32b(blk + 4), u1 = u32b(blk + 8), u2 = u32b(blk + 12);
        uint32_t u3 = ((u2 >> 4) & km2) | (((u1 >> 6) & km3) << 4);
        uint32_t aux = u1 & km1;
        uint32_t u1n = (u2 & km2) | (((u0 >> 6) & km3) << 4);
        uint32_t u0n = u0 & km1;
        int32_t sc[8], mn[8];
        for (int s = 0; s < 4; s++) {
            sc[s] = (int32_t) ((u0n >> (8 * s)) & 255u);
            sc[4 + s] = (int32_t) ((u1n >> (8 * s)) & 255u);
            mn[s] = (int32_t) ((aux >> (8 * s)) & 255u);
            mn[4 + s] = (int32_t) ((u3 >> (8 * s)) & 255u);
        }
        const uint8_t * qb = blk + 16;
        const uint8_t * ab = a + kb * 292;
        const float da = *(const float *) ab;
        const int8_t * q1 = (const int8_t *) (ab + 4);
        const int16_t * bs = (const int16_t *) (ab + 260);
        int32_t s1 = 0, s2 = 0;
        for (int s = 0; s < 8; s++) {
            const uint8_t * src = qb + 32 * (s / 2);
            int32_t sub = 0;
            for (int e = 0; e < 32; e++) {
                const int v = (s & 1) ? ((src[e] >> 4) & 15) : (src[e] & 15);
                sub += v * (int32_t) q1[s * 32 + e];
            }
            s1 = (int32_t) ((uint32_t) s1 + (uint32_t) (sc[s] * sub));
            s2 = (int32_t) ((uint32_t) s2 + (uint32_t) (mn[s] * (int32_t) (bs[2 * s] + bs[2 * s + 1])));
        }
        float res = d * (float) s1;
        res = __builtin_fmaf(-dmin, (float) s2, res);
        total = __builtin_fmaf(da, res, total);
    }
    return total;
}

static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int main(int argc,char**argv){
    const char*p=argc>1?argv[1]:"evidence-mmid/failing_case.bin";
    FILE*f=fopen(p,"rb"); if(!f){perror("open");return 2;}
    int64_t ne00,ne01,nb01,nbw1,cne1,nb0,nb1,nb2,r0,r1;
    fread(&ne00,8,1,f);fread(&ne01,8,1,f);fread(&nb01,8,1,f);fread(&nbw1,8,1,f);fread(&cne1,8,1,f);
    fread(&nb0,8,1,f);fread(&nb1,8,1,f);fread(&nb2,8,1,f);fread(&r0,8,1,f);fread(&r1,8,1,f);
    std::vector<uint8_t> src0((size_t)(nb01*ne01)),act((size_t)(nbw1*cne1));
    fread(src0.data(),1,src0.size(),f);fread(act.data(),1,act.size(),f);
    std::vector<int32_t> routes((size_t)(2*cne1));fread(routes.data(),4,routes.size(),f);
    long total=(long)((r1-r0)*cne1), bad=0;
    for(int32_t ith=0; ith<16; ++ith){
        const int64_t ng=(ne01+63)/64;
        const int64_t g0=(ng*ith)/16, g1=(ng*(ith+1))/16;
        if(g0>=g1) continue;
        std::vector<uint8_t> scratch((size_t)16*512*1024,0);
        const size_t db=(size_t)(nb0*ne01+nb1*((int64_t)routes[0]+2)+nb2*((int64_t)routes[1]+2)+4096);
        std::vector<uint8_t> out(db,0);
        args_t a{src0.data(),ne00,ne01,nb01,act.data(),nbw1,routes.data(),cne1,nb0,nb1,nb2,(float*)out.data(),ith,16};
        fg_cpu_simd_mmid_tiled_q4_k(&a,scratch.data());
        long eq=0,n=0,first=-1;
        for(int64_t r=g0*64;r<std::min<int64_t>(g1*64,ne01);r++) for(int64_t m=0;m<cne1;m++){
            const float vr = ref_dot(src0.data()+r*nb01, act.data()+m*nbw1, ne00);
            const int64_t idx=r*nb0+(int64_t)routes[2*m]*nb1+(int64_t)routes[2*m+1]*nb2;
            float vg; memcpy(&vg,out.data()+idx,4);
            n++; if(bits(vg)==bits(vr)) eq++; else { bad++; if(first<0){first=r; printf("diff row %lld: goose=%.9g vecdot=%.9g\n",(long long)r,(double)vg,(double)vr);} }
        }
        printf("ith=%2d nth=16 group=%lld rows=[%lld,%lld) eq=%ld/%ld first_diff=%ld\n",ith,(long long)g0,(long long)(g0*64),(long long)std::min<int64_t>(g1*64,ne01),eq,n,first);
    }
    printf("total rows compared=%ld diffs=%ld\n", total, bad);
    return bad?1:0;
}
