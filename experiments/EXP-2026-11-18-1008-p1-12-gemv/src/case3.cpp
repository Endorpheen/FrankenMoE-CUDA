// Localize the group-6 divergence: same inputs, three references, several ith/nth.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <vector>
#include "ggml.h"
typedef struct { const uint8_t* src0; int64_t ne00,ne01,nb01; const uint8_t* act; int64_t nbw1;
                 const int32_t* routes; int64_t cne1; int64_t nb0,nb1,nb2; float* dst;
                 int32_t ith,nth; } args_t;
extern "C" int32_t fg_cpu_simd_mmid_tiled_q4_k(const args_t*, uint8_t*);
extern "C" void ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int main(int argc, char** argv) {
    const char* path = argc>1?argv[1]:"evidence-mmid/failing_case_group6.bin";
    FILE* f=fopen(path,"rb"); if(!f){perror("open");return 2;}
    int64_t ne00,ne01,nb01,nbw1,cne1,nb0,nb1,nb2,r0,r1;
    fread(&ne00,8,1,f);fread(&ne01,8,1,f);fread(&nb01,8,1,f);fread(&nbw1,8,1,f);
    fread(&cne1,8,1,f);fread(&nb0,8,1,f);fread(&nb1,8,1,f);fread(&nb2,8,1,f);fread(&r0,8,1,f);fread(&r1,8,1,f);
    std::vector<uint8_t> src0((size_t)(nb01*ne01)), act((size_t)(nbw1*cne1));
    fread(src0.data(),1,src0.size(),f); fread(act.data(),1,act.size(),f);
    std::vector<int32_t> routes((size_t)(2*cne1)); fread(routes.data(),4,routes.size(),f);
    std::vector<float> nat((size_t)((r1-r0)*cne1)), goo((size_t)((r1-r0)*cne1));
    for(size_t i=0;i<nat.size();i++){fread(&nat[i],4,1,f);fread(&goo[i],4,1,f);} fclose(f);

    // dense reference for the whole expert (all groups) from vec_dot
    std::vector<float> ref((size_t)(ne01*cne1));
    for(int64_t r=0;r<ne01;r++) for(int64_t m=0;m<cne1;m++)
        ggml_vec_dot_q4_K_q8_K((int)ne00,&ref[(size_t)(r*cne1+m)],0,src0.data()+r*nb01,0,act.data()+m*nbw1,0,1);

    const char* dis = getenv("FGOOSE_SIMD_CPU_DISABLE");
    printf("mode=%s ne00=%lld ne01=%lld cne1=%lld nb0=%lld nb1=%lld nb2=%lld routes=(%d,%d)\n",
           dis?"scalar":"wkl",(long long)ne00,(long long)ne01,(long long)cne1,(long long)nb0,(long long)nb1,(long long)nb2,routes[0],routes[1]);
    // sanity: dump's native vs vec_dot
    long nvd=0; for(size_t i=0;i<nat.size();i++) if(bits(nat[i])==bits(ref[(size_t)(r0*cne1+i/cne1)])) nvd++;
    printf("dump native == vecdot: %ld/%zu\n", nvd, nat.size());

    struct cfg { int ith, nth; } cfgs[] = {{0,1},{1,16},{7,16},{3,16},{13,16},{15,16}};
    const int64_t ngroups=(ne01+63)/64;
    for(auto& c: cfgs){
        std::vector<uint8_t> scratch((size_t)c.nth*512*1024, 0);
        const size_t dstb=(size_t)(nb0*ne01+nb1*(routes[0]+2)+nb2*(routes[1]+2)+4096);
        std::vector<uint8_t> out(dstb,0);
        args_t a{src0.data(),ne00,ne01,nb01,act.data(),nbw1,routes.data(),cne1,nb0,nb1,nb2,(float*)out.data(),c.ith,c.nth};
        int32_t rc=fg_cpu_simd_mmid_tiled_q4_k(&a,scratch.data());
        const int64_t g0=(ngroups*c.ith)/c.nth, g1=(ngroups*(c.ith+1))/c.nth;
        const int64_t rr0=g0*64, rr1=std::min<int64_t>(g1*64,ne01);
        long eq=0,n=0,first=-1; double maxrel=0;
        for(int64_t r=rr0;r<rr1;r++) for(int64_t m=0;m<cne1;m++){
            const int64_t idx=(r-rr0)*nb0+(int64_t)routes[2*m]*nb1+(int64_t)routes[2*m+1]*nb2;
            float vg; memcpy(&vg,out.data()+idx,4);
            float vr=ref[(size_t)(r*cne1+m)];
            n++; if(bits(vg)==bits(vr)) eq++; else if(first<0) first=r;
            double rel=fabs((double)vg-(double)vr)/(fabs((double)vr)>1e-30?fabs((double)vr):1.0);
            if(rel>maxrel) maxrel=rel;
        }
        printf("ith=%2d nth=%2d groups=[%lld,%lld) rows=[%lld,%lld) rc=%d eq=%ld/%ld first_diff_row=%ld maxrel=%.3e\n",
               c.ith,c.nth,(long long)g0,(long long)g1,(long long)rr0,(long long)rr1,rc,eq,n,first,maxrel);
    }
    // locality probe: for group 0 with ith=1/nth=16 find what the kernel emitted
    {
        std::vector<uint8_t> scratch((size_t)16*512*1024, 0);
        const size_t dstb=(size_t)(nb0*ne01+nb1*(routes[0]+2)+nb2*(routes[1]+2)+4096);
        std::vector<uint8_t> out(dstb,0);
        args_t a{src0.data(),ne00,ne01,nb01,act.data(),nbw1,routes.data(),cne1,nb0,nb1,nb2,(float*)out.data(),1,16};
        fg_cpu_simd_mmid_tiled_q4_k(&a,scratch.data());
        printf("\n-- group0 probe (ith=1,nth=16) --\n");
        for(int64_t r=0;r<8;r++){
            const int64_t idx=r*nb0+(int64_t)routes[0]*nb1+(int64_t)routes[1]*nb2;
            float vg; memcpy(&vg,out.data()+idx,4);
            int found=-1; for(int64_t t=0;t<ne01;t++) if(bits(vg)==bits(ref[(size_t)(t*cne1+0)])){found=t;break;}
            printf("row %lld goose=%.9g ref=%.9g bits_match_row=%d\n",(long long)r,(double)vg,(double)ref[(size_t)(r*cne1+0)],found);
        }
    }
    return 0;
}
