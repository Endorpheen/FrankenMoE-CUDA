#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
typedef struct { const void* data; int64_t len; } bytes_t;
extern "C" void ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);
extern "C" int32_t fg_cpu_simd_row_q4_k_q8_k(bytes_t,bytes_t,int32_t,float*);
static uint32_t bits(float f){uint32_t u;memcpy(&u,&f,4);return u;}
int main(int argc,char**argv){
    const char*p=argc>1?argv[1]:"evidence-mmid/failing_case.bin";
    FILE*f=fopen(p,"rb"); if(!f)return 2;
    int64_t ne00,ne01,nb01,nbw1,cne1,nb0,nb1,nb2,r0,r1;
    fread(&ne00,8,1,f);fread(&ne01,8,1,f);fread(&nb01,8,1,f);fread(&nbw1,8,1,f);fread(&cne1,8,1,f);
    fread(&nb0,8,1,f);fread(&nb1,8,1,f);fread(&nb2,8,1,f);fread(&r0,8,1,f);fread(&r1,8,1,f);
    std::vector<uint8_t> src0((size_t)(nb01*ne01)),act((size_t)(nbw1*cne1));
    fread(src0.data(),1,src0.size(),f);fread(act.data(),1,act.size(),f); fclose(f);
    ggml_backend_t cpu=ggml_backend_cpu_init();
    long eq=0,n=0,eq_noinit=0;
    float v_noinit=0; ggml_vec_dot_q4_K_q8_K((int)ne00,&v_noinit,0,src0.data()+2*nb01,0,act.data(),0,1);
    printf("row2 before init: %.9g\n",(double)v_noinit);
    for(int64_t r=0;r<ne01;r++){
        float a=0,g=0;
        ggml_vec_dot_q4_K_q8_K((int)ne00,&a,0,src0.data()+r*nb01,0,act.data(),0,1);
        fg_cpu_simd_row_q4_k_q8_k((bytes_t){src0.data()+r*nb01,nb01},(bytes_t){act.data(),nbw1},(int32_t)(ne00/256),&g);
        n++; if(bits(a)==bits(g)) eq++;
    }
    printf("ggml_vec_dot(init) vs delivered EXP665 row object: %ld/%ld bit-identical\n",eq,n);
    printf("sample row2: vec_dot=%.9g row_object=%.9g\n",(double)([&]{float a=0;ggml_vec_dot_q4_K_q8_K((int)ne00,&a,0,src0.data()+2*nb01,0,act.data(),0,1);return a;})(),0.0);
    ggml_backend_free(cpu);
    return eq==n?0:1;
}
