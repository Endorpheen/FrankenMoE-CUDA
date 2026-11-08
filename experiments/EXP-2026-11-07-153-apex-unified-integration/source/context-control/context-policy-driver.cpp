// worker-4: executable policy/control trace driver (cparams, fused-op policy,
// graph-cb pin policy, realloc counters). Model-free ABI shim; UNVERIFIED.
#include "context-services.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" int64_t fg107_goose_resolve_fused(int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_graph_cb_policy(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_realloc_policy(int64_t, int64_t, int64_t, int64_t, int64_t);
extern "C" int64_t fg107_goose_cparams_policy(int64_t);

using i64 = int64_t;
struct PF {
    // params
    i64 p_rope_type=0, p_attn_type=5, p_fa_type=8, p_n_batch=8, p_n_ubatch=4, p_n_out=0, p_n_out_seq=0;
    float p_rope_base=10000.0f, p_rope_scale=1.0f, p_yarn_ext=-1.0f, p_yarn_attn=-1.0f;
    // hparams
    i64 h_rope_type=2, h_pool=4, h_causal=1, h_arch=0;
    float h_rope_base=10000.0f, h_rope_scale=1.0f, h_yarn_log_mul=0.0f, h_rope_attn=1.0f;
    // cparams
    i64 c_rope_type=0, c_pool=3, c_causal=0, c_flash=0, c_auto_fa=0;
    i64 fused[7]={0,0,0,0,0,0,0};
    i64 auto_f[4]={0,0,0,0};
    i64 c_n_batch=0, c_n_ubatch=0, c_n_out=0, c_n_out_seq=0, c_n_ctx=256;
    float c_rope_scale=1.0f, c_yarn_ext=0.0f, c_yarn_attn=1.0f;
    // fused probe
    struct N { i64 op, il, dev, layer; };
    std::vector<N> nodes; i64 gf_ok=1; bool has_encoder=false;
    i64 same=0, diff=0;
    std::vector<std::string> trace;
    void rec(const char* s){ trace.push_back(s); }
};
static PF & P(i64 h){ return *(PF*)(intptr_t)h; }

extern "C" i64 fg107_pol(i64 h, i64 op, i64 a, i64 b, i64 c, i64 d) {
    auto & x = P(h); (void)d;
    switch (op) {
        case 0: return x.fused[a];
        case 1: x.fused[a]=b; return 0;
        case 2: return x.auto_f[a];
        case 3: x.auto_f[a]=0; return 0;
        case 4: x.rec("graph_reserve_probe"); return x.gf_ok ? 0x60 : 0;
        case 5: return (i64)x.nodes.size();
        case 6: return x.nodes[(size_t)a].op;
        case 7: return x.nodes[(size_t)a].il;
        case 8: return (i64)a;
        case 9: return 0x100;                     // backend
        case 10: return x.nodes.empty() ? 0 : x.nodes[(size_t)0].dev;
        case 11: return x.nodes.empty() ? 0 : x.nodes[(size_t)0].layer;
        case 12: x.rec("log"); (void)b; (void)c; return 0;
        case 13: return a;                         // probe op == which
        case 14: return a==2 ? 16 : 1;
        case 15: if (a==0) x.diff++; else x.same++; return 0;
        case 16: return a==0 ? x.same : x.diff;
        default: return 0;
    }
}
extern "C" i64 fg107_cpi(i64 h, i64 op, i64 f, i64 v) {
    auto & x = P(h);
    if (op==0) switch (f) {
        case 0:return x.p_rope_type;case 1:return x.p_attn_type;case 2:return x.p_fa_type;
        case 3:return x.p_n_batch;case 4:return x.p_n_ubatch;case 5:return x.p_n_out;case 6:return x.p_n_out_seq;
        case 200:return x.h_rope_type;case 201:return x.h_pool;case 202:return x.h_causal;case 203:return x.h_arch;
        case 400:return x.c_rope_type;case 401:return x.c_pool;case 402:return x.c_causal;case 403:return x.c_flash;
        case 404:return x.c_auto_fa;case 405:return x.fused[1];case 406:return x.fused[2];case 407:return x.auto_f[1];
        case 408:return x.fused[3];case 409:return x.auto_f[2];case 410:return x.fused[4];case 411:return x.fused[5];
        case 412:return x.fused[6];case 413:return x.auto_f[3];case 414:return x.c_n_batch;case 415:return x.c_n_ubatch;
        case 416:return x.c_n_out;case 417:return x.c_n_out_seq;case 418:return x.c_n_ctx;default:return 0;
    }
    switch (f) {
        case 400:x.c_rope_type=v;return 0;case 401:x.c_pool=v;return 0;case 402:x.c_causal=v;return 0;
        case 403:x.c_flash=v;return 0;case 404:x.c_auto_fa=v;return 0;case 405:x.fused[1]=v;return 0;
        case 406:x.fused[2]=v;return 0;case 407:x.auto_f[1]=v;return 0;case 408:x.fused[3]=v;return 0;
        case 409:x.auto_f[2]=v;return 0;case 410:x.fused[4]=v;return 0;case 411:x.fused[5]=v;return 0;
        case 412:x.fused[6]=v;return 0;case 413:x.auto_f[3]=v;return 0;case 414:x.c_n_batch=v;return 0;
        case 415:x.c_n_ubatch=v;return 0;case 416:x.c_n_out=v;return 0;case 417:x.c_n_out_seq=v;return 0;
        default:return 0;
    }
}
extern "C" float fg107_cpf(i64 h, i64 op, i64 f, float v) {
    auto & x = P(h);
    if (op==0) switch (f) {
        case 100:return x.p_rope_base;case 101:return x.p_rope_scale;case 102:return x.p_yarn_ext;case 103:return x.p_yarn_attn;
        case 300:return x.h_rope_base;case 301:return x.h_rope_scale;case 302:return x.h_yarn_log_mul;case 303:return x.h_rope_attn;
        case 500:return x.c_rope_scale;case 501:return x.c_yarn_ext;case 502:return x.c_yarn_attn;default:return 0.0f;
    }
    switch (f) {
        case 500:x.c_rope_scale=v;return 0;case 501:x.c_yarn_ext=v;return 0;case 502:x.c_yarn_attn=v;return 0;
        default:return 0;
    }
}
extern "C" i64 fg107_cpc(i64 h, i64 which) {
    (void)h; static const i64 c[10]={0,1,2,3,4,5,6,7,8,0}; return c[which];
}
extern "C" i64 fg107_cplog(i64 h, i64 site, i64 a, i64 b) { (void)site;(void)a;(void)b;P(h).rec("cplog");return 0; }
extern "C" i64 fg107_cpmodel(i64 h) { return P(h).has_encoder?1:0; }

static bool near(float a,float b){return (a>b?a-b:b-a)<1e-4f;}
int main(){
    bool ok=true;
    { // cparams policy
        PF f; f.p_rope_type=0; f.h_rope_type=2; f.p_rope_scale=1.0f; f.p_yarn_ext=-1.0f;
        f.h_yarn_log_mul=0.0f; f.h_rope_attn=1.0f; f.p_attn_type=5; f.h_causal=1;
        f.p_fa_type=8; f.p_n_batch=8; f.p_n_ubatch=4; f.p_n_out=0; f.p_n_out_seq=0; f.c_n_ctx=256;
        f.c_yarn_ext=-1.0f; f.c_flash=0;
        const i64 rc=fg107_goose_cparams_policy((i64)(intptr_t)&f);
        const bool pass = rc==0 && f.c_rope_type==2 && near(f.c_yarn_ext,1.0f) &&
            f.c_pool==4 && f.c_causal==1 && f.c_flash==1 && f.c_auto_fa==1 &&
            f.fused[1]==1 && f.fused[2]==1 && f.fused[3]==1 && f.fused[4]==1 &&
            f.c_n_batch==8 && f.c_n_ubatch==4 && f.c_n_out==8 && f.c_n_out_seq==8;
        std::printf("%s cparams_policy rc=%lld rope=%lld yarn=%.3f batch=%lld/%lld out=%lld/%lld\n",
            pass?"PASS":"FAIL",(long long)rc,(long long)f.c_rope_type,(double)f.c_yarn_ext,
            (long long)f.c_n_batch,(long long)f.c_n_ubatch,(long long)f.c_n_out,(long long)f.c_n_out_seq);
        ok = ok && pass;
    }
    { // graph cb pin policy
        PF f;
        const i64 a=fg107_goose_graph_cb_policy((i64)(intptr_t)&f,8,0,1,0,0);
        const i64 b=fg107_goose_graph_cb_policy((i64)(intptr_t)&f,64,0,1,0,0);
        const i64 c=fg107_goose_graph_cb_policy((i64)(intptr_t)&f,8,-1,1,0,0);
        const i64 d=fg107_goose_graph_cb_policy((i64)(intptr_t)&f,64,0,0,1,1);
        const bool pass = a==1 && b==0 && c==0 && d==1;
        std::printf("%s graph_cb_policy %lld%lld%lld%lld\n",pass?"PASS":"FAIL",(long long)a,(long long)b,(long long)c,(long long)d);
        ok = ok && pass;
    }
    { // realloc policy
        PF f;
        fg107_goose_realloc_policy((i64)(intptr_t)&f,1,0,10,2); // ignored
        fg107_goose_realloc_policy((i64)(intptr_t)&f,0,1,10,2); // diff
        fg107_goose_realloc_policy((i64)(intptr_t)&f,0,0,10,2); // same
        const bool pass = f.diff==1 && f.same==1;
        std::printf("%s realloc_policy same=%lld diff=%lld\n",pass?"PASS":"FAIL",(long long)f.same,(long long)f.diff);
        ok = ok && pass;
    }
    { // fused-op mismatch disables the flag
        PF f; f.auto_f[0]=1; f.fused[0]=1; f.nodes.push_back({0,0,0xAAA,0xBBB});
        const i64 rc=fg107_goose_resolve_fused((i64)(intptr_t)&f,0,1);
        const bool pass = rc==0 && f.fused[0]==0 && f.auto_f[0]==0;
        std::printf("%s resolve_fused_mismatch rc=%lld flag=%lld auto=%lld\n",pass?"PASS":"FAIL",
            (long long)rc,(long long)f.fused[0],(long long)f.auto_f[0]);
        ok = ok && pass;
    }
    { // fused-op match keeps the flag
        PF f; f.auto_f[1]=1; f.fused[1]=1; f.fused[2]=1; f.nodes.push_back({1,0,0xAAA,0xAAA});
        const i64 rc=fg107_goose_resolve_fused((i64)(intptr_t)&f,0,1);
        const bool pass = rc==0 && f.fused[1]==1 && f.auto_f[1]==0;
        std::printf("%s resolve_fused_match rc=%lld flag=%lld auto=%lld\n",pass?"PASS":"FAIL",
            (long long)rc,(long long)f.fused[1],(long long)f.auto_f[1]);
        ok = ok && pass;
    }
    std::printf("{\"status\":\"%s\",\"driver\":\"fg107-policy-trace\"}\n", ok?"PASS":"FAIL");
    return ok?0:2;
}
