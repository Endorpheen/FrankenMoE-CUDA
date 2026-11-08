// worker-4: executable native/Goose trace driver for context-execution.goose.
// UNVERIFIED PROPOSAL. Model-free: it links the generated Goose module against
// an ABI-identical in-process shim (no llama_context, no weights, no network)
// and compares the Goose service-call trace with a native oracle that follows
// inputs/active-llama-context.cpp:1908-2308 on the same inputs.
//
// Build (parent):
//   c++ -std=c++17 -O2 context-trace-driver.cpp <goose-generated.o> -o fg107-trace
// The Goose object must export (strong):
//   fg107_goose_decode, fg107_goose_output_reorder, fg107_goose_sched_copy_experts
//
// Exit: 0 all scenarios pass, 2 mismatch, 3 missing export (link failure is
// preferred; this is only a runtime guard).
#include "context-services.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" int64_t fg107_goose_decode(int64_t, int64_t);
extern "C" int64_t fg107_goose_output_reorder(int64_t);
extern "C" int64_t fg107_goose_sched_copy_experts(int64_t, int64_t, int64_t, int64_t, int64_t);

using i64 = int64_t;
static const i64 H_GF = 0x1000, H_NODE = 0x2000, H_IDS = 0x3000, H_SRC = 0x4000,
                 H_DST = 0x5000, H_BE = 0x6000, H_LOGITS = 0x8000, H_UB = 0x9000;

struct TestUb { int32_t n = 0; std::vector<int32_t> out, pos, seq; };

struct TestFrame {
    i64 n_ctx=256,n_batch=8,n_ubatch=4,n_seq_max=1,n_vocab=16,n_embd=8,n_embd_out=8,n_cls_out=0;
    i64 n_outputs_max=8,n_outputs_max_per_seq=8,n_outputs=0;
    bool has_memory=true, sched_present=true, embeddings=false, nextn=false, nextn_masked=false;
    bool causal=true, kv_unified=false, pipeline=false, reuse_disabled=false, arch_t5=false, has_logits=true;
    bool has_samplers=false, reuse=false, moe_present=false;
    std::vector<int32_t> b_tokens,b_output,b_pos,b_seq;
    i64 n_tokens_all=0,n_outputs_all=0; std::vector<int32_t> out_ids;
    std::vector<TestUb> ubs; size_t ui=0;
    i64 mctx_status=0, compute_status=0; bool mctx_next=false;
    float logits_buf[4096]={0}, embd_buf[4096]={0}, nextn_buf[4096]={0}, layer_buf[4096]={0};
    i64 logits_count=0, embd_count=0, nextn_count=0, layer_count=0;
    std::vector<int32_t> output_ids, embd_idx, ce_data; std::vector<uint8_t> ce_used;
    std::vector<std::pair<int32_t,int32_t>> swaps;
    i64 ce_ids=0, last_ret=0, last_error=0; std::vector<int32_t> counts;
    i64 layer_n=1;
    i64 node_op=30, src_ne2=8, src_nb2=64, ids_ne0=4, ids_ne1=1, ids_nb0=4, ids_nb1=16;
    std::vector<std::string> trace;
    void rec(const char * op, i64 a=0, i64 b=0, i64 c=0) {
        char buf[96];
        std::snprintf(buf,sizeof buf,"%s:%lld,%lld,%lld",op,(long long)a,(long long)b,(long long)c);
        trace.push_back(buf);
    }
    const TestUb & ub() const { return ubs[ui]; }
};

static TestFrame & F(i64 h) { return *(TestFrame *)(intptr_t) h; }
static TestUb  & U(i64 h) { return F(h).ubs[F(h).ui]; }

extern "C" i64 fg107_ctx(i64 h, i64 f, i64 a) {
    auto & x = F(h);
    switch (f) {
        case 0: return x.has_memory; case 1: return x.sched_present; case 2: return x.n_ctx;
        case 3: return x.n_batch; case 4: return x.n_ubatch; case 5: return x.n_seq_max;
        case 6: return x.n_vocab; case 7: return x.n_embd; case 8: return x.n_embd_out;
        case 9: return x.n_cls_out; case 10: return x.embeddings; case 11: return x.nextn;
        case 12: return x.nextn_masked; case 13: return x.causal; case 14: return x.kv_unified;
        case 15: return x.pipeline; case 16: return x.reuse_disabled; case 17: return 0;
        case 18: return 0; case 19: return x.arch_t5; case 20: return x.has_logits;
        case 21: return x.n_outputs_max; case 22: return x.n_outputs_max_per_seq;
        case 23: return x.has_samplers; case 24: return x.layer_n; case 25: return x.n_outputs;
        case 26: return 1; case 31: return 8; case 32: return 0x7fffffff; case 33: return 1;
        case 36: return 0;
        case 37: if ((size_t)a >= x.counts.size()) x.counts.assign(8,0); return ++x.counts[(size_t)a];
        case 39: return x.last_ret;
        default: x.rec("ctx_bad",f,a); return 0;
    }
}
extern "C" i64 fg107_ctx_set(i64 h, i64 f, i64 v) {
    auto & x = F(h);
    switch (f) {
        case 0: x.n_outputs=(i64)v; return 0; case 1: return 0;
        case 2: x.last_error=(int)v; return 0; case 3: return 0;
        case 4: x.last_ret=(i64)v; return 0; case 5: x.counts.assign(8,0); return 0;
        default: return 0;
    }
}
extern "C" i64 fg107_perf(i64 h, i64 op, i64 f, i64 v) {
    auto & x = F(h); (void)v;
    x.rec(op==0?"perf_get":"perf_set",f,op==0?0:v);
    return f==9 ? 1 : 0;
}
extern "C" i64 fg107_log(i64 h, i64 site, i64 a, i64 b) { F(h).rec("log",site,a,b); return 0; }
extern "C" i64 fg107_batch(i64 h, i64 f, i64 i, i64 s) {
    auto & x = F(h);
    switch (f) {
        case 0: return (i64)x.b_tokens.size(); case 1: return x.n_embd; case 2: return x.n_embd;
        case 3: return x.b_output[(size_t)i]; case 4: return 1; case 5: return x.b_seq[(size_t)i];
        default: return 0;
    }
}
extern "C" i64 fg107_ub(i64 h, i64 f, i64 i, i64 s) {
    auto & u = U(h); (void)s;
    switch (f) {
        case 0: return u.n; case 1: return u.out[(size_t)i]; case 2: return 1;
        case 3: return u.seq[(size_t)i]; case 4: return u.pos[(size_t)i];
        case 5: return 1; case 6: return 0; case 7: return 0; case 8: return u.n; case 9: return (i64)i;
        default: return 0;
    }
}
extern "C" i64 fg107_balloc(i64 h, i64 op, i64 a, i64 b) {
    auto & x = F(h);
    switch (op) {
        case 0: x.rec("balloc_init"); x.n_tokens_all=(i64)x.b_tokens.size();
                x.n_outputs_all=0; x.out_ids.clear();
                for (size_t i=0;i<x.b_output.size();++i) if (x.b_output[i]) { x.n_outputs_all++; x.out_ids.push_back((int32_t)i); }
                x.ui=0; return 1;
        case 1: return x.n_tokens_all; case 2: return x.n_outputs_all;
        case 3: return (i64)x.out_ids.size(); case 4: return x.out_ids[(size_t)a];
        case 5: { auto t=x.out_ids[(size_t)a]; x.out_ids[(size_t)a]=x.out_ids[(size_t)b]; x.out_ids[(size_t)b]=t; return 0; }
        default: (void)b; return 0;
    }
}
extern "C" i64 fg107_mem(i64 h, i64 op, i64 a, i64 b, i64 d) {
    auto & x = F(h);
    x.rec("mem",op,a,b);
    switch (op) {
        case 0: x.ui=0; return h; case 1: return h; case 2: return h;
        case 3: (void)d; return 1; default: return 0;
    }
}
extern "C" i64 fg107_mctx(i64 h, i64 op, i64 a) {
    auto & x = F(h); (void)a;
    switch (op) {
        case 0: x.rec("mctx_status"); return x.mctx_status;
        case 1: x.rec("mctx_apply"); return 1;
        case 2: x.rec("mctx_next"); if (x.ui+1 < x.ubs.size()) { x.ui++; x.mctx_next=true; return 1; } x.mctx_next=false; return 0;
        case 3: return h; default: return 0;
    }
}
extern "C" i64 fg107_res_prev(i64 h) { F(h).rec("res_prev"); return h; }
extern "C" i64 fg107_res_prev_active(i64 h) { (void)h; return 0; }
extern "C" i64 fg107_res_set_prev_active(i64 h, i64 r) { F(h).rec("prev_active",r); return 0; }
extern "C" i64 fg107_res_reset_all_prev(i64 h) { F(h).rec("reset_all_prev"); return 0; }
extern "C" i64 fg107_res(i64 h, i64 r, i64 op, i64 a) {
    auto & x = F(h); (void)r;
    switch (op) {
        case 0: return H_GF; case 1: return x.reuse; case 2: x.rec("res_reset"); return 0;
        case 3: x.rec("set_inputs",a); return 0; case 4: return x.logits_count ? H_LOGITS : 0;
        case 5: return x.embd_count ? H_LOGITS : 0; case 6: return 0; case 7: return x.nextn_count ? H_LOGITS : 0;
        case 8: return x.layer_count ? H_LOGITS : 0; case 9: return 0; default: return 0;
    }
}
extern "C" i64 fg107_res_sampling_tensor(i64 h, i64 kind, i64 i) { (void)h; (void)kind; (void)i; return i < 0 ? 0 : 0; }
extern "C" i64 fg107_graph_params(i64 h, i64 r, i64 u, i64 m, i64 g) {
    F(h).rec("graph_params",r,u); (void)m; (void)g; return H_GF;
}
extern "C" i64 fg107_model_build_graph(i64 h, i64 gp) { F(h).rec("build_graph",gp); return H_GF; }
extern "C" i64 fg107_graph_reserve(i64 h, i64 t, i64 s, i64 o, i64 m, i64 l) {
    F(h).rec("graph_reserve",t,s); (void)o; (void)m; (void)l; return H_GF;
}
extern "C" i64 fg107_graph(i64 g, i64 op, i64 a) {
    (void)g;
    switch (op) { case 0: return 1; case 1: (void)a; return H_NODE; default: return 0; }
}
extern "C" i64 fg107_sched(i64 h, i64 op, i64 a, i64 b, i64 d) {
    auto & x = F(h); (void)b; (void)d;
    switch (op) {
        case 0: x.rec("sched_reset"); return 0;
        case 1: x.rec("sched_eval_cb"); return 0;
        case 2: x.rec("sched_alloc",a); return 1;
        case 3: x.rec("sched_sync"); return 0;
        case 4: return H_BE;
        case 5: return 0;
        case 6: x.rec("compute",a); return x.compute_status;
        case 7: x.rec("sched_reserve"); return 0;
        case 8: x.rec("copy_experts_reset"); return 0;
        case 9: x.rec("apply_threadpool",a); return 0;
        case 10: x.rec("apply_n_threads",a); return 0;
        case 11: return 1; case 12: return H_BE; case 13: return 0; case 14: return 0; case 15: return 0;
        default: return 0;
    }
}
extern "C" i64 fg107_tensor(i64 t, i64 f, i64 i) {
    if (t == H_NODE) {
        if (f == 0) return 30;                     // MUL_MAT_ID
        if (f == 1) return i == 0 ? H_DST : (i == 2 ? H_IDS : 0);
        return 0;
    }
    if (t == H_IDS) {
        switch (f) { case 2: return i==0?4:1; case 3: return i==0?4:16; case 4: return 4;
                     case 5: return 16; case 6: return 1; case 7: return 0x7000; default: return 0; }
    }
    if (t == H_SRC) { if (f == 2) return i==2?8:1; if (f == 3) return i==2?64:4; if (f==7) return 0x5000; return 0; }
    if (t == H_LOGITS) { if (f==4) return 64; if (f==5) return 256; if (f==6) return 1; if (f==7) return 0x8000; return 0; }
    return 0;
}
extern "C" i64 fg107_tensor_get_async(i64 be, i64 t, i64 dst, i64 off, i64 size) {
    (void)be; (void)t; (void)dst; (void)off; (void)size; return 0;
}
extern "C" i64 fg107_tensor_set_async(i64 be, i64 t, i64 src, i64 off, i64 size) {
    (void)be; (void)t; (void)src; (void)off; (void)size; return 0;
}
extern "C" i64 fg107_backend_synchronize(i64 be) { (void)be; return 0; }
extern "C" i64 fg107_moe(i64 h, i64 op, i64 a, i64 b, i64 d, i64 e, i64 f) {
    auto & x = F(h); (void)a; (void)b; (void)d; (void)e; (void)f;
    switch (op) { case 0: return x.moe_present; case 1: return 0; case 2: return 0; default: return 0; }
}
extern "C" i64 fg107_op_mul_mat_id(void) { return 30; }
extern "C" i64 fg107_out(i64 h, i64 op, i64 a, i64 b, i64 d) {
    auto & x = F(h);
    switch (op) {
        case 0: return 0; case 1: x.rec("buf_alloc",a); return 1; case 2: return (i64)(intptr_t)x.logits_buf;
        case 3: return (i64)(intptr_t)x.logits_buf; case 4: return x.logits_count;
        case 5: return x.embd_count ? (i64)(intptr_t)x.embd_buf : 0; case 6: return x.embd_count;
        case 7: return x.nextn_count ? (i64)(intptr_t)x.nextn_buf : 0; case 8: return x.nextn_count;
        case 9: return x.layer_count ? (i64)(intptr_t)x.layer_buf : 0; case 10: return x.layer_count;
        case 11: return x.layer_count ? 1 : 0; case 12: return 0; case 13: x.rec("embd_seq_clear"); return 0;
        case 16: x.rec("sampling_prepare",a); return 0; case 17: x.rec("sampling_clear"); return 0;
        case 18: return 0; case 19: return 0; case 20: return 0;
        case 21: x.rec("sampling_count",a,b); return 0;
        case 22: return 4;
        case 23: x.rec("sampling_swap",a,b); return 0;
        default: (void)d; return 0;
    }
}
extern "C" i64 fg107_out_bind(i64 h, i64 which, i64 il, i64 off, i64 count) {
    auto & x = F(h); (void)il; (void)off;
    switch (which) {
        case 0: x.logits_count=count; return 0; case 1: x.embd_count=count; return 0;
        case 2: x.nextn_count=count; return 0; case 3: x.layer_count=count; return 0;
        default: return 0;
    }
}
extern "C" i64 fg107_out_view_swap(i64 h, i64 which, i64 il, i64 i, i64 j) {
    F(h).rec("view_swap",which,i,j); (void)il; return 0;
}
extern "C" i64 fg107_exec(i64 h, i64 op, i64 a, i64 b, i64 d) {
    auto & x = F(h); (void)d;
    switch (op) {
        case 0: if (x.output_ids.empty()) x.output_ids.resize((size_t)a); return 0;
        case 1: return (i64)x.output_ids.size(); case 2: return x.output_ids[(size_t)a];
        case 3: x.output_ids[(size_t)a]=(int32_t)b; return 0;
        case 4: for (auto & v : x.output_ids) v=(int32_t)a; return 0;
        case 5: return (i64)x.swaps.size();
        case 6: return b==0 ? x.swaps[(size_t)a].first : x.swaps[(size_t)a].second;
        case 7: x.swaps.push_back({(int32_t)a,(int32_t)b}); x.rec("swap_push",a,b); return 0;
        case 8: x.swaps.clear(); x.rec("swaps_clear"); return 0;
        case 9: return (i64)x.embd_idx.size(); case 10: return x.embd_idx[(size_t)a];
        case 11: { auto & u=U(h); for (int32_t k=0;k<u.n;++k) x.embd_idx.push_back(k); x.rec("embd_append",u.n); return 0; }
        case 12: x.embd_idx.clear(); x.rec("embd_clear"); return 0;
        case 13: { auto t=x.embd_idx[(size_t)a]; x.embd_idx[(size_t)a]=x.embd_idx[(size_t)b]; x.embd_idx[(size_t)b]=t; x.rec("embd_swap",a,b); return 0; }
        case 14: return x.ce_ids; case 15: x.ce_ids=a; return 0;
        case 16: x.ce_data.resize((size_t)a); return 0; case 17: return (i64)(intptr_t)x.ce_data.data();
        case 18: return x.ce_data[(size_t)a]; case 19: return (i64)x.ce_used.size();
        case 20: x.ce_used.assign((size_t)a,0); return 0; case 21: return x.ce_used[(size_t)a];
        case 22: x.ce_used[(size_t)a]=(uint8_t)b; return 0; default: return 0;
    }
}
extern "C" i64 fg107_exec_copy_id_at(i64 h, i64 i) { return F(h).ce_data[(size_t)i]; }

// ---- native oracle: transcription of decode for the no-embd/no-sampler path
static int oracle_decode(TestFrame & x) {
    if (!x.has_memory) return -100;
    if (x.b_tokens.empty()) return -1;
    fg107_balloc((i64)(intptr_t)&x,0,0,0);
    const i64 nt=fg107_balloc((i64)(intptr_t)&x,1,0,0);
    const i64 no=fg107_balloc((i64)(intptr_t)&x,2,0,0);
    (void)nt; (void)no;
    fg107_out((i64)(intptr_t)&x,12,0,0,0);
    fg107_out((i64)(intptr_t)&x,13,0,0,0);
    fg107_perf((i64)(intptr_t)&x,0,2,0);
    fg107_perf((i64)(intptr_t)&x,1,2,1);
    fg107_perf((i64)(intptr_t)&x,1,0,nt);
    fg107_exec((i64)(intptr_t)&x,8,0,0,0);
    fg107_exec((i64)(intptr_t)&x,12,0,0,0);
    fg107_sched((i64)(intptr_t)&x,7,0,0,0);
    fg107_mem((i64)(intptr_t)&x,2,0,0,0);
    fg107_mctx((i64)(intptr_t)&x,0,0);
    fg107_res_reset_all_prev((i64)(intptr_t)&x);
    fg107_res_set_prev_active((i64)(intptr_t)&x,0);
    fg107_mctx((i64)(intptr_t)&x,1,0);
    fg107_mem((i64)(intptr_t)&x,1,0,0,0);
    fg107_graph_reserve((i64)(intptr_t)&x,nt,x.n_seq_max,no,0,0);
    fg107_mem((i64)(intptr_t)&x,0,x.n_ubatch,0,0);
    fg107_mctx((i64)(intptr_t)&x,0,0);
    // output_reserve (no sampling, no embd)
    if (fg107_exec((i64)(intptr_t)&x,1,0,0,0)==0) fg107_exec((i64)(intptr_t)&x,0,x.n_batch,0,0);
    fg107_out((i64)(intptr_t)&x,1,(x.n_vocab*no)*4,0,0);
    fg107_out_bind((i64)(intptr_t)&x,0,0,0,x.n_vocab*no);
    fg107_out((i64)(intptr_t)&x,17,0,0,0);
    fg107_exec((i64)(intptr_t)&x,4,-1,0,0);
    fg107_ctx_set((i64)(intptr_t)&x,0,0);
    fg107_ctx_set((i64)(intptr_t)&x,3,0);
    i64 nop=0, ntp=0;
    for (;;) {
        const i64 u=(i64)(intptr_t)&x;
        const i64 unt=fg107_ub(u,0,0,0);
        i64 nnew=0;
        if (x.n_outputs_all==x.n_tokens_all) nnew=unt;
        else for (i64 i=0;i<unt;++i) nnew+=fg107_ub(u,1,i,0);
        fg107_ctx_set((i64)(intptr_t)&x,0,nnew);
        fg107_mctx((i64)(intptr_t)&x,1,0);
        fg107_res_prev((i64)(intptr_t)&x);
        fg107_graph_params((i64)(intptr_t)&x,u,u,0,0);
        fg107_res_prev_active((i64)(intptr_t)&x);
        fg107_res((i64)(intptr_t)&x,u,1,0);
        fg107_res_set_prev_active((i64)(intptr_t)&x,0);
        fg107_res((i64)(intptr_t)&x,u,2,0);
        fg107_sched((i64)(intptr_t)&x,0,0,0,0);
        fg107_sched((i64)(intptr_t)&x,1,0,0,0);
        fg107_model_build_graph((i64)(intptr_t)&x,0);
        fg107_sched((i64)(intptr_t)&x,2,0,0,0);
        fg107_res_set_prev_active((i64)(intptr_t)&x,u);
        fg107_res((i64)(intptr_t)&x,u,3,u);
        const i64 st=fg107_sched((i64)(intptr_t)&x,6,0,0,0);
        if (st != 0) {
            for (i64 s=0;s<x.n_seq_max;++s) {
                i64 pmin=0x7fffffff;
                for (i64 i=0;i<unt;++i) {
                    if (fg107_ub(u,3,i,0)==s) { const i64 p=fg107_ub(u,4,i,0); if (p<pmin) pmin=p; }
                }
                if (pmin!=0x7fffffff) fg107_mem((i64)(intptr_t)&x,3,s,pmin,-1);
            }
            fg107_ctx_set((i64)(intptr_t)&x,4,st);
            return st==1?2:(st==-2?-2:-3);
        }
        fg107_ctx_set((i64)(intptr_t)&x,4,0);
        fg107_res((i64)(intptr_t)&x,u,4,0);
        fg107_ctx((i64)(intptr_t)&x,10,0);
        fg107_ctx((i64)(intptr_t)&x,11,0);
        fg107_ctx((i64)(intptr_t)&x,25,0);
        fg107_out((i64)(intptr_t)&x,3,0,0,0);
        // needs_raw_logits: no samplers -> 1
        fg107_ub(u,1,0,0); fg107_ub(u,2,0,0); fg107_ctx((i64)(intptr_t)&x,36,0);
        fg107_ctx((i64)(intptr_t)&x,6,0);
        fg107_sched((i64)(intptr_t)&x,4,0,0,0);
        fg107_tensor_get_async((i64)(intptr_t)&x,0,0,0,0);
        fg107_ctx((i64)(intptr_t)&x,24,0);
        fg107_ctx((i64)(intptr_t)&x,12,0);
        fg107_ctx((i64)(intptr_t)&x,18,0);
        fg107_out((i64)(intptr_t)&x,7,0,0,0);
        fg107_exec((i64)(intptr_t)&x,9,0,0,0);
        nop+=fg107_ctx((i64)(intptr_t)&x,25,0); ntp+=unt;
        if (!fg107_mctx((i64)(intptr_t)&x,2,0)) break;
    }
    fg107_ctx_set((i64)(intptr_t)&x,0,no);
    if (no > 0) {
        fg107_balloc((i64)(intptr_t)&x,3,0,0);
        for (i64 i=0;i<no;++i) { const i64 id=fg107_balloc((i64)(intptr_t)&x,4,i,0); fg107_exec((i64)(intptr_t)&x,3,id,i,0); }
    }
    return 0;
}

static int cnt(const std::vector<std::string>&t,const char*p){int n=0;for(auto&s:t) if(s.rfind(p,0)==0) ++n;return n;}

static bool same_milestones(const std::vector<std::string>&a,const std::vector<std::string>&b){
    static const char * keys[] = {
        "mem:2","mem:0","mem:1","mem:3","graph_reserve","balloc_init",
        "sched_reserve","compute","set_inputs","graph_params","res_prev","mctx_status","mctx_apply"
    };
    for (const char * k : keys) if (cnt(a,k)!=cnt(b,k)) return false;
    return true;
}

static TestFrame make_decode(bool abort) {
    TestFrame x;
    x.n_seq_max=1; x.b_tokens={11,12,13,14,15}; x.b_output={0,0,1,0,1}; x.b_pos={0,1,2,3,4}; x.b_seq={0,0,0,0,0};
    x.ubs.push_back({4,{0,0,1,0},{0,1,2,3},{0,0,0,0}});
    x.ubs.push_back({1,{1},{4},{0}});
    x.compute_status = abort ? 1 : 0;
    return x;
}

int main() {
    bool ok = true;
    std::fprintf(stderr, "stage: decode\n"); std::fflush(stderr);
    // 1) decode differential (Goose vs native oracle)
    for (int ab=0; ab<2; ++ab) {
        TestFrame g=make_decode(ab!=0), o=make_decode(ab!=0);
        std::fprintf(stderr, "stage: decode ab=%d goose\n", ab); std::fflush(stderr);
        const int rg=fg107_goose_decode((i64)(intptr_t)&g,(i64)(intptr_t)&g);
        std::fprintf(stderr, "stage: decode ab=%d oracle\n", ab); std::fflush(stderr);
        const int ro=oracle_decode(o);
        const bool pass = rg==ro && same_milestones(g.trace,o.trace) && g.n_outputs==o.n_outputs;
        std::printf("%s decode_%s goose_rc=%d oracle_rc=%d trace=%zu/%zu n_out=%lld/%lld\n",
            pass?"PASS":"FAIL", ab?"abort":"normal", rg, ro, g.trace.size(), o.trace.size(),
            (long long)g.n_outputs,(long long)o.n_outputs);
        ok = ok && pass;
    }
    // 2) output_reorder
    {
        TestFrame g; g.n_vocab=4; g.n_embd=3; g.n_embd_out=3; g.logits_count=8; g.embd_count=6;
        g.nextn_count=0; g.layer_count=3; g.output_ids={0,1}; g.swaps={{0,1}}; g.embd_idx={1,0};
        fg107_goose_output_reorder((i64)(intptr_t)&g);
        int v0=0,v1=0,v3=0,sw=0,cl=0;
        for (auto & s : g.trace) {
            if (s.rfind("view_swap:0,",0)==0) v0++;
            if (s.rfind("view_swap:1,",0)==0) v1++;
            if (s.rfind("view_swap:3,",0)==0) v3++;
            if (s.rfind("embd_swap:",0)==0) sw++;
            if (s.rfind("embd_clear:",0)==0) cl++;
        }
        const bool pass = v0==4 && v1==3 && v3==3 && sw==1 && cl==1;
        std::printf("%s reorder swaps v0=%d v1=%d v3=%d idx=%d\n",pass?"PASS":"FAIL",v0,v1,v3,sw);
        ok = ok && pass;
    }
    // 3) expert-overlap callback (strong export, actual method)
    {
        TestFrame g; g.moe_present=false; g.ce_data={0,1,0,0}; g.ce_used.assign(8,0);
        const i64 r=fg107_goose_sched_copy_experts((i64)(intptr_t)&g,H_BE,H_SRC,H_DST,H_GF);
        const bool pass = r!=0 && g.ce_ids==H_IDS && g.ce_used[0]==1 && g.ce_used[1]==1 && g.ce_used[2]==0;
        std::printf("%s expert_copy rc=%lld ids=%lld used=%d%d%d\n",pass?"PASS":"FAIL",
            (long long)r,(long long)g.ce_ids,g.ce_used[0],g.ce_used[1],g.ce_used[2]);
        ok = ok && pass;
    }
    std::printf("{\"status\":\"%s\",\"driver\":\"fg107-native-goose-trace\"}\n", ok?"PASS":"FAIL");
    return ok?0:2;
}
