// Temporary native integration debt: owned OS resources and GGML callback ABI.
// Expert descriptors, FP8 decode, routing and latent graph policy are Goose.
#include "pack.h"
#include "ggml-cpu.h"
#include "ggml-backend.h"
extern "C" {
#include "runtime.h"
}
#include <array>
#include <vector>
#include <mutex>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static void e0_require(bool ok, const char * msg) { if (!ok) throw std::runtime_error(std::string("Edge0: ")+msg); }
static int64_t eh(const void * p) { return (int64_t)(intptr_t)p; }
static void exact_read(int fd, void * p, size_t n, int64_t off) {
    auto * dst = (uint8_t *)p;
    size_t done = 0;
    while (done < n) {
        ssize_t got = pread(fd, dst+done, n-done, off+done);
        if (got < 0 && errno == EINTR) continue;
        e0_require(got > 0, "short or failed positioned weight read"); done += (size_t)got;
    }
}
struct franken_edge0_pack {
    struct layer_ref { franken_edge0_pack * owner = nullptr; int id = 0; std::array<ggml_tensor *,5> weights{}; };
    std::array<layer_ref,48> layers;
    std::array<int,3> fd{{-1,-1,-1}};
    std::array<std::array<float,1280>,48> inverse;
    std::vector<uint8_t> labels, du, experts, arena;
    std::mutex mutex;
    int threads = 8;
    uint64_t calls = 0, bytes_read = 0;
    ~franken_edge0_pack() { for (int f : fd) if (f >= 0) ::close(f); }
};
std::shared_ptr<franken_edge0_pack> franken_edge0_open(const std::string & path) {
    static std::once_flag init;
    std::call_once(init, []{ goose_init(0,nullptr); });
    auto p = std::make_shared<franken_edge0_pack>();
    const char * names[] = {"experts.bin","latent_dense_q4.bin","ple-fp8.gguf"};
    const int64_t sizes[] = {28626124800LL,1415577600LL,51200246112LL};
    for (int i=0;i<3;++i) {
        p->fd[i] = ::open((path+"/"+names[i]).c_str(), O_RDONLY|O_CLOEXEC);
        e0_require(p->fd[i] >= 0,"required external pack file missing");
        struct stat st{}; e0_require(fstat(p->fd[i],&st)==0 && st.st_size==sizes[i],"external pack size mismatch");
    }
    p->labels.resize(98304);
    int lf = ::open((path+"/labels.bin").c_str(),O_RDONLY|O_CLOEXEC);
    e0_require(lf >= 0,"labels missing");
    try { exact_read(lf,p->labels.data(),p->labels.size(),0); } catch (...) { ::close(lf); throw; } ::close(lf);
    for (int l=0;l<48;++l) {
        p->layers[l].owner=p.get(); p->layers[l].id=l;
        for(int e=0;e<512;++e) e0_require(e0_route_group({p->labels.data(),98304},l,e)>=0,"invalid group label");
        std::array<float,1280> s;
        int sf = ::open((path+"/scales/l"+std::to_string(l)+".f32").c_str(),O_RDONLY|O_CLOEXEC);
        e0_require(sf>=0,"column scales missing");
        try { exact_read(sf,s.data(),5120,0); } catch (...) { ::close(sf); throw; } ::close(sf);
        e0_require(e0_inverse_scales({s.data(),1280},{p->inverse[l].data(),1280})==1280,"invalid column scales");
    }
    p->du.resize(29491200); p->experts.resize(11648000); p->arena.resize(64*1024*1024);
    std::fprintf(stderr,"Edge0 pack open: Q4 D/U, 10 routed + shared, external buffers=%zu bytes, no full PLE/expert mapping\n",p->du.size()+p->experts.size()+p->arena.size());
    return p;
}
void franken_edge0_bind(franken_edge0_pack & p,int l,ggml_tensor * r,ggml_tensor * g,ggml_tensor * u,ggml_tensor * d,ggml_tensor * sr) {
    e0_require(l>=0 && l<48,"layer out of range");
    p.layers[l].weights={{r,g,u,d,sr}};
    for(auto * t:p.layers[l].weights) e0_require(t!=nullptr,"missing dense FFN weight");
}
static ggml_tensor * external(ggml_context * c, ggml_type ty, int64_t n0, int64_t n1, void * data) {
    ggml_set_no_alloc(c,true); auto * t=ggml_new_tensor_2d(c,ty,n0,n1); ggml_set_no_alloc(c,false);
    t->data=data; return t;
}
static ggml_tensor * cpu_weight(ggml_context * c, ggml_tensor * source) {
    if (source->buffer && ggml_backend_buffer_is_host(source->buffer)) return external(c,source->type,source->ne[0],source->ne[1],source->data);
    auto * t=ggml_new_tensor_2d(c,source->type,source->ne[0],source->ne[1]);
    ggml_backend_tensor_get(source,t->data,0,ggml_nbytes(source)); return t;
}
static void compute(ggml_context * c, ggml_tensor * out,int threads) {
    auto * gf=ggml_new_graph_custom(c,4096,false); ggml_build_forward_expand(gf,out);
    e0_require(ggml_graph_compute_with_ctx(c,gf,threads)==GGML_STATUS_SUCCESS,"inner CPU graph execution failed");
}
static void evaluate(franken_edge0_pack::layer_ref & ref,const float * in,float * out) {
    auto & p=*ref.owner; const int l=ref.id;
    ggml_context * c=ggml_init({p.arena.size(),p.arena.data(),false}); e0_require(c!=nullptr,"FFN arena initialization");
    try {
        auto * x=ggml_new_tensor_2d(c,GGML_TYPE_F32,2560,1); std::memcpy(x->data,in,2560*4);
        auto * r=cpu_weight(c,ref.weights[0]); auto * logits=ggml_mul_mat(c,r,x); compute(c,logits,p.threads);
        int64_t ids[10]; float coefficients[10],probs[512];
        e0_require(moe_route({(float*)logits->data,512},10,1,1.0f,{ids,10},{coefficients,10},{probs,512})==10,"Goose top-10 routing failed");
        exact_read(p.fd[1],p.du.data(),p.du.size(),l*(int64_t)p.du.size()); p.bytes_read+=p.du.size();
        auto * inv=external(c,GGML_TYPE_F32,1280,1,p.inverse[l].data());
        int64_t weights[50],coeffs[10],shared[4],work[16],nodes[109];
        for(int k=0;k<10;++k) {
            const int g=(int)e0_route_group({p.labels.data(),98304},l,ids[k]); e0_require(g>=0,"selected expert group");
            int64_t desc[8];
            e0_require(e0_expert_desc(l,ids[k],0,{desc,8})==0,"expert descriptor");
            uint8_t * payload=p.experts.data()+k*1164800;
            exact_read(p.fd[0],payload,1164800,desc[1]); p.bytes_read+=1164800;
            for(int m=0;m<2;++m) {
                e0_require(e0_du_desc(l,g,4,m,{desc,8})==0,"D/U descriptor");
                weights[k*5+m]=eh(external(c,(ggml_type)desc[3],desc[4],desc[5],p.du.data()+desc[1]-l*(int64_t)p.du.size()));
            }
            for(int m=0;m<3;++m) {
                e0_require(e0_expert_desc(l,ids[k],m,{desc,8})==0,"selected expert descriptor");
                weights[k*5+2+m]=eh(external(c,(ggml_type)desc[3],desc[4],desc[5],payload+(m<2?m*352000:704000)));
            }
            coeffs[k]=eh(external(c,GGML_TYPE_F32,1,1,coefficients+k));
        }
        for(int k=0;k<4;++k) shared[k]=eh(cpu_weight(c,ref.weights[k+1]));
        auto result=e0_routed_shared(eh(c),eh(x),eh(inv),{weights,50},{coeffs,10},{shared,4},{work,16},{nodes,109});
        e0_require(result!=0,"Goose routed/shared graph refused inputs");
        auto * y=(ggml_tensor*)(intptr_t)result; compute(c,y,p.threads);
        for(int i=0;i<2560;++i) e0_require(std::isfinite(((float*)y->data)[i]),"nonfinite FFN output");
        std::memcpy(out,y->data,2560*4); ++p.calls;
        if(p.calls==1 || p.calls%48==0) std::fprintf(stderr,"Edge0 FFN progress: calls=%llu completed_tokens=%llu streamed_MiB=%.2f\n",(unsigned long long)p.calls,(unsigned long long)(p.calls/48),p.bytes_read/1048576.0);
    } catch (...) { ggml_free(c); throw; } ggml_free(c);
}
static void custom(ggml_tensor * dst,const ggml_tensor * src,int ith,int nth,void * opaque) {
    (void)nth; if(ith!=0) return;
    auto & ref=*(franken_edge0_pack::layer_ref*)opaque;
    std::lock_guard<std::mutex> guard(ref.owner->mutex);
    try {
        e0_require(src->type==GGML_TYPE_F32 && src->ne[0]==2560 && src->ne[2]==1 && src->ne[3]==1,"unsupported FFN input");
        for(int64_t i=0;i<src->ne[1];++i) evaluate(ref,(const float*)((const char*)src->data+i*src->nb[1]),(float*)((char*)dst->data+i*dst->nb[1]));
    } catch(const std::exception & e) { ggml_abort(__FILE__,__LINE__,"%s",e.what()); }
}
ggml_tensor * franken_edge0_ffn(franken_edge0_pack & p,ggml_context * c,ggml_tensor * x,int l) {
    e0_require(l>=0 && l<48,"FFN layer range");
    return ggml_map_custom1(c,ggml_cont(c,x),custom,1,&p.layers[l]);
}
void franken_edge0_ple(franken_edge0_pack & p,const int32_t * rows,int64_t n,float * out) {
    std::lock_guard<std::mutex> guard(p.mutex);
    e0_require(n>=0 && n%16==0,"PLE row count");
    uint8_t scratch[2560]; int64_t ids[16],desc[8];
    for(int64_t start=0;start<n;start+=16) {
        for(int j=0;j<16;++j) ids[j]=rows[start+j];
        e0_require(e0_ple_read_rows(p.fd[2],{ids,16},e0_ple_supported_scale(),{scratch,2560},{out+start*160,2560},{desc,8})==16,"Goose PLE row provider");
    }
}
