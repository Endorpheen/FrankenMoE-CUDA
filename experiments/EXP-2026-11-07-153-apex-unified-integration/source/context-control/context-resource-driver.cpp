// worker-4: executable resource/checkpoint trace driver.
// Links the generated resource module (unit-resource.o) against an ABI shim and
// exercises resource_init/release, sched_reserve and state orchestration.
// No llama_context, no model, no network. UNVERIFIED PROPOSAL for Codex.
#include "context-services.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" int64_t fg107_goose_resource_init(int64_t);
extern "C" int64_t fg107_goose_resource_release(int64_t);
extern "C" int64_t fg107_goose_sched_new(int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_sched_reserve(int64_t);
extern "C" int64_t fg107_goose_state_get_size(int64_t);
extern "C" int64_t fg107_goose_state_get_data(int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_state_set_data(int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_state_seq_get_size(int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_state_seq_get_data(int64_t, int64_t, int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_state_seq_set_data(int64_t, int64_t, int64_t, int64_t, int64_t);

using i64 = int64_t;
struct RF {
    i64 n_ctx=256,n_ubatch=4,n_seq_max=1,n_outputs_max=8,n_outputs_max_per_seq=8;
    i64 vocab_only=0, training=0, no_alloc=0, n_devices=1, n_gpu_layers=0, n_layer_all=2;
    i64 split_layer=0, offload_kqv=0, tensor_overrides=0, moe_cache_size=0;
    i64 n_input_tensors=0, graph_has_logits=0, arch=0, flash_attn=1;
    i64 memory=1, sched=0, sched_need=1, pipeline=0, lazy_pp=0;
    i64 backends=0, reserve_ok=1, tokens=0;
    int mode=0; // 0 dummy, 1 host-write, 2 host-read, 3 device
    std::vector<unsigned char> buf; size_t off=0;
    std::vector<std::string> trace;
    void rec(const char*op,i64 a=0,i64 b=0){char t[96];std::snprintf(t,sizeof t,"%s:%lld,%lld",op,(long long)a,(long long)b);trace.push_back(t);} 
    int has(const char*op) const { for (auto &s: trace) if (s.rfind(op,0)==0) return 1; return 0; }
};
static RF & R(i64 h){ return *(RF*)(intptr_t)h; }

extern "C" i64 fg107_ctx(i64 h,i64 f,i64 a){
    auto &x=R(h); (void)a;
    switch(f){case 0:return x.memory;case 1:return x.sched;case 2:return x.n_ctx;case 4:return x.n_ubatch;
    case 5:return x.n_seq_max;case 15:return x.pipeline;case 21:return x.n_outputs_max;case 30:return x.lazy_pp;
    case 34:return 64;case 24:return 0;case 18:return 0;case 10:return 0;case 11:return 0;case 12:return 0;
    case 25:return 0;case 6:return 16;case 7:return 8;case 8:return 8;case 9:return 0;default:return 0;}
}
extern "C" i64 fg107_ctx_set(i64 h,i64 f,i64 v){ (void)h;(void)f;(void)v; return 0; }
extern "C" i64 fg107_rq(i64 h,i64 f,i64 a){
    auto &x=R(h); (void)a;
    switch(f){case 0:return x.vocab_only;case 1:return x.n_devices;case 2:return x.sched_need;case 3:return x.training;
    case 4:return x.no_alloc;case 5:return x.n_devices;case 6:return x.n_gpu_layers;case 7:return x.n_layer_all;
    case 8:return x.split_layer;case 9:return x.offload_kqv;case 10:return x.tensor_overrides;case 11:return x.moe_cache_size;
    case 12:return x.n_input_tensors;case 13:return x.graph_has_logits;case 14:return x.backends;case 15:return x.arch;
    case 16:return 0;case 17:return x.flash_attn;case 18:return 0;case 19:return 0;case 20:return x.n_layer_all;default:return 0;}
}
extern "C" i64 fg107_rq_set(i64 h,i64 f,i64 v){
    auto &x=R(h);
    switch(f){case 0:x.sched_need=v;return 0;case 1:x.graph_has_logits=v;return 0;case 2:x.pipeline=v;return 0;
    case 3:x.n_input_tensors=v;return 0;default:return 0;}
}
extern "C" i64 fg107_r(i64 h,i64 op,i64 a,i64 b,i64 d){
    auto &x=R(h); (void)d;
    switch(op){
    case 0:return 0x10; case 1:return 0x20; case 2:x.backends+=1;return 0; case 3:return 1; case 4:return 0x10;
    case 5:return 0; case 6:x.rec("init_cpu");return 0x30; case 7:return 0; case 8:return 0;
    case 9:x.rec("output_reserve",a);return x.n_outputs_max>=a; case 10:x.memory=1;x.rec("create_memory");return 0x40;
    case 11:x.rec("enumerate");return 0; case 12:x.rec("pipeline");x.pipeline=0;return 0; case 13:return 0;
    case 14:x.rec("flash_check");return 1; case 15:return 0; case 16:return 0; case 17:x.rec("token_ids");return 0;
    case 18:x.rec("res_reserve_new",a);return 0; case 19:x.rec("resolve_fused");return 0;
    case 23:return 1; case 25:return 128; case 26:return 0; case 27:x.sched=1;x.rec("sched_create",a,b);return 0;
    case 28:x.rec("copy_cb");return 0; case 29:x.rec("expert_pipeline",a);return 0; case 30:x.rec("realloc_cb");return 0;
    case 31:return 0x20; case 32:return 0x50; case 33:return 1; case 34:x.rec("opt_free");return 0; case 35:return x.memory;
    case 36:x.rec("buf_check",a);return 0; case 37:return x.backends; case 38:x.rec("log_reserve",a);return 0;
    case 50:x.mode=0;x.buf.clear();x.off=0;return 0;
    case 51:x.mode=1;x.buf.clear();x.off=0;return 0;
    case 52:x.mode=2;x.off=0;return 0;
    case 53:{if(x.mode==3)return -1;uint32_t v=(uint32_t)a;x.buf.insert(x.buf.end(),(unsigned char*)&v,(unsigned char*)&v+4);return 0;}
    case 54:{uint32_t v=0;if(x.mode==3||x.off+4>x.buf.size())return -1;memcpy(&v,&x.buf[x.off],4);x.off+=4;return v;}
    case 55:{if(x.mode==3)return -1;i64 v=a;x.buf.insert(x.buf.end(),(unsigned char*)&v,(unsigned char*)&v+8);return 0;}
    case 56:{i64 v=0;if(x.mode==3||x.off+8>x.buf.size())return -1;memcpy(&v,&x.buf[x.off],8);x.off+=8;return v;}
    case 57:{if(x.mode==3)return -1;static const char s[8]={'S','T','A','T','E','0','0','1'};x.buf.insert(x.buf.end(),s,s+8);return (i64)x.buf.size();}
    case 58:{if(x.mode==3||x.off+8>x.buf.size())return -1;x.off+=8;return (i64)x.off;}
    case 59:{if(x.mode==3)return -1;static const char s[8]={'S','E','Q','S','T','0','0','1'};x.buf.insert(x.buf.end(),s,s+8);return (i64)x.buf.size();}
    case 60:{if(x.mode==3||x.off+8>x.buf.size())return -1;x.off+=8;return (i64)x.off;}
    case 61:return (i64)(x.mode==1?x.buf.size():x.off);
    case 62:x.off=0;return 0;
    case 63:return 0;
    case 64:x.mode=3;return -1; case 65:x.mode=3;return -1;
    case 66:return 1; case 67:return (a&1)!=0;
    default:x.rec("r_bad",op);return 0;}
}
extern "C" i64 fg107_sched(i64 h,i64 op,i64 a,i64 b,i64 d){auto &x=R(h);(void)a;(void)b;(void)d;switch(op){case 3:x.rec("sync");return 0;case 7:x.rec("reserve");return 0;default:return 0;}}
extern "C" i64 fg107_mem(i64 h,i64 op,i64 a,i64 b,i64 d){auto &x=R(h);(void)a;(void)b;(void)d;if(op==1){x.rec("init_full");return 0x40;}if(op==3){x.rec("seq_rm");return 1;}return 0;}
extern "C" i64 fg107_graph_reserve(i64 h,i64 t,i64 s,i64 o,i64 m,i64 l){auto &x=R(h);(void)s;(void)o;(void)m;(void)l;x.rec("graph_reserve",t);return x.reserve_ok?0x60:0;}
extern "C" i64 fg107_res_reset_all_prev(i64 h){(void)h;return 0;}
extern "C" i64 fg107_log(i64 h,i64 site,i64 a,i64 b){R(h).rec("log",site,a);(void)b;return 0;}

int main(){
    bool ok=true;
    { // happy path init + reserve + state round-trip
        RF f; f.vocab_only=0; f.n_devices=1;
        const i64 rc=fg107_goose_resource_init((i64)(intptr_t)&f);
        const bool init_ok = rc==0 && f.backends>=1 && f.sched==1 && f.has("create_memory") && f.has("enumerate") && f.has("sched_create");
        std::printf("%s resource_init rc=%lld backends=%lld sched=%lld\n", init_ok?"PASS":"FAIL",(long long)rc,(long long)f.backends,(long long)f.sched);
        ok = ok && init_ok;

        f.sched_need=1;
        const i64 rr=fg107_goose_sched_reserve((i64)(intptr_t)&f);
        const bool reserve_ok = rr==0 && f.has("res_reserve_new") && f.has("graph_reserve");
        std::printf("%s sched_reserve rc=%lld\n", reserve_ok?"PASS":"FAIL",(long long)rr);
        ok = ok && reserve_ok;

        const i64 sz=fg107_goose_state_get_size((i64)(intptr_t)&f);
        std::vector<unsigned char> dst(256);
        const i64 wr=fg107_goose_state_get_data((i64)(intptr_t)&f,(i64)(intptr_t)dst.data(),(i64)dst.size());
        const bool size_ok = sz>0 && wr==sz;
        std::printf("%s state_get size=%lld written=%lld\n", size_ok?"PASS":"FAIL",(long long)sz,(long long)wr);
        ok = ok && size_ok;

        const i64 rd=fg107_goose_state_set_data((i64)(intptr_t)&f,(i64)(intptr_t)dst.data(),(i64)sz);
        const bool read_ok = rd==sz;
        std::printf("%s state_set read=%lld\n", read_ok?"PASS":"FAIL",(long long)rd);
        ok = ok && read_ok;

        const i64 ssz=fg107_goose_state_seq_get_size((i64)(intptr_t)&f,0,0);
        const i64 swr=fg107_goose_state_seq_get_data((i64)(intptr_t)&f,0,(i64)(intptr_t)dst.data(),(i64)dst.size(),0);
        const i64 srd=fg107_goose_state_seq_set_data((i64)(intptr_t)&f,0,(i64)(intptr_t)dst.data(),(i64)ssz,0);
        const bool seq_ok = ssz>0 && swr==ssz && srd==ssz;
        std::printf("%s state_seq size=%lld write=%lld read=%lld\n", seq_ok?"PASS":"FAIL",(long long)ssz,(long long)swr,(long long)srd);
        ok = ok && seq_ok;

        const i64 dev=fg107_goose_state_seq_get_data((i64)(intptr_t)&f,0,(i64)(intptr_t)dst.data(),(i64)dst.size(),1);
        const bool boundary_ok = dev==0; // on-device IO is an explicit native boundary
        std::printf("%s on_device_boundary rc=%lld\n", boundary_ok?"PASS":"FAIL",(long long)dev);
        ok = ok && boundary_ok;

        const i64 rel=fg107_goose_resource_release((i64)(intptr_t)&f);
        const bool rel_ok = rel==0 && f.has("sync") && f.has("opt_free");
        std::printf("%s resource_release rc=%lld\n", rel_ok?"PASS":"FAIL",(long long)rel);
        ok = ok && rel_ok;
    }
    { // reserve failure propagates
        RF f; f.vocab_only=0; f.reserve_ok=0;
        const i64 rc=fg107_goose_resource_init((i64)(intptr_t)&f);
        const bool fail_ok = rc==-1;
        std::printf("%s resource_init_reserve_fail rc=%lld\n", fail_ok?"PASS":"FAIL",(long long)rc);
        ok = ok && fail_ok;
    }
    std::printf("{\"status\":\"%s\",\"driver\":\"fg107-resource-trace\"}\n", ok?"PASS":"FAIL");
    return ok?0:2;
}
