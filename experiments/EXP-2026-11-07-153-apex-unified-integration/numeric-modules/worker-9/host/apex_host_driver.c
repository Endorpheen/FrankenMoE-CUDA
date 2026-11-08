// worker-9 host differential driver.
// Runs the exact Goose-emitted kernel text with a pthread SIMT emulation of the
// permitted device primitives, then compares against independent scalar
// references. No GPU, no model, no weights.
#include "apex_host_runtime.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>

_Thread_local int64_t apex_h_tid_x, apex_h_tid_y, apex_h_tid_z;
_Thread_local int64_t apex_h_bid_x, apex_h_bid_y, apex_h_bid_z;
int64_t apex_h_bdim_x, apex_h_bdim_y, apex_h_bdim_z;
int64_t apex_h_gdim_x, apex_h_gdim_y, apex_h_gdim_z;
pthread_barrier_t apex_h_barrier;
float apex_h_warp[1024];

extern void apex_goose_unary_f32(int64_t, gg_slice, gg_slice, int64_t);
extern void apex_goose_glu_swiglu_f32(gg_slice, gg_slice, gg_slice, int64_t, int64_t, int64_t, int64_t);
extern void apex_goose_bin_bcast_f32(int64_t, gg_slice, gg_slice, gg_slice,
    int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
    int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t);
extern void apex_goose_norm_f32(gg_slice, gg_slice, int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,float,int64_t,gg_slice);
extern void apex_goose_group_norm_f32(gg_slice, gg_slice, int64_t,int64_t,float,int64_t,gg_slice,gg_slice);
extern void apex_goose_rms_norm_f32(gg_slice, gg_slice, int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,float,int64_t,int64_t,int64_t,gg_slice);
extern void apex_goose_l2_norm_f32(gg_slice, gg_slice, int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,float,int64_t,gg_slice);
extern void apex_goose_soft_max_f32(gg_slice, gg_slice, int64_t, gg_slice,
    int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
    float,float,float,float,int64_t,int64_t,gg_slice);
extern void apex_goose_rope_multi_f32(gg_slice, gg_slice, gg_slice,
    int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
    int64_t,int64_t,float,float,float,float,float,float,int64_t,int64_t,int64_t,int64_t,int64_t);
extern void apex_goose_rope_multi_f32_f16(gg_slice, gg_slice, gg_slice,
    int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
    int64_t,int64_t,float,float,float,float,float,float,int64_t,int64_t,int64_t,int64_t,int64_t);
extern void apex_goose_rope_multi_f32_f16(gg_slice, gg_slice, gg_slice,
    int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,
    int64_t,int64_t,float,float,float,float,float,float,int64_t,int64_t,int64_t,int64_t,int64_t);

typedef struct { void * (*body)(void *); void * ctx; int tx,ty,tz,bx,by,bz; } task;
static void * tramp(void * p) {
    task * t = (task *) p;
    apex_h_tid_x = t->tx; apex_h_tid_y = t->ty; apex_h_tid_z = t->tz;
    apex_h_bid_x = t->bx; apex_h_bid_y = t->by; apex_h_bid_z = t->bz;
    t->body(t->ctx);
    return NULL;
}
static void run_launch(int gx,int gy,int gz,int bx,int by,int bz, void * (*body)(void*), void * ctx) {
    const int n = bx*by*bz;
    apex_h_bdim_x=bx; apex_h_bdim_y=by; apex_h_bdim_z=bz;
    apex_h_gdim_x=gx; apex_h_gdim_y=gy; apex_h_gdim_z=gz;
    task * ts = (task *) malloc((size_t) n * sizeof(task));
    pthread_t * th = (pthread_t *) malloc((size_t) n * sizeof(pthread_t));
    for (int bzz=0; bzz<gz; bzz++) for (int byy=0; byy<gy; byy++) for (int bxx=0; bxx<gx; bxx++) {
        pthread_barrier_init(&apex_h_barrier, NULL, (unsigned) n);
        int idx = 0;
        for (int tzz=0; tzz<bz; tzz++) for (int tyy=0; tyy<by; tyy++) for (int txx=0; txx<bx; txx++) {
            ts[idx].body=body; ts[idx].ctx=ctx; ts[idx].tx=txx; ts[idx].ty=tyy; ts[idx].tz=tzz;
            ts[idx].bx=bxx; ts[idx].by=byy; ts[idx].bz=bzz;
            if (pthread_create(&th[idx], NULL, tramp, &ts[idx])) { fprintf(stderr,"pthread_create failed\n"); exit(2); }
            idx++;
        }
        for (int i=0;i<n;i++) pthread_join(th[i], NULL);
        pthread_barrier_destroy(&apex_h_barrier);
    }
    free(ts); free(th);
}

static int g_fail = 0;
static int close_rel(float a, float b, float rtol, float atol) {
    if (isnan(a)||isnan(b)) return 0;
    float d = fabsf(a-b);
    float s = fmaxf(fabsf(a), fabsf(b));
    return d <= atol + rtol*s;
}
static void report(const char * name, int ok) {
    printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) g_fail = 1;
}

// ---------------- unary ----------------
typedef struct { int64_t op,k; gg_slice x,d; } UnaryCtx;
static void * body_unary(void * p) { UnaryCtx * c=(UnaryCtx*)p; apex_goose_unary_f32(c->op,c->x,c->d,c->k); return NULL; }
static float ref_unary(int64_t op, float x) {
    if (op==0) return x/(1.0f+expf(-x));
    if (op==1) return 1.0f/(1.0f+expf(-x));
    return x>20.0f ? x : logf(1.0f+expf(x));
}
static void test_unary(void) {
    const int64_t n = 1000;
    float * x = (float*)malloc(sizeof(float)*n);
    float * d = (float*)malloc(sizeof(float)*n);
    for (int64_t i=0;i<n;i++) x[i] = sinf((float)i*0.013f)*7.5f;
    for (int op=0; op<3; op++) {
        memset(d,0,sizeof(float)*n);
        UnaryCtx c = { op, n, gg_view(x,n), gg_view(d,n) };
        int64_t nb=(n+255)/256; run_launch((int)nb,1,1,256,1,1,body_unary,&c);
        int ok=1; for (int64_t i=0;i<n;i++) { float r=ref_unary(op,x[i]); if (!close_rel(d[i],r,1e-6f,1e-7f)) { ok=0; printf("unary op=%d i=%lld got=%g ref=%g\n",op,(long long)i,d[i],r); break; } }
        char nm[64]; snprintf(nm,sizeof nm,"unary op%d",op); report(nm,ok);
    }
    free(x); free(d);
}

// ---------------- GLU ----------------
typedef struct { gg_slice x,g,d; int64_t k,n,o0,o1; } GluCtx;
static void * body_glu(void * p) { GluCtx * c=(GluCtx*)p; apex_goose_glu_swiglu_f32(c->x,c->g,c->d,c->k,c->n,c->o0,c->o1); return NULL; }
static void test_glu(void) {
    const int64_t n=128, rows=4, k=n*rows;
    float * x=(float*)malloc(sizeof(float)*k*4);
    float * g=(float*)malloc(sizeof(float)*k*4);
    float * d=(float*)malloc(sizeof(float)*k);
    for (int64_t i=0;i<k*4;i++){ x[i]=sinf((float)i*0.017f)*5.0f; g[i]=cosf((float)i*0.011f)*3.0f; }
    for (int sep=0; sep<2; sep++) {
        const int64_t o0 = sep ? 2*n : n;
        const int64_t o1 = sep ? 3*n : n;
        const int64_t xoff = sep ? n : 0;
        const int64_t goff = sep ? 2*n : 0;
        GluCtx c = { gg_view(x+xoff, rows*o0), gg_view(g+goff, rows*o1), gg_view(d,k), k,n,o0,o1 };
        int64_t nb=(k+255)/256; run_launch((int)nb,1,1,256,1,1,body_glu,&c);
        int ok=1;
        for (int64_t i=0;i<k;i++) { int64_t r=i/n; float xv=x[xoff+r*o0+(i%n)]; float gv=g[goff+r*o1+(i%n)]; float rv=(xv/(1.0f+expf(-xv)))*gv; if(!close_rel(d[i],rv,1e-6f,1e-7f)){ok=0;break;} }
        char nm[64]; snprintf(nm,sizeof nm,"glu sep=%d",sep); report(nm,ok);
    }
    free(x);free(g);free(d);
}

// ---------------- bin_bcast ----------------
typedef struct { int64_t op; gg_slice s0,s1,d;
    int64_t ne0,ne1,ne2,ne3,ne10,ne11,ne12,ne13,d1,d2,d3,s00,s01,s02,s03,s10,s11,s12,s13; } BinCtx;
static void * body_bin(void * p){ BinCtx*c=(BinCtx*)p; apex_goose_bin_bcast_f32(c->op,c->s0,c->s1,c->d,c->ne0,c->ne1,c->ne2,c->ne3,c->ne10,c->ne11,c->ne12,c->ne13,c->d1,c->d2,c->d3,c->s00,c->s01,c->s02,c->s03,c->s10,c->s11,c->s12,c->s13); return NULL; }
static float ref_bin(int op,float a,float b){ if(op==0)return a+b; if(op==1)return a-b; if(op==2)return a*b; return a/b; }
static void test_bin(void){
    struct { int64_t ne0,ne1,ne2,ne3, ne10,ne11,ne12,ne13; } shapes[] = {
        {128,4,2,1, 128,4,2,1},
        {128,4,2,1, 1,4,2,1},
        {128,4,2,1, 128,1,1,1},
        {64,3,1,1, 64,1,1,1},
        {7,5,1,1, 7,5,1,1},
    };
    for (unsigned si=0; si<sizeof(shapes)/sizeof(shapes[0]); si++) {
        int64_t ne0=shapes[si].ne0,ne1=shapes[si].ne1,ne2=shapes[si].ne2,ne3=shapes[si].ne3;
        int64_t ne10=shapes[si].ne10,ne11=shapes[si].ne11,ne12=shapes[si].ne12,ne13=shapes[si].ne13;
        int64_t s00=1,s01=ne0,s02=ne0*ne1,s03=ne0*ne1*ne2;
        int64_t d1=ne0,d2=ne0*ne1,d3=ne0*ne1*ne2;
        int64_t s10=1,s11=ne10,s12=ne10*ne11,s13=ne10*ne11*ne12;
        int64_t n0=ne0*ne1*ne2*ne3, n1=ne10*ne11*ne12*ne13;
        float * a=(float*)malloc(sizeof(float)*n0);
        float * b=(float*)malloc(sizeof(float)*n1);
        float * d=(float*)malloc(sizeof(float)*n0);
        for (int64_t i=0;i<n0;i++) a[i]=sinf((float)i*0.007f)*2.0f;
        for (int64_t i=0;i<n1;i++) b[i]=cosf((float)i*0.019f)+2.5f;
        for (int op=0; op<3; op++) {
            BinCtx c = { op, gg_view(a,n0), gg_view(b,n1), gg_view(d,n0), ne0,ne1,ne2,ne3,ne10,ne11,ne12,ne13,d1,d2,d3,s00,s01,s02,s03,s10,s11,s12,s13 };
            int64_t hne0=ne0/2; if(hne0<1)hne0=1;
            int bdx=(int)(hne0<128?hne0:128); int bdy=(int)(ne1<128/bdx?ne1:128/bdx);
            unsigned lim=128/(unsigned)bdx/(unsigned)bdy; if(lim>64)lim=64; unsigned v=(unsigned)(ne2*ne3); int bdz=(int)(v<lim?v:lim);
            int gx=(int)((hne0+bdx-1)/bdx), gy=(int)((ne1+bdy-1)/bdy), gz=(int)((ne2*ne3+bdz-1)/bdz);
            run_launch(gx,gy,gz,bdx,bdy,bdz,body_bin,&c);
            int ok=1;
            for (int64_t i3=0;i3<ne3;i3++) for (int64_t i2=0;i2<ne2;i2++) for (int64_t i1=0;i1<ne1;i1++) for (int64_t i0=0;i0<ne0;i0++) {
                int64_t ia=i3*s03+i2*s02+i1*s01+i0*s00;
                int64_t ib=(i3%ne13)*s13+(i2%ne12)*s12+(i1%ne11)*s11+(i0%ne10)*s10;
                int64_t id=i3*d3+i2*d2+i1*d1+i0;
                float r=ref_bin(op,a[ia],b[ib]);
                if (!close_rel(d[id],r,1e-6f,1e-7f)) { ok=0; printf("bin shape=%u op=%d idx=%lld got=%g ref=%g\n",si,op,(long long)id,d[id],r); break; }
            }
            char nm[64]; snprintf(nm,sizeof nm,"bin shape%u op%d",si,op); report(nm,ok);
        }
        free(a);free(b);free(d);
    }
}


// ---------------- norm family ----------------
typedef struct { gg_slice x,d,scratch; int64_t ncols,nch,ns,srow,sch,ssamp,block,vec,vpt; float eps; } NormCtx;
static void * body_norm(void * p){ NormCtx*c=(NormCtx*)p; apex_goose_norm_f32(c->x,c->d,c->ncols,c->nch,c->ns,c->srow,c->sch,c->ssamp,c->eps,c->block,c->scratch); return NULL; }
static void * body_rms(void * p){ NormCtx*c=(NormCtx*)p; apex_goose_rms_norm_f32(c->x,c->d,c->ncols,c->nch,c->ns,c->srow,c->sch,c->ssamp,c->eps,c->vec,c->block,c->vpt,c->scratch); return NULL; }
static void * body_l2(void * p){ NormCtx*c=(NormCtx*)p; apex_goose_l2_norm_f32(c->x,c->d,c->ncols,c->nch,c->ns,c->srow,c->sch,c->ssamp,c->eps,c->block,c->scratch); return NULL; }
static void launch_normlike(NormCtx*c,int kind){
    int64_t bs=c->block;
    int64_t nb=c->nch<65535?c->nch:65535, nsam=c->ns<65535?c->ns:65535;
    void*(*body)(void*) = kind==0?body_norm:(kind==1?body_rms:body_l2);
    run_launch((int)c->ncols*0 + (int)0,1,1,1,1,1,body,c); // placeholder replaced below
    (void)bs;(void)nb;(void)nsam;
}
static void test_norm_family(void){
    struct { int64_t ncols,nrows,nch,ns; int kind; } cases[] = {
        {128,4,2,1,0},{1024,3,1,1,0},{2048,2,2,1,0},
        {1,2,1,1,0},{17,2,1,1,0},
        {512,3,1,1,1},{4096,2,1,1,1},{1000,2,1,1,1},{1001,2,1,1,1},
        {1,2,1,1,1},{17,2,1,1,1},
        {512,3,1,1,2},{2048,2,1,1,2},
    };
    float * scratch=(float*)malloc(sizeof(float)*64);
    for (unsigned ci=0; ci<sizeof(cases)/sizeof(cases[0]); ci++) {
        int64_t ncols=cases[ci].ncols, nrows=cases[ci].nrows, nch=cases[ci].nch, ns=cases[ci].ns;
        int kind=cases[ci].kind;
        int64_t total=ncols*nrows*nch*ns;
        float*x=(float*)malloc(sizeof(float)*total);
        float*d=(float*)malloc(sizeof(float)*total);
        for(int64_t i=0;i<total;i++) x[i]=sinf((float)i*0.0031f)*1.7f+0.2f;
        memset(d,0,sizeof(float)*total);
        int64_t srow=ncols, sch=ncols*nrows, ssamp=ncols*nrows*nch;
        int64_t bs; int64_t vec=0,vpt=1;
        if (kind==1) {
            vec = (ncols%4==0);
            int64_t n4=ncols/4;
            if (vec) { if(n4<=32)bs=32; else if(n4<=128)bs=128; else if(n4<=256)bs=256; else if(n4<=512)bs=512; else if(n4<=1024)bs=1024; else if(n4<=4096){bs=1024;vpt=4;} else {vec=0;bs=1024;} }
            else bs = ncols<1024?256:1024;
        } else bs = ncols<1024?32:1024;
        NormCtx c={gg_view(x,total),gg_view(d,total),gg_view(scratch,32),ncols,nch,ns,srow,sch,ssamp,bs,vec,vpt,1e-5f};
        void*(*body)(void*)= kind==0?body_norm:(kind==1?body_rms:body_l2);
        run_launch((int)nrows,(int)(nch<65535?nch:65535),(int)(ns<65535?ns:65535),(int)bs,1,1,body,&c);
        int ok=1;
        for(int64_t ss=0;ss<ns;ss++)for(int64_t ch=0;ch<nch;ch++)for(int64_t r=0;r<nrows;r++){
            const float* xr=x+ss*ssamp+ch*sch+r*srow;
            float* dr=d+((ss*nch+ch)*nrows+r)*ncols;
            if(kind==0){
                double sum=0,sq=0; for(int64_t i=0;i<ncols;i++){double v=xr[i];sum+=v;sq+=v*v;}
                double mean=sum/ncols, var=sq/ncols-mean*mean; float inv=(float)(1.0/sqrt(var+(double)1e-5));
                for(int64_t i=0;i<ncols;i++){float rv=(float)((xr[i]-mean)*inv); if(!close_rel(dr[i],rv,2e-4f,2e-5f)){ok=0;printf("norm mismatch ci=%u r=%lld i=%lld got=%g ref=%g\n",ci,(long long)r,(long long)i,dr[i],rv);break;}}
            } else if(kind==1){
                double sq=0; for(int64_t i=0;i<ncols;i++){double v=xr[i];sq+=v*v;}
                float inv=(float)(1.0/sqrt(sq/ncols+(double)1e-5));
                for(int64_t i=0;i<ncols;i++){float rv=inv*xr[i]; if(!close_rel(dr[i],rv,2e-4f,2e-5f)){ok=0;printf("rms mismatch ci=%u r=%lld i=%lld got=%g ref=%g\n",ci,(long long)r,(long long)i,dr[i],rv);break;}}
            } else {
                double sq=0; for(int64_t i=0;i<ncols;i++){double v=xr[i];sq+=v*v;}
                float scale=(float)(1.0/sqrt(fmax(sq,(double)1e-5*(double)1e-5)));
                for(int64_t i=0;i<ncols;i++){float rv=scale*xr[i]; if(!close_rel(dr[i],rv,2e-4f,2e-5f)){ok=0;printf("l2 mismatch ci=%u r=%lld i=%lld got=%g ref=%g\n",ci,(long long)r,(long long)i,dr[i],rv);break;}}
            }
            if(!ok)break;
        }
        char nm[96]; snprintf(nm,sizeof nm,"%s ncols=%lld",kind==0?"norm":(kind==1?"rms":"l2"),(long long)ncols); report(nm,ok);
        free(x);free(d);
    }
    free(scratch);
}

// ---------------- group norm ----------------
typedef struct { gg_slice x,d,s0,s1; int64_t gs,ne,bs; float eps; } GrpCtx;
static void * body_grp(void * p){ GrpCtx*c=(GrpCtx*)p; apex_goose_group_norm_f32(c->x,c->d,c->gs,c->ne,c->eps,c->bs,c->s0,c->s1); return NULL; }
static void test_group_norm(void){
    int64_t ne0=32,ne1=2,ne2=4,ne3=1, ng=2;
    int64_t gs=ne0*ne1*((ne2+ng-1)/ng);
    int64_t ne=ne0*ne1*ne2*ne3;
    float*x=(float*)malloc(sizeof(float)*ne); float*d=(float*)malloc(sizeof(float)*ne);
    float*sa=(float*)malloc(sizeof(float)*64); float*sb=(float*)malloc(sizeof(float)*64);
    for(int64_t i=0;i<ne;i++)x[i]=cosf((float)i*0.021f)*2.0f+0.3f;
    int64_t bs= gs<1024?32:1024;
    GrpCtx c={gg_view(x,ne),gg_view(d,ne),gg_view(sa,32),gg_view(sb,32),gs,ne,bs,1e-5f};
    run_launch((int)(ng*ne3),1,1,(int)bs,1,1,body_grp,&c);
    int ok=1;
    for(int64_t g=0; g<ng*ne3; g++){
        int64_t start=g*gs, end=start+gs; if(end>ne)end=ne;
        double sum=0; for(int64_t j=start;j<end;j++)sum+=x[j];
        double mean=sum/gs; double sq=0; for(int64_t j=start;j<end;j++){double v=x[j]-mean;sq+=v*v;}
        float scale=(float)(1.0/sqrt(sq/gs+(double)1e-5));
        for(int64_t j=start;j<end;j++){float rv=(float)((x[j]-mean)*scale); if(!close_rel(d[j],rv,3e-5f,2e-6f)){ok=0;printf("group mismatch g=%lld j=%lld got=%g ref=%g\n",(long long)g,(long long)j,d[j],rv);break;}}
        if(!ok)break;
    }
    report("group_norm",ok); free(x);free(d);free(sa);free(sb);
}

// ---------------- softmax ----------------
typedef struct { gg_slice x,m,d,scratch; int64_t mkind,ncols,ne01,ne02,ne03,ne12,ne13,mb11,mb12,mb13; float scale,max_bias,m0,m1; int64_t nhl,bs; } SmCtx;
static void * body_sm(void * p){ SmCtx*c=(SmCtx*)p; apex_goose_soft_max_f32(c->x,c->m,c->mkind,c->d,c->ncols,c->ne01,c->ne02,c->ne03,c->ne12,c->ne13,c->mb11,c->mb12,c->mb13,c->scale,c->max_bias,c->m0,c->m1,c->nhl,c->bs,c->scratch); return NULL; }
static void test_softmax(void){
    struct { int64_t ncols, ne01,ne02,ne03; int mkind; float scale,max_bias; } cases[] = {
        {64,2,2,1,1,0.5f,0.0f},
        {2048,1,1,1,0,1.0f,0.0f},
        {100,1,2,1,0,0.7f,0.0f},
        {1,1,1,1,1,1.0f,0.0f},
        {17,2,1,1,0,1.0f,0.0f},
        {17,1,1,1,1,0.5f,0.0f},
    };
    for(unsigned ci=0; ci<sizeof(cases)/sizeof(cases[0]); ci++){
        int64_t ncols=cases[ci].ncols, ne01=cases[ci].ne01, ne02=cases[ci].ne02, ne03=cases[ci].ne03;
        int mkind=cases[ci].mkind; float scale=cases[ci].scale, maxb=cases[ci].max_bias;
        int64_t nrows=ne01*ne02*ne03, total=nrows*ncols;
        float*x=(float*)malloc(sizeof(float)*total); float*d=(float*)malloc(sizeof(float)*total);
        uint8_t*mb=(uint8_t*)malloc(sizeof(uint8_t)*total*(mkind==0?2:4));
        for(int64_t i=0;i<total;i++) x[i]=sinf((float)i*0.017f)*3.0f;
        if(mkind==0){ uint16_t*h=(uint16_t*)mb; for(int64_t i=0;i<total;i++) h[i]=apex_f32_to_f16(cosf((float)i*0.013f)*2.0f); }
        else { float*f=(float*)mb; for(int64_t i=0;i<total;i++) f[i]=cosf((float)i*0.013f)*2.0f; }
        float*scratch=(float*)malloc(sizeof(float)*64);
        int64_t bs=32; while(bs<ncols&&bs<1024)bs*=2;
        uint32_t nhl=1u<<(uint32_t)floorf(log2f((float)ne02));
        float m0=powf(2.0f,-maxb/(float)nhl), m1=powf(2.0f,-(maxb/2.0f)/(float)nhl);
        SmCtx c={gg_view(x,total),gg_view(mb,total*(mkind==0?2:4)),gg_view(d,total),gg_view(scratch,32),mkind,ncols,ne01,ne02,ne03,ne02,ne03,ncols,ncols*ne01,ncols*ne01*ne02,scale,maxb,m0,m1,(int64_t)nhl,bs};
        run_launch((int)ne01,(int)ne02,(int)ne03,(int)bs,1,1,body_sm,&c);
        int ok=1;
        for(int64_t i3=0;i3<ne03;i3++)for(int64_t i2=0;i2<ne02;i2++)for(int64_t i1=0;i1<ne01;i1++){
            int64_t rowx=i1+i2*ne01+i3*ne01*ne02; const float*xr=x+rowx*ncols; float*dr=d+rowx*ncols;
            int64_t mbase=i1*c.mb11+(i2%c.ne12)*c.mb12+(i3%c.ne13)*c.mb13;
            double mx=-1e30;
            for(int64_t j=0;j<ncols;j++){ double mv; if(mkind==0){ uint16_t hh=((uint16_t*)mb)[mbase+j]; mv=apex_f16_to_f32(hh);} else mv=((float*)mb)[mbase+j]; double v=(double)xr[j]*scale+mv; if(v>mx)mx=v; }
            double sum=0;
            for(int64_t j=0;j<ncols;j++){ double mv; if(mkind==0){uint16_t hh=((uint16_t*)mb)[mbase+j]; mv=apex_f16_to_f32(hh);} else mv=((float*)mb)[mbase+j]; double v=(double)xr[j]*scale+mv; sum+=exp(v-mx); }
            for(int64_t j=0;j<ncols;j++){ double mv; if(mkind==0){uint16_t hh=((uint16_t*)mb)[mbase+j]; mv=apex_f16_to_f32(hh);} else mv=((float*)mb)[mbase+j]; double v=(double)xr[j]*scale+mv; float rv=(float)(exp(v-mx)/sum); if(!close_rel(dr[j],rv,3e-5f,2e-6f)){ok=0;printf("softmax ci=%u row=%lld j=%lld got=%g ref=%g\n",ci,(long long)rowx,(long long)j,dr[j],rv);break;} }
            if(!ok)break;
        }
        char nm[96]; snprintf(nm,sizeof nm,"softmax ncols=%lld mask=%s",(long long)ncols,mkind==0?"f16":"f32"); report(nm,ok);
        free(x);free(d);free(mb);free(scratch);
    }
}

// ---------------- rope ----------------
static float ref_yarn_ramp(float low,float high,int64_t i0){ float y=((float)(i0/2)-low)/fmaxf(0.001f,high-low); return 1.0f-fminf(1.0f,fmaxf(0.0f,y)); }
static void ref_yarn(float theta_extrap,float freq_scale,float corr0,float corr1,int64_t i0,float ext,float amp,float*ct,float*st){
    float ti=freq_scale*theta_extrap, th=ti, ms=amp;
    if(ext!=0.0f){ float rm=ref_yarn_ramp(corr0,corr1,i0)*ext; th=ti*(1.0f-rm)+theta_extrap*rm; ms*=1.0f+0.1f*logf(1.0f/freq_scale); }
    *ct=cosf(th)*ms; *st=sinf(th)*ms;
}
typedef struct { gg_slice x,d,p; int64_t ne00,ne01,ne02,s01,s02,s03,d1,d2,d3,nd,no;
                 float fs,ef,af,c0,c1,ts; int64_t sec0,sec1,sec2,sec3,im; } RopeCtx;
static void * body_rope(void * pp){ RopeCtx*c=(RopeCtx*)pp;
    apex_goose_rope_multi_f32(c->x,c->d,c->p,c->ne00,c->ne01,c->ne02,c->s01,c->s02,c->s03,c->d1,c->d2,c->d3,
        c->nd,c->no,c->fs,c->ef,c->af,c->c0,c->c1,c->ts,c->sec0,c->sec1,c->sec2,c->sec3,c->im); return NULL; }
static void * body_rope16(void * pp){ RopeCtx*c=(RopeCtx*)pp;
    apex_goose_rope_multi_f32_f16(c->x,c->d,c->p,c->ne00,c->ne01,c->ne02,c->s01,c->s02,c->s03,c->d1,c->d2,c->d3,
        c->nd,c->no,c->fs,c->ef,c->af,c->c0,c->c1,c->ts,c->sec0,c->sec1,c->sec2,c->sec3,c->im); return NULL; }
static void test_rope(void){
    const int64_t ne00=128, ne01=2, ne02=3, ne03=1, nr=ne01*ne02*ne03;
    const int64_t n_dims=128, n_offs=0;
    const int sec[4]={16,24,24,24};
    const float freq_base=10000.0f, freq_scale=1.0f, ext=0.0f, attn=1.0f;
    const int nctx=4096;
    const float theta_scale=powf(freq_base,-2.0f/(float)n_dims);
    const float pi=3.14159265358979323846f;
    float sd=(float)n_dims*logf((float)nctx/(2.0f*pi))/(2.0f*logf(freq_base));
    float ed=sd; // beta_fast == beta_slow == 0 in this corpus
    float corr0=floorf(sd); if(corr0<0)corr0=0;
    float corr1=ceilf(ed); if(corr1>(float)(n_dims-1))corr1=(float)(n_dims-1);
    const int64_t total=ne00*ne01*ne02*ne03;
    float*x=(float*)malloc(sizeof(float)*total); float*d=(float*)malloc(sizeof(float)*total);
    for(int64_t i=0;i<total;i++) x[i]=sinf((float)i*0.019f);
    int32_t*pos=(int32_t*)malloc(sizeof(int32_t)*16*ne02);
    for(int64_t i=0;i<16*ne02;i++) pos[i]=(int32_t)(i*7+3);
    const int64_t s01=ne00,s02=ne00*ne01,s03=ne00*ne01*ne02;
    RopeCtx c={gg_view(x,total),gg_view(d,total),gg_view(pos,16*ne02*4),ne00,ne01,ne02,s01,s02,s03,s01,s02,s03,n_dims,n_offs,
               freq_scale,ext,attn,corr0,corr1,theta_scale,sec[0],sec[1],sec[2],sec[3],1};
    const int64_t nbx=(ne00+2*256-1)/(2*256);
    run_launch((int)nr,(int)nbx,1,1,256,1,body_rope,&c);
    int ok=1;
    const int64_t sect_dims=sec[0]+sec[1]+sec[2]+sec[3];
    const int64_t sec_w=sec[1]+sec[0];
    for(int64_t i3=0;i3<ne03&&ok;i3++)for(int64_t i2=0;i2<ne02&&ok;i2++)for(int64_t i1=0;i1<ne01&&ok;i1++)
      for(int64_t i0=0;i0<ne00;i0+=2){
        int64_t iw=i0-n_offs;
        int64_t idst=i0/2+i1*s01+i2*s02+i3*s03;
        if(i0<n_offs||i0>=n_offs+n_dims){
            if(!close_rel(d[idst],x[idst],1e-6f,1e-7f)||!close_rel(d[idst+1],x[idst+1],1e-6f,1e-7f)){ok=0;}
            continue;
        }
        int64_t sector=(iw/2)%sect_dims;
        float theta_base=0.0f;
        if(sector%3==1&&sector<3*sec[1]) theta_base=(float)pos[i2+ne02]*(float)powf(theta_scale,(float)iw/2.0f);
        else if(sector%3==2&&sector<3*sec[2]) theta_base=(float)pos[i2+ne02*2]*(float)powf(theta_scale,(float)iw/2.0f);
        else if(sector%3==0&&sector<3*sec[0]) theta_base=(float)pos[i2]*(float)powf(theta_scale,(float)iw/2.0f);
        else theta_base=(float)pos[i2+ne02*3]*(float)powf(theta_scale,(float)iw/2.0f);
        (void)sec_w;
        float ct,st; ref_yarn(theta_base,freq_scale,corr0,corr1,iw,ext,attn,&ct,&st);
        float x0=x[idst+n_offs/2], x1=x[idst+n_offs/2+n_dims/2];
        float r0=x0*ct-x1*st, r1=x0*st+x1*ct;
        if(!close_rel(d[idst+n_offs/2],r0,2e-5f,1e-6f)||!close_rel(d[idst+n_offs/2+n_dims/2],r1,2e-5f,1e-6f)){ok=0;printf("rope mismatch i1=%lld i2=%lld i0=%lld got=%g,%g ref=%g,%g\n",(long long)i1,(long long)i2,(long long)i0,d[idst+n_offs/2],d[idst+n_offs/2+n_dims/2],r0,r1);}
      }
    report("rope_multi_f32 imrope",ok);
    uint16_t * d16 = (uint16_t*)calloc((size_t)total, sizeof(uint16_t));
    RopeCtx c16 = c; c16.d = gg_view(d16, total*2);
    run_launch((int)nr,(int)nbx,1,1,256,1,body_rope16,&c16);
    int ok16=1;
    for(int64_t i=0;i<total;i++){
        float hv=apex_f16_to_f32(d16[i]);
        if(!close_rel(hv,d[i],3e-3f,3e-4f)){ok16=0;printf("rope f16 mismatch i=%lld got=%g f32=%g\n",(long long)i,hv,d[i]);break;}
    }
    report("rope_multi_f32_f16 imrope",ok16);
    free(d16);free(x);free(d);free(pos);
}

int main(void){
    setvbuf(stdout, NULL, _IONBF, 0);
    test_unary();
    test_glu();
    test_bin();
    test_norm_family();
    test_group_norm();
    test_softmax();
    test_rope();
    printf(g_fail?"HOST_DIFFERENTIAL_FAIL\n":"HOST_DIFFERENTIAL_PASS\n");
    return g_fail;
}
