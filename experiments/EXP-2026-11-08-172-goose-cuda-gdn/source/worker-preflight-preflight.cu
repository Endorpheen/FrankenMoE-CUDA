// GDN before-stream preflight fixture (fg174s scalar gated-delta-net trial).
// Target: the current public entries ggml_cuda_op_gated_delta_net(ctx,dst) and
// ggml_cuda_op_gated_delta_net_fused_cache(ctx,dst,cache) exactly as declared in the
// production header gated_delta_net.cuh. Every case builds the valid baseline shape
// (S_v=32, H=2, n_tokens=3, n_seqs=2, neq1=1, K=2; dst packed flat dims [S_v,H,nt+K*S_v,N]
// = [32,2,67,2], one contiguous span covering the attention output plus both state slots,
// 384+2*4096=8576 floats) and then mutates exactly one field so a single provided host
// fail guard rejects the call. Tensor data are fake, non-null, mutually disjoint
// device-looking addresses; they are never dereferenced because no executed case passes
// the guards, and the valid baseline itself is never invoked (no valid fake-pointer case
// is ever launched). Each case runs in a forked child; the callback installed with
// ggml_set_abort_callback exits 81 only when ctx.streams[0][0] is still nullptr (the
// reject happened before any stream or kernel effect) and 82 otherwise; the parent waits
// and requires exit status 81 for every case. The fixture never calls ctx.stream()
// itself, never exercises the rollback/native path or old native invalid sets, performs
// no timings and edits no production or compiler sources.
#include "gated_delta_net.cuh"
#include <sys/wait.h>
#include <unistd.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

static ggml_backend_cuda_context* active;
static void reject(const char*){ _exit(active->streams[0][0]==nullptr?81:82); }

// Fake tensor metadata: logical dims, standard contiguous byte strides, fake address.
static void shape(ggml_tensor& t, ggml_type type, int64_t n0, int64_t n1, int64_t n2, int64_t n3, uint64_t addr){
    memset(&t,0,sizeof t);
    t.type=type; t.op=GGML_OP_GATED_DELTA_NET;
    t.ne[0]=n0; t.ne[1]=n1; t.ne[2]=n2; t.ne[3]=n3;
    t.nb[0]=ggml_type_size(type); t.nb[1]=t.nb[0]*n0; t.nb[2]=t.nb[1]*n1; t.nb[3]=t.nb[2]*n2;
    t.data=(void*)(uintptr_t)addr;
}

static void link(ggml_tensor& d, ggml_tensor* s){
    for(int i=0;i<6;i++) d.src[i]=&s[i];
    d.op_params[0]=2; // K=2 snapshot slots (keep_rs path)
}

int main(){
    for(int c=0;c<32;c++){
        pid_t pid=fork();
        if(pid<0) return 2;
        if(pid==0){
            ggml_backend_cuda_context ctx(0);
            active=&ctx;
            ggml_set_abort_callback(reject);
            ggml_tensor s[6], d;
            shape(s[0],GGML_TYPE_F32,32,1,3,1,0x1000000);  // q  [S_v,neq1,nt,neq3]
            shape(s[1],GGML_TYPE_F32,32,1,3,1,0x2000000);  // k
            shape(s[2],GGML_TYPE_F32,32,2,3,2,0x3000000);  // v  [S_v,H,nt,N]
            shape(s[3],GGML_TYPE_F32, 1,2,3,2,0x4000000);  // g  per-head scalar gate
            shape(s[4],GGML_TYPE_F32, 1,2,3,2,0x5000000);  // beta
            shape(s[5],GGML_TYPE_F32,32,32,2,2,0x6000000); // recurrent state [S_v,S_v,H,N]
            shape(d,   GGML_TYPE_F32,32,2,67,2,0x7000000); // dst [S_v,H,nt+K*S_v,N] packed
            link(d,s);
            ggml_tensor* dst=&d;
            bool fused=false; uint64_t cdata=0; int64_t cslot=0;
            switch(c){
            case 0: dst=nullptr; break;                                       // guard 3
            case 1: d.src[0]=nullptr; break;                                  // guard 7
            case 2: d.src[5]=nullptr; break;                                  // guard 7
            case 3: d.type=GGML_TYPE_I32; break;                              // guard 5
            case 4: s[0].type=GGML_TYPE_I32; break;                           // guard 5
            case 5: s[0].data=nullptr; break;                                 // guard 5
            case 6: s[0].nb[0]=8; break;                                      // guard 5 (nb0!=4)
            case 7: s[3].nb[0]=8; break;                                      // guard 5 (nb0!=4)
            case 8: s[0].ne[1]=-1; break;                                     // guard 6 (negative dim)
            case 9: s[2].ne[1]=0; break;                                      // guard 6 (zero dim)
            case 10: s[0].ne[2]=3000000000LL; break;                          // guard 6 (dim overflow)
            case 11: s[4].nb[1]=(size_t)-8; break;                            // guard 6 (negative stride)
            case 12: shape(s[0],GGML_TYPE_F32,31,1,3,1,0x1000000);            // guard 22 (head dim)
                     shape(s[1],GGML_TYPE_F32,31,1,3,1,0x2000000); break;
            case 13: shape(s[2],GGML_TYPE_F32,32,2,3,70000,0x3000000);        // guard 16 (n_seqs cap)
                     shape(s[5],GGML_TYPE_F32,32,32,2,70000,0x6000000); break;
            case 14: shape(s[0],GGML_TYPE_F32,32,1,2,1,0x1000000); break;     // guard 22 (q tokens<nt)
            case 15: shape(s[0],GGML_TYPE_F32,32,2,3,1,0x1000000); break;     // guard 8 (neq1!=nek1)
            case 16: shape(s[0],GGML_TYPE_F32,32,1,3,4,0x1000000); break;     // guard 9 (seq divisibility)
            case 17: shape(s[5],GGML_TYPE_F32,32,32,1,2,0x6000000); break;    // guard 14 (state dims)
            case 18: shape(s[3],GGML_TYPE_F32,16,2,3,2,0x4000000); break;     // guard 12 (wrong g type)
            case 19: s[3].nb[3]=16; break;                                    // guard 13 (g contiguity)
            case 20: s[4].nb[2]=16; break;                                    // guard 13 (beta contiguity)
            case 21: s[5].nb[2]=2048; break;                                  // guard 13 (state contiguity)
            case 22: shape(s[2],GGML_TYPE_F32,48,2,3,2,0x3000000);            // guard 20 (S_v=48)
                     shape(s[0],GGML_TYPE_F32,48,1,3,1,0x1000000);
                     shape(s[1],GGML_TYPE_F32,48,1,3,1,0x2000000);
                     shape(s[5],GGML_TYPE_F32,48,48,2,2,0x6000000); break;
            case 23: shape(d,GGML_TYPE_F32,32,2,3,2,0x7000000); link(d,s); break; // guard 17 (dst span)
            case 24: s[5].data=(void*)(uintptr_t)(UINT64_MAX-4096); break;    // guard 18 (source wrap)
            case 25: s[0].nb[2]=384; break;                                   // guard 11 (q/k strides)
            case 26: s[5].data=(void*)(uintptr_t)(0x7000000+384*4+4); break;  // guard 25 (shifted state)
            case 27: d.data=s[0].data; break;                                 // guard 25 (in/out alias)
            case 28: fused=true; break;                                       // guard 3 (cache data null)
            case 29: fused=true; cdata=0xA000000; cslot=-1; break;            // guard 23 (negative slot)
            case 30: fused=true; cdata=0xA000000; cslot=2048; break;          // guard 23 (slot<K slots)
            case 31: fused=true; cdata=UINT64_MAX-100; cslot=4096; break;     // guard 24 (cache wrap)
            }
            if(fused){
                ggml_cuda_op_gated_delta_net_fused_cache(ctx,dst,ggml_cuda_gated_delta_net_fused_cache{(float*)(uintptr_t)cdata,cslot});
            }else{
                ggml_cuda_op_gated_delta_net(ctx,dst);
            }
            _exit(86); // reached only if the provided guards failed to reject a malformed case
        }
        int st=0;
        if(waitpid(pid,&st,0)!=pid||!WIFEXITED(st)||WEXITSTATUS(st)!=81){
            fprintf(stderr,"FAIL GDN preflight case=%d status=%d\n",c,st);
            return 3;
        }
    }
    puts("PASS GDN preflight cases=32 baseline_not_launched=1 no_stream_or_kernel_before_rejection=1");
    return 0;
}
