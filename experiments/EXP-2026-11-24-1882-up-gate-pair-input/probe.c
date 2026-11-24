#include <cuda.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { width=2048, hidden=512, routes=8, extent=8, layers=10 };
static void check(CUresult error, const char *call) {
    if (error != CUDA_SUCCESS) {
        const char *message;
        cuGetErrorString(error,&message);
        fprintf(stderr,"%s: %s\n",call,message);
        exit(2);
    }
}
#define CHECK(call) check((call),#call)

static void launch(CUfunction f, unsigned x, unsigned y, void **args, CUstream s) {
    CHECK(cuLaunchKernel(f,x,y,1,128,1,1,0,s,args,NULL));
}
static float timing(CUgraphExec graph, CUstream stream, CUevent a, CUevent b) {
    enum { replays=64 };
    for (int i=0;i<8;++i) CHECK(cuGraphLaunch(graph,stream));
    CHECK(cuEventRecord(a,stream));
    for (int i=0;i<replays;++i) CHECK(cuGraphLaunch(graph,stream));
    CHECK(cuEventRecord(b,stream));
    CHECK(cuEventSynchronize(b));
    float ms;
    CHECK(cuEventElapsedTime(&ms,a,b));
    return ms*1000/(replays*layers);
}

int main(int argc,char **argv) {
    if (argc!=4) return 2;
    CUmodule reference,control,candidate;
    CUdevice device;
    CUcontext context;
    CUfunction quant,original,fused;
    CHECK(cuInit(0));
    CHECK(cuDeviceGet(&device,0));
    CHECK(cuCtxCreate(&context,0,device));
    CHECK(cuModuleLoad(&reference,argv[1]));
    CHECK(cuModuleLoad(&control,argv[2]));
    CHECK(cuModuleLoad(&candidate,argv[3]));
    CHECK(cuModuleGetFunction(&quant,reference,"goose_mmvq_quantize"));
    CHECK(cuModuleGetFunction(&original,control,"goose_mmvq_up_gate_glu"));
    CHECK(cuModuleGetFunction(&fused,candidate,"goose_mmvq_up_gate_glu"));

    uint64_t stride=extent*144;
    const size_t input_bytes=width*sizeof(float), output_bytes=hidden*routes*sizeof(float);
    const size_t plane=stride*hidden, bank_bytes=2*layers*routes*plane;
    CUdeviceptr weights, input, packed, outputs[2], table;
    CHECK(cuMemAlloc(&weights,bank_bytes));
    CHECK(cuMemAlloc(&input,input_bytes));
    CHECK(cuMemAlloc(&packed,extent*288*routes));
    CHECK(cuMemAlloc(&outputs[0],layers*output_bytes));
    CHECK(cuMemAlloc(&outputs[1],layers*output_bytes));
    uint64_t jobs[layers][2][routes][3]={0};
    CHECK(cuMemAlloc(&table,sizeof(jobs)));
    for (unsigned l=0;l<layers;++l) {
        for (unsigned p=0;p<2;++p) for (unsigned r=0;r<routes;++r) {
            jobs[l][p][r][1]=r*hidden*sizeof(float);
            jobs[l][p][r][2]=((l*2+p)*routes+r)*plane;
        }
    }
    CHECK(cuMemcpyHtoD(table,jobs,sizeof(jobs)));
    uint8_t *host=malloc(bank_bytes), *a=malloc(layers*output_bytes),*b=malloc(layers*output_bytes);
    if (!host || !a || !b) return 2;
    float row[width];
    uint32_t seed=57;
    for (size_t at=0; at<bank_bytes; at+=144) {
        for (unsigned j=0; j<144; ++j) {
            seed=seed*UINT32_C(1664525)+UINT32_C(1013904223);
            host[at+j]=(uint8_t)(seed>>24);
        }
        host[at]=0;
        host[at+1]=28+(at/144)%8;
        host[at+2]=0;
        host[at+3]=27+(at/144)%6;
    }
    CHECK(cuMemcpyHtoD(weights,host,bank_bytes));
    CUstream stream;
    CUevent before,after;
    CUgraph graphs[2]; CUgraphExec exec[2];
    CHECK(cuStreamCreate(&stream,CU_STREAM_DEFAULT));
    CHECK(cuEventCreate(&before,0));
    CHECK(cuEventCreate(&after,0));
    int columns=extent, words=3;
    for (unsigned arm=0;arm<2;++arm) {
        CHECK(cuStreamBeginCapture(stream,CU_STREAM_CAPTURE_MODE_THREAD_LOCAL));
        for (unsigned l=0;l<layers;++l) {
            CUdeviceptr up_jobs=table+l*sizeof(jobs[l]), gate_jobs=up_jobs+sizeof(jobs[l][0]);
            CUdeviceptr dst=outputs[arm]+l*output_bytes;
            void *args[]={&weights,&packed,&dst,&up_jobs,&gate_jobs,&stride,&columns};
            launch(arm ? fused : original,hidden,routes,args,stream);
        }
        CHECK(cuStreamEndCapture(stream,&graphs[arm]));
        CHECK(cuGraphInstantiateWithFlags(&exec[arm],graphs[arm],0));
        CHECK(cuGraphUpload(exec[arm],stream));
    }
    for (int sample=0;sample<3;++sample) {
        for (unsigned i=0;i<width;++i) {
            seed=seed*UINT32_C(1664525)+UINT32_C(1013904223);
            row[i]=sample==0 ? 0.0f : ((int)(seed>>23)-256.0f)/(512*sample);
        }
        CHECK(cuMemcpyHtoD(input,row,input_bytes));
        CUdeviceptr first_jobs=table;
        void *args[]={&input,&packed,&first_jobs,&columns,&words};
        launch(quant,extent*2,routes,args,stream);
        CHECK(cuGraphLaunch(exec[0],stream));
        CHECK(cuGraphLaunch(exec[1],stream));
        CHECK(cuStreamSynchronize(stream));
        CHECK(cuMemcpyDtoH(a,outputs[0],layers*output_bytes));
        CHECK(cuMemcpyDtoH(b,outputs[1],layers*output_bytes));
        if (memcmp(a,b,layers*output_bytes)) {
            size_t i=0;
            while (a[i]==b[i]) ++i;
            fprintf(stderr,"FP32 mismatch: sample=%d entry=%zu\n",sample,i/4);
            return 1;
        }
    }
    printf("Compared %zu FP32 values bitwise\n",3*layers*output_bytes/4);
    const unsigned order[]={0,1,1,0};
    for (unsigned i=0;i<sizeof(order)/sizeof(*order);++i) {
        const unsigned arm=order[i];
        printf("%s %.3f us per layer\n",arm?"fused":"reference",timing(exec[arm],stream,before,after));
    }
    CHECK(cuCtxDestroy(context));
    free(host); free(a); free(b);
}
