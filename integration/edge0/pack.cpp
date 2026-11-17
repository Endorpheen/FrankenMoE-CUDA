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
#include <chrono>
#include <cstdlib>

// Profiling ranges are disjoint host intervals. CPU compute includes GGML's
// internal threadpool/work preparation; CUDA kernel/copy timings come from nsys.
struct e0_metrics {
    double arena=0, router=0, du_io=0, expert_io=0, copy=0, graph=0, cpu=0, ple=0, ffn=0;
    uint64_t calls=0, du_bytes=0, expert_bytes=0, copy_bytes=0, ple_rows=0, expert_hits=0,expert_misses=0,du_hits=0,du_misses=0;
};
static e0_metrics e0_profile[3];
static int e0_phase=0;
static bool e0_profiling() { static bool active=std::getenv("FRANKEN_EDGE0_PROFILE")!=nullptr; return active; }
static double e0_now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
struct e0_span { double & dst; double start; explicit e0_span(double & d):dst(d),start(e0_now()){} ~e0_span(){dst+=e0_now()-start;} };
void franken_edge0_profile_phase(int phase) {
    if (e0_profiling() && phase==3) for(int i=0;i<3;++i) {
        const auto & m=e0_profile[i];
        std::fprintf(stderr,"EDGE0_PROFILE phase=%d ffn_s=%.9f arena_s=%.9f route_s=%.9f du_read_s=%.9f expert_read_s=%.9f weight_copy_s=%.9f graph_s=%.9f cpu_compute_s=%.9f ple_s=%.9f calls=%llu du_bytes=%llu expert_bytes=%llu copy_bytes=%llu ple_rows=%llu expert_hits=%llu expert_misses=%llu du_hits=%llu du_misses=%llu\n",i,m.ffn,m.arena,m.router,m.du_io,m.expert_io,m.copy,m.graph,m.cpu,m.ple,(unsigned long long)m.calls,(unsigned long long)m.du_bytes,(unsigned long long)m.expert_bytes,(unsigned long long)m.copy_bytes,(unsigned long long)m.ple_rows,(unsigned long long)m.expert_hits,(unsigned long long)m.expert_misses,(unsigned long long)m.du_hits,(unsigned long long)m.du_misses);
    }
    e0_phase=phase;
}
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
    std::vector<uint8_t> labels, du, experts, arena, expert_cache;
    bool du_cached=true;
    int64_t cache_slots=0, next_slot=0;
    std::array<int64_t,192> layout{};
    std::array<int64_t,24576> valid{},spec{},prev{},next{},ever{},slot_of{};
    std::array<int64_t,2> lru{{-1,-1}};
    std::array<int64_t,5> account{};
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
    const char * dc=std::getenv("FRANKEN_EDGE0_DU_CACHE"); p->du_cached=!dc || std::string(dc)!="0";
    int64_t cache_mib=3072;
    if(const char * v=std::getenv("FRANKEN_EDGE0_CACHE_MIB")) cache_mib=std::stoll(v);
    e0_require(cache_mib>=0 && cache_mib<=6144,"expert cache budget must be 0..6144 MiB");
    p->cache_slots=cache_mib*1024*1024/1164800;
    e0_require(!cache_mib || p->cache_slots>=10,"expert cache needs at least ten records");
    p->expert_cache.resize(p->cache_slots*1164800);
    p->slot_of.fill(-1);p->prev.fill(-1);p->next.fill(-1);
    for(int l=0;l<48;++l) {p->layout[l*4]=1;p->layout[l*4+1]=352000;p->layout[l*4+2]=352000;p->layout[l*4+3]=460800;}
    p->du.resize(p->du_cached?1415577600:29491200);
    if(p->du_cached) {
        e0_span timer(e0_profile[0].du_io);exact_read(p->fd[1],p->du.data(),p->du.size(),0);
        e0_profile[0].du_bytes+=p->du.size();++e0_profile[0].du_misses;
    }
    p->experts.resize(11648000); p->arena.resize(64*1024*1024);
    std::fprintf(stderr,"Edge0 cache budget: DU=%zu expert=%zu slots=%lld cache policy=existing Goose expert-lru\n",p->du.size(),p->expert_cache.size(),(long long)p->cache_slots);
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
    { e0_span timer(e0_profile[e0_phase].copy);
      ggml_backend_tensor_get(source,t->data,0,ggml_nbytes(source)); }
    e0_profile[e0_phase].copy_bytes+=ggml_nbytes(source); return t;
}
// The Goose LRU owns admission, promotion and victim choice. Native code only
// maps slots to bounded byte storage; an unsuccessful read publishes nothing.
static uint8_t * expert_payload(franken_edge0_pack & p,int layer,int64_t expert,int rank,int64_t off) {
    auto & m=e0_profile[e0_phase]; const int64_t key=layer*512+expert;
    int64_t resident=0;
    xl_query({p.valid.data(),24576},{p.spec.data(),24576},layer,{&expert,1},1,512,p.cache_slots==0,{&resident,1});
    if(resident) {
        ++m.expert_hits;
        e0_require(xl_touch({p.layout.data(),192},{p.valid.data(),24576},{p.spec.data(),24576},
            {p.prev.data(),24576},{p.next.data(),24576},{p.ever.data(),24576},{p.lru.data(),2},{p.account.data(),5},512,layer,expert,1,-1,1)==1,"LRU hit");
        e0_require(p.slot_of[key]>=0 && p.slot_of[key]<p.cache_slots,"cache slot map hit");
        return p.expert_cache.data()+p.slot_of[key]*1164800;
    }
    ++m.expert_misses;uint8_t * staging=p.experts.data()+rank*1164800;
    {e0_span timer(m.expert_io);exact_read(p.fd[0],staging,1164800,off);}
    m.expert_bytes+=1164800;p.bytes_read+=1164800;
    if(!p.cache_slots)return staging;
    int64_t slot;
    if(p.next_slot<p.cache_slots)slot=p.next_slot++;
    else {
        const int64_t old=xl_evict_tail({p.layout.data(),192},{p.valid.data(),24576},{p.spec.data(),24576},
            {p.prev.data(),24576},{p.next.data(),24576},{p.ever.data(),24576},{p.lru.data(),2},{p.account.data(),5},512);
        e0_require(old>=0 && old<24576,"LRU victim");slot=p.slot_of[old];p.slot_of[old]=-1;
    }
    e0_require(slot>=0 && slot<p.cache_slots,"cache slot map fill");
    uint8_t * payload=p.expert_cache.data()+slot*1164800;std::memcpy(payload,staging,1164800);p.slot_of[key]=slot;
    e0_require(xl_touch({p.layout.data(),192},{p.valid.data(),24576},{p.spec.data(),24576},
        {p.prev.data(),24576},{p.next.data(),24576},{p.ever.data(),24576},{p.lru.data(),2},{p.account.data(),5},512,layer,expert,1,-1,1)==0,"LRU fill");
    return payload;
}
static void compute(ggml_context * c, ggml_tensor * out,int threads) {
    ggml_cgraph * gf;
    { e0_span timer(e0_profile[e0_phase].graph); gf=ggml_new_graph_custom(c,4096,false); ggml_build_forward_expand(gf,out); }
    { e0_span timer(e0_profile[e0_phase].cpu);
      e0_require(ggml_graph_compute_with_ctx(c,gf,threads)==GGML_STATUS_SUCCESS,"inner CPU graph execution failed"); }
}
static void evaluate(franken_edge0_pack::layer_ref & ref,const float * in,float * out) {
    auto & p=*ref.owner; const int l=ref.id;
    e0_span total(e0_profile[e0_phase].ffn); ++e0_profile[e0_phase].calls;
    const double arena_begin=e0_now();
    ggml_context * c=ggml_init({p.arena.size(),p.arena.data(),false}); e0_require(c!=nullptr,"FFN arena initialization");
    try {
        auto * x=ggml_new_tensor_2d(c,GGML_TYPE_F32,2560,1); std::memcpy(x->data,in,2560*4);
        e0_profile[e0_phase].arena+=e0_now()-arena_begin;
        auto * r=cpu_weight(c,ref.weights[0]); auto * logits=ggml_mul_mat(c,r,x); compute(c,logits,p.threads);
        int64_t ids[10]; float coefficients[10],probs[512];
        { e0_span timer(e0_profile[e0_phase].router);
          e0_require(moe_route({(float*)logits->data,512},10,1,1.0f,{ids,10},{coefficients,10},{probs,512})==10,"Goose top-10 routing failed"); }
        if(!p.du_cached) {
            e0_span timer(e0_profile[e0_phase].du_io);exact_read(p.fd[1],p.du.data(),29491200,l*29491200LL);
            e0_profile[e0_phase].du_bytes+=29491200;p.bytes_read+=29491200;++e0_profile[e0_phase].du_misses;
        } else ++e0_profile[e0_phase].du_hits;
        if(p.cache_slots)xl_retain({p.valid.data(),24576},{p.prev.data(),24576},{p.next.data(),24576},
                                  {p.lru.data(),2},l,{ids,10},10,512);
        auto * inv=external(c,GGML_TYPE_F32,1280,1,p.inverse[l].data());
        int64_t weights[50],coeffs[10],shared[4],work[16],nodes[109];
        for(int k=0;k<10;++k) {
            const int g=(int)e0_route_group({p.labels.data(),98304},l,ids[k]); e0_require(g>=0,"selected expert group");
            int64_t desc[8];
            e0_require(e0_expert_desc(l,ids[k],0,{desc,8})==0,"expert descriptor");
            uint8_t * payload=expert_payload(p,l,ids[k],k,desc[1]);
            for(int m=0;m<2;++m) {
                e0_require(e0_du_desc(l,g,4,m,{desc,8})==0,"D/U descriptor");
                weights[k*5+m]=eh(external(c,(ggml_type)desc[3],desc[4],desc[5],p.du.data()+desc[1]-(p.du_cached?0:l*29491200LL)));
            }
            for(int m=0;m<3;++m) {
                e0_require(e0_expert_desc(l,ids[k],m,{desc,8})==0,"selected expert descriptor");
                weights[k*5+2+m]=eh(external(c,(ggml_type)desc[3],desc[4],desc[5],payload+(m<2?m*352000:704000)));
            }
            coeffs[k]=eh(external(c,GGML_TYPE_F32,1,1,coefficients+k));
        }
        for(int k=0;k<4;++k) shared[k]=eh(cpu_weight(c,ref.weights[k+1]));
        int64_t result;
        { e0_span timer(e0_profile[e0_phase].graph);
          result=e0_routed_shared(eh(c),eh(x),eh(inv),{weights,50},{coeffs,10},{shared,4},{work,16},{nodes,109}); }
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
    e0_span timer(e0_profile[e0_phase].ple); e0_profile[e0_phase].ple_rows+=n;
    e0_require(n>=0 && n%16==0,"PLE row count");
    uint8_t scratch[2560]; int64_t ids[16],desc[8];
    for(int64_t start=0;start<n;start+=16) {
        for(int j=0;j<16;++j) ids[j]=rows[start+j];
        e0_require(e0_ple_read_rows(p.fd[2],{ids,16},e0_ple_supported_scale(),{scratch,2560},{out+start*160,2560},{desc,8})==16,"Goose PLE row provider");
    }
}
