#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include "ggml.h"
#include "ggml-backend.h"
#include "ggml-cpu.h"
extern "C" void ggml_vec_dot_q4_K_q8_K(int,float*,size_t,const void*,size_t,const void*,size_t,int);
int main(int argc,char**argv){
    const char*p=argc>1?argv[1]:"evidence-mmid/failing_case.bin";
    FILE*f=fopen(p,"rb"); if(!f)return 2;
    int64_t ne00,ne01,nb01,nbw1,cne1,nb0,nb1,nb2,r0,r1;
    fread(&ne00,8,1,f);fread(&ne01,8,1,f);fread(&nb01,8,1,f);fread(&nbw1,8,1,f);fread(&cne1,8,1,f);
    fread(&nb0,8,1,f);fread(&nb1,8,1,f);fread(&nb2,8,1,f);fread(&r0,8,1,f);fread(&r1,8,1,f);
    std::vector<uint8_t> src0((size_t)(nb01*ne01)),act((size_t)(nbw1*cne1));
    fread(src0.data(),1,src0.size(),f);fread(act.data(),1,act.size(),f);
    std::vector<int32_t> routes((size_t)(2*cne1));fread(routes.data(),4,routes.size(),f);
    std::vector<float> nat((size_t)((r1-r0)*cne1)),goo((size_t)((r1-r0)*cne1));
    for(size_t i=0;i<nat.size();i++){fread(&nat[i],4,1,f);fread(&goo[i],4,1,f);} fclose(f);
    printf("dump rows %lld..%lld, routes=(%d,%d)\n",(long long)r0,(long long)r1,routes[0],routes[1]);
    for(int64_t r=r0;r<r0+6;r++){
        float v0=0; ggml_vec_dot_q4_K_q8_K((int)ne00,&v0,0,src0.data()+r*nb01,0,act.data(),0,1);
        printf("row %lld: dump_native=%.9g dump_goose=%.9g vecdot_noinit=%.9g\n",(long long)r,(double)nat[(size_t)((r-r0)*cne1)],(double)goo[(size_t)((r-r0)*cne1)],(double)v0);
    }
    ggml_backend_t cpu=ggml_backend_cpu_init();
    printf("-- after ggml_backend_cpu_init() --\n");
    for(int64_t r=r0;r<r0+6;r++){
        float v1=0; ggml_vec_dot_q4_K_q8_K((int)ne00,&v1,0,src0.data()+r*nb01,0,act.data(),0,1);
        printf("row %lld: vecdot_init=%.9g dump_native=%.9g dump_goose=%.9g\n",(long long)r,(double)v1,(double)nat[(size_t)((r-r0)*cne1)],(double)goo[(size_t)((r-r0)*cne1)]);
    }
    ggml_backend_free(cpu);
    return 0;
}
