// test-effects.cpp - LOCAL MOCK of raw effects for canonical device flush checks.
// Real ggml effects live in services.c (compiled separately). Test harness only.
#include "canonical-io.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

struct T { int used; int64_t buf,nbytes,type,el,vsrc,voff,shape; unsigned char *data; };
static T TT[128]; static unsigned char POOL[1<<16]; static int64_t used_pool;
static int64_t buft_id = 0x1000;
static T * t(int64_t h){ return &TT[h-1]; }
static int64_t newt(int64_t nb,int64_t ty,int64_t el,int64_t shape,unsigned char* d){
    for(int i=0;i<128;i++) if(!TT[i].used){ TT[i]={1,buft_id,nb,ty,el,0,0,shape,d}; return i+1; }
    abort();
}
static int64_t align16(int64_t n){ return (n+15)/16*16; }

extern "C" int64_t fgcio_raw(int64_t op,int64_t a,int64_t b,int64_t c,int64_t d){
    switch(op){
    case 1: memcpy((void*)a,(void*)b,(size_t)c); return 0;
    case 2: memcpy((void*)a,(void*)b,(size_t)c); return 0;
    case 3: { FILE*f=(FILE*)a; return fwrite((void*)b,1,c,f)==(size_t)c?0:-1; }
    case 4: { FILE*f=(FILE*)a; return fread((void*)b,1,c,f)==(size_t)c?1:0; }
    case 5: { T*x=t(a); memcpy((void*)b,x->data+c,(size_t)d); return 0; }
    case 6: { T*x=t(a); memcpy(x->data+c,(void*)b,(size_t)d); return 0; }
    case 7: return *(int64_t*)a;
    case 8: *(int64_t*)a=b; return 0;
    case 10: { T*x=t(b); void*tmp=malloc(c); memcpy(tmp,x->data+d,c);
               FILE*f=(FILE*)a; size_t n=fwrite(tmp,1,c,f); free(tmp); return n==(size_t)c?0:-1; }
    case 11: { void*tmp=malloc(c); FILE*f=(FILE*)a; size_t n=fread(tmp,1,c,f);
               if(n==(size_t)c){T*x=t(b); memcpy(x->data+d,tmp,c);} free(tmp); return n==(size_t)c?1:0; }
    case 12: return t(a)->buf;
    case 13: return t(a)->el;
    case 14: return t(a)->type;
    case 15: return t(a)->nbytes;
    case 16: return t(a)->shape==t(b)->shape?1:0;
    case 17: return t(a)->vsrc;
    case 18: return t(a)->voff;
    case 19: return 0;
    case 20: { size_t n=t(a)->nbytes<t(b)->nbytes?t(a)->nbytes:t(b)->nbytes;
               memcpy(t(b)->data,t(a)->data,n); return 0; }
    case 21: return (int64_t)(intptr_t) new int64_t(a);
    case 22: delete (int64_t*)(intptr_t)a; return 0;
    case 23: { T*s=t(b); unsigned char* p=s->data+d; return newt(c*s->el,s->type,s->el,c?s->shape:c,p); }
    case 24: { unsigned char* p=POOL+used_pool; used_pool+=align16(c*4);
               return newt(c*4,b,4,c,p); }
    case 25: return 64;
    case 26: return buft_id;
    case 27: return 16;
    case 28: return t(b)->nbytes;
    case 29: return 1<<16;
    case 30: return a;
    case 31: return (int64_t)(intptr_t) new int64_t(0);
    case 32: { T*x=t(b); int64_t* u=(int64_t*)(intptr_t)a; x->data=POOL+*u; *u+=align16(x->nbytes); return 0; }
    case 33: delete (int64_t*)(intptr_t)a; return 0;
    case 34: fprintf(stderr,"FGTEST abort %lld %lld %lld\n",(long long)a,(long long)b,(long long)c); abort();
    case 38: return 0;
    case 40: return newt(a,b,4,c,(unsigned char*)(intptr_t)d);
    case 41: return (int64_t)(intptr_t) t(a)->data;
    default: abort();
    }
}
