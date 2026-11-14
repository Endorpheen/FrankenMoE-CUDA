// Differential gate for P7-04 S1a: type selection, fallback and imatrix policy.
//
// The native oracle is llama-quant.cpp included into this translation unit:
// llama_quant_compute_types() runs on mock tensors, and the file-local
// tensor_requires_imatrix() and quantize_state_impl::n_fallback are readable
// directly. The port is quant.goose (qt_compute, qt_tensor_requires_imatrix,
// qt_ggml_quantize_requires_imatrix, qt_default_type).
//
// Coverage:
//   - llama_quant_compute_types over every llama_ftype x n_gqa x n_expert x
//     n_layer x tensor shape, the public overrides (token/output type,
//     quantize_output, only_copy, pure) and every tensor name the allow-cut
//     and categorizer rules touch; the n_fallback counter is compared too;
//   - qt_default_type against llama_ftype_get_default_type for every ftype;
//   - tensor_requires_imatrix for every ggml_type x ftype x representative name;
//   - ggml_quantize_requires_imatrix for every ggml_type against the real
//     exported function.
//
// built and run by tests/build.sh.

#include "runtime-gen.h"
#include "llama-quant.cpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static int g_cases  = 0;
static int g_values = 0;
static int g_fails  = 0;

static void fail(const std::string & what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 200) {
        printf("FAIL %s: %s\n", what.c_str(), detail.c_str());
    }
}

static sl_u8 sl(std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}
static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

struct tensor_spec {
    std::string name;
    int64_t     ne0, ne1, ne2;
    int         ndims;
    ggml_type   orig;
};

struct Case {
    std::string  id;
    int          n_layer   = 8;
    int          n_expert  = 0;
    int          n_embd    = 2048;
    int          n_ff      = 8192;
    int          n_head    = 8;
    int          n_head_kv = 2;
    int          n_embd_head_k = 128;
    int          n_embd_head_v = 128;
    llama_ftype  ftype     = LLAMA_FTYPE_MOSTLY_Q4_K_M;
    int          token_type  = (int) GGML_TYPE_COUNT;
    int          output_type = (int) GGML_TYPE_COUNT;
    int          quantize_output = 1;
    int          only_copy       = 0;
    int          pure            = 0;
    std::vector<tensor_spec> tensors;
};

// Full llama_ftype list, holes included: 4-6 and 33-35 are removed values.
static const llama_ftype kFtypes[] = {
    LLAMA_FTYPE_ALL_F32,          LLAMA_FTYPE_MOSTLY_F16,
    LLAMA_FTYPE_MOSTLY_Q4_0,      LLAMA_FTYPE_MOSTLY_Q4_1,
    LLAMA_FTYPE_MOSTLY_Q8_0,      LLAMA_FTYPE_MOSTLY_Q5_0,
    LLAMA_FTYPE_MOSTLY_Q5_1,      LLAMA_FTYPE_MOSTLY_Q2_K,
    LLAMA_FTYPE_MOSTLY_Q3_K_S,    LLAMA_FTYPE_MOSTLY_Q3_K_M,
    LLAMA_FTYPE_MOSTLY_Q3_K_L,    LLAMA_FTYPE_MOSTLY_Q4_K_S,
    LLAMA_FTYPE_MOSTLY_Q4_K_M,    LLAMA_FTYPE_MOSTLY_Q5_K_S,
    LLAMA_FTYPE_MOSTLY_Q5_K_M,    LLAMA_FTYPE_MOSTLY_Q6_K,
    LLAMA_FTYPE_MOSTLY_IQ2_XXS,   LLAMA_FTYPE_MOSTLY_IQ2_XS,
    LLAMA_FTYPE_MOSTLY_Q2_K_S,    LLAMA_FTYPE_MOSTLY_IQ3_XS,
    LLAMA_FTYPE_MOSTLY_IQ3_XXS,   LLAMA_FTYPE_MOSTLY_IQ1_S,
    LLAMA_FTYPE_MOSTLY_IQ4_NL,    LLAMA_FTYPE_MOSTLY_IQ3_S,
    LLAMA_FTYPE_MOSTLY_IQ3_M,     LLAMA_FTYPE_MOSTLY_IQ2_S,
    LLAMA_FTYPE_MOSTLY_IQ2_M,     LLAMA_FTYPE_MOSTLY_IQ4_XS,
    LLAMA_FTYPE_MOSTLY_IQ1_M,     LLAMA_FTYPE_MOSTLY_BF16,
    LLAMA_FTYPE_MOSTLY_TQ1_0,     LLAMA_FTYPE_MOSTLY_TQ2_0,
    LLAMA_FTYPE_MOSTLY_MXFP4_MOE, LLAMA_FTYPE_MOSTLY_NVFP4,
    LLAMA_FTYPE_MOSTLY_Q1_0,      LLAMA_FTYPE_MOSTLY_Q2_0,
};

static const int kNFtypes = (int) (sizeof(kFtypes) / sizeof(kFtypes[0]));

static std::string ftype_tag(llama_ftype f) {
    return "f" + std::to_string((int) f);
}

static void add_dense(Case & c, bool experts) {
    const int64_t ngqa = (int64_t) c.n_embd * c.n_head_kv / c.n_head;
    c.tensors.push_back({ "token_embd.weight", c.n_embd, 32000, 1, 2, GGML_TYPE_F16 });
    c.tensors.push_back({ "output.weight",     c.n_embd, 32000, 1, 2, GGML_TYPE_F16 });
    if (experts) {
        c.tensors.push_back({ "blk.0.ffn_gate_inp.weight", c.n_embd, c.n_expert, 1, 2, GGML_TYPE_F16 });
    }
    for (int i = 0; i < c.n_layer; ++i) {
        const std::string p = "blk." + std::to_string(i) + ".";
        c.tensors.push_back({ p + "attn_q.weight",      c.n_embd, c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_k.weight",      ngqa,     c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_v.weight",      ngqa,     c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_qkv.weight",    c.n_embd, c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_kv_b.weight",   c.n_embd, c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_output.weight", c.n_embd, c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_gate.weight",   c.n_embd, c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "ffn_down.weight",    c.n_ff,   c.n_embd, 1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "ffn_gate.weight",    c.n_embd, c.n_ff,   1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "ffn_up.weight",      c.n_embd, c.n_ff,   1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "ffn_latent.weight",  c.n_embd, c.n_ff,   1, 2, GGML_TYPE_F16 });
        c.tensors.push_back({ p + "attn_norm.weight",   c.n_embd, 1,        1, 1, GGML_TYPE_F32  });
        if (experts) {
            c.tensors.push_back({ p + "ffn_down_exps.weight", c.n_ff,   c.n_embd, c.n_expert, 3, GGML_TYPE_F16 });
            c.tensors.push_back({ p + "ffn_up_exps.weight",   c.n_embd, c.n_ff,   c.n_expert, 3, GGML_TYPE_F16 });
            c.tensors.push_back({ p + "ffn_gate_exps.weight", c.n_embd, c.n_ff,   c.n_expert, 3, GGML_TYPE_F16 });
        }
    }
}

// Tensor names that exercise the allow-cut and categorizer rules one by one.
static const char * kSpecialNames[] = {
    "blk.0.attn_q.weight",
    "blk.0.attn_k.weight",
    "blk.0.attn_v.weight",
    "blk.0.attn_qkv.weight",
    "blk.0.attn_kv_b.weight",
    "blk.0.attn_output.weight",
    "blk.0.attn_gate.weight",
    "blk.0.ffn_up.weight",
    "blk.0.ffn_gate.weight",
    "blk.0.ffn_down.weight",
    "blk.0.ffn_latent.weight",
    "blk.0.ffn_gate_inp.weight",
    "blk.0.ffn_gate_tid2eid.weight",
    "blk.0.altup_proj.weight",
    "blk.0.laurel.proj.weight",
    "blk.0.per_layer_model_proj.weight",
    "blk.0.ssm_conv1d.weight",
    "blk.0.shortconv.conv.weight",
    "blk.0.indexer.k_proj.weight",
    "blk.0.indexer.q_proj.weight",
    "blk.0.time_mix_first.weight",
    "blk.0.time_mix_w0.weight",
    "blk.0.time_mix_w1.weight",
    "blk.0.time_mix_w2.weight",
    "blk.0.time_mix_v0.weight",
    "blk.0.time_mix_v1.weight",
    "blk.0.time_mix_v2.weight",
    "blk.0.time_mix_a0.weight",
    "blk.0.time_mix_a1.weight",
    "blk.0.time_mix_a2.weight",
    "blk.0.time_mix_g1.weight",
    "blk.0.time_mix_g2.weight",
    "blk.0.time_mix_decay_w1.weight",
    "blk.0.time_mix_decay_w2.weight",
    "blk.0.time_mix_lerp_fused.weight",
    "blk.0.attn_rel_b.weight",
    "blk.0.position_embd.weight",
    "sam.pos_embd.weight",
    "sam.neck.proj.weight",
    "sam.net_2.weight",
    "blk.0.rel_pos.weight",
    "blk.0.patch_embd.weight",
    "blk.0.patch_merger.weight",
    "blk.0.a.rvq.codebook.weight",
    "mm.a.code_embd.weight",
    "blk.0.attn_norm.weight",
    "blk.0.attn_q.bias",
    "blk.0.mystery",
    "blk.0.mystery.weight",
};

static void add_special_names(Case & c) {
    for (const char * n : kSpecialNames) {
        const bool one_d = std::strcmp(n, "blk.0.attn_norm.weight") == 0;
        if (one_d) {
            c.tensors.push_back({ n, c.n_embd, 1, 1, 1, GGML_TYPE_F32 });
        } else {
            c.tensors.push_back({ n, c.n_embd, c.n_embd, 1, 2, GGML_TYPE_F16 });
        }
    }
    c.tensors.push_back({ "blk.0.ffn_down_exps.weight", c.n_ff,   c.n_embd, c.n_expert, 3, GGML_TYPE_F16 });
    c.tensors.push_back({ "blk.0.ffn_up_exps.weight",   c.n_embd, c.n_ff,   c.n_expert, 3, GGML_TYPE_F16 });
    c.tensors.push_back({ "blk.0.ffn_gate_exps.weight", c.n_embd, c.n_ff,   c.n_expert, 3, GGML_TYPE_F16 });
}

static std::vector<Case> make_cases() {
    std::vector<Case> cs;

    // F: every ftype on one dense qwen-like shape. NVFP4 is excluded: this
    // llama.cpp revision has no LLAMA_FTYPE_MOSTLY_NVFP4 case in
    // llama_ftype_get_default_type, so the native call gets GGML_TYPE_COUNT.
    for (llama_ftype f : kFtypes) {
        if (f == LLAMA_FTYPE_MOSTLY_NVFP4) {
            continue;
        }
        Case c;
        c.id = "ftype/" + ftype_tag(f);
        c.ftype = f;
        add_dense(c, false);
        cs.push_back(c);
    }

    // G: every ftype x n_gqa (n_head = 8, n_head_kv = 8/4/2/1).
    for (llama_ftype f : kFtypes) {
        if (f == LLAMA_FTYPE_MOSTLY_NVFP4) {
            continue;
        }
        for (int kv : { 1, 2, 4, 8 }) {
            Case c;
            c.id = "gqa/" + ftype_tag(f) + "/kv" + std::to_string(kv);
            c.ftype = f;
            c.n_head_kv = kv;
            add_dense(c, false);
            cs.push_back(c);
        }
    }

    // E: every ftype x n_expert (2, 4 and the special 8).
    for (llama_ftype f : kFtypes) {
        if (f == LLAMA_FTYPE_MOSTLY_NVFP4) {
            continue;
        }
        for (int ne : { 2, 4, 8 }) {
            Case c;
            c.id = "expert/" + ftype_tag(f) + "/e" + std::to_string(ne);
            c.ftype = f;
            c.n_expert = ne;
            add_dense(c, true);
            cs.push_back(c);
        }
    }

    // L: layer-index branches over n_layer.
    static const llama_ftype layer_ftypes[] = {
        LLAMA_FTYPE_MOSTLY_Q2_K,   LLAMA_FTYPE_MOSTLY_Q2_K_S,
        LLAMA_FTYPE_MOSTLY_Q3_K_S, LLAMA_FTYPE_MOSTLY_Q3_K_M,
        LLAMA_FTYPE_MOSTLY_Q3_K_L, LLAMA_FTYPE_MOSTLY_Q4_K_S,
        LLAMA_FTYPE_MOSTLY_Q4_K_M, LLAMA_FTYPE_MOSTLY_Q5_K_M,
        LLAMA_FTYPE_MOSTLY_IQ3_XS, LLAMA_FTYPE_MOSTLY_IQ3_XXS,
        LLAMA_FTYPE_MOSTLY_IQ3_M,  LLAMA_FTYPE_MOSTLY_IQ4_NL,
        LLAMA_FTYPE_MOSTLY_IQ4_XS, LLAMA_FTYPE_MOSTLY_Q4_0,
        LLAMA_FTYPE_MOSTLY_Q5_0,   LLAMA_FTYPE_MOSTLY_IQ2_XXS,
    };
    for (llama_ftype f : layer_ftypes) {
        for (int nl : { 1, 2, 8, 16, 32 }) {
            Case c;
            c.id = "layer/" + ftype_tag(f) + "/l" + std::to_string(nl);
            c.ftype = f;
            c.n_layer = nl;
            add_dense(c, false);
            cs.push_back(c);
        }
    }

    // S: shapes that fit and shapes that force the fallback table. Q1_0, Q2_0
    // and NVFP4 only get shapes divisible by their real block size: the native
    // fallback throws for them and the port has no exceptions.
    struct Shape { int embd, ff; };
    static const Shape shapes[] = {
        { 2048, 8192 }, { 288, 800 }, { 96, 800 }, { 512, 2048 }, { 128, 512 }, { 1024, 4096 },
    };
    for (llama_ftype f : kFtypes) {
        if (f == LLAMA_FTYPE_MOSTLY_NVFP4) {
            continue;
        }
        const bool strict = f == LLAMA_FTYPE_MOSTLY_Q1_0 || f == LLAMA_FTYPE_MOSTLY_Q2_0;
        for (const Shape & sh : shapes) {
            if (strict && (sh.embd % 128 != 0 || sh.ff % 128 != 0 ||
                           (sh.embd * 2 / 8) % 128 != 0)) {
                continue;
            }
            Case c;
            c.id = "shape/" + ftype_tag(f) + "/" + std::to_string(sh.embd) + "x" + std::to_string(sh.ff);
            c.ftype = f;
            c.n_embd = sh.embd;
            c.n_ff = sh.ff;
            add_dense(c, false);
            cs.push_back(c);
        }
    }

    // O: public overrides.
    static const int token_types[] = {
        (int) GGML_TYPE_COUNT, (int) GGML_TYPE_F16, (int) GGML_TYPE_BF16,
        (int) GGML_TYPE_Q8_0, (int) GGML_TYPE_Q6_K, (int) GGML_TYPE_IQ2_XS,
        (int) GGML_TYPE_Q2_K,
    };
    static const int output_types[] = {
        (int) GGML_TYPE_COUNT, (int) GGML_TYPE_F16, (int) GGML_TYPE_Q8_0,
        (int) GGML_TYPE_Q6_K, (int) GGML_TYPE_IQ2_XS, (int) GGML_TYPE_Q4_K,
    };
    static const llama_ftype ov_ftypes[] = {
        LLAMA_FTYPE_MOSTLY_Q4_K_M, LLAMA_FTYPE_MOSTLY_Q2_K, LLAMA_FTYPE_MOSTLY_IQ3_XXS,
    };
    for (llama_ftype f : ov_ftypes) {
        for (int tt : token_types) {
            for (int ot : output_types) {
                Case c;
                c.id = "override/" + ftype_tag(f) + "/t" + std::to_string(tt) + "/o" + std::to_string(ot);
                c.ftype = f;
                c.token_type = tt;
                c.output_type = ot;
                add_dense(c, false);
                cs.push_back(c);
            }
        }
    }
    for (llama_ftype f : { LLAMA_FTYPE_MOSTLY_Q4_K_M, LLAMA_FTYPE_MOSTLY_Q2_K,
                           LLAMA_FTYPE_MOSTLY_IQ3_XXS, LLAMA_FTYPE_MOSTLY_Q4_0 }) {
        for (int qo = 0; qo < 2; ++qo) {
            for (int oc = 0; oc < 2; ++oc) {
                for (int pu = 0; pu < 2; ++pu) {
                    Case c;
                    c.id = "flags/" + ftype_tag(f) + "/q" + std::to_string(qo) +
                           "c" + std::to_string(oc) + "p" + std::to_string(pu);
                    c.ftype = f;
                    c.quantize_output = qo;
                    c.only_copy = oc;
                    c.pure = pu;
                    add_dense(c, false);
                    cs.push_back(c);
                }
            }
        }
    }

    // N: the categorizer and allow-cut names.
    for (llama_ftype f : { LLAMA_FTYPE_MOSTLY_Q4_K_M, LLAMA_FTYPE_MOSTLY_Q2_K,
                           LLAMA_FTYPE_MOSTLY_IQ3_XXS, LLAMA_FTYPE_MOSTLY_MXFP4_MOE,
                           LLAMA_FTYPE_MOSTLY_Q3_K_M }) {
        Case c;
        c.id = "names/" + ftype_tag(f);
        c.ftype = f;
        c.n_expert = 8;
        add_special_names(c);
        cs.push_back(c);
    }

    // N1D: every name variant with a 1-D shape (all must be rejected).
    {
        Case c;
        c.id = "names/one-dim";
        c.n_expert = 8;
        c.tensors.push_back({ "blk.0.attn_v.weight",     c.n_embd, 1, 1, 1, GGML_TYPE_F16 });
        c.tensors.push_back({ "blk.0.ffn_down.weight",   c.n_ff,   1, 1, 1, GGML_TYPE_F16 });
        c.tensors.push_back({ "output.weight",           c.n_embd, 1, 1, 1, GGML_TYPE_F16 });
        c.tensors.push_back({ "token_embd.weight",       c.n_embd, 1, 1, 1, GGML_TYPE_F16 });
        c.tensors.push_back({ "blk.0.attn_qkv.weight",   c.n_embd, 1, 1, 1, GGML_TYPE_F16 });
        cs.push_back(c);
    }

    // OS: the output/tied branch reads the real block size of the default type.
    // Q1_0 (128) and Q2_0 (64) are driven with ne0 divisible by 32 but not by
    // their block, so a wrong block table changes Q8_0 into Q6_K. Only
    // output.weight is present so the native fallback cannot throw.
    for (llama_ftype f : { LLAMA_FTYPE_MOSTLY_Q1_0, LLAMA_FTYPE_MOSTLY_Q2_0 }) {
        for (int ne : { 96, 160, 192 }) {
            Case c;
            c.id = "output-shape/" + ftype_tag(f) + "/n" + std::to_string(ne);
            c.ftype = f;
            c.n_embd = ne;
            c.n_ff = ne * 4;
            c.tensors.push_back({ "output.weight", ne, 32000, 1, 2, GGML_TYPE_F16 });
            cs.push_back(c);
        }
    }

    return cs;
}

static void run_case(const Case & c) {
    ++g_cases;
    const size_t n = c.tensors.size();

    struct ggml_init_params ip = { (size_t) (n + 8) * ggml_tensor_overhead(), nullptr, true };
    struct ggml_context * ctx = ggml_init(ip);
    if (!ctx) {
        fail(c.id, "ggml_init failed");
        return;
    }

    std::vector<ggml_tensor *> tensors;
    tensors.reserve(n);
    for (const auto & s : c.tensors) {
        ggml_tensor * t = s.ndims == 1
            ? ggml_new_tensor_1d(ctx, s.orig, s.ne0)
            : (s.ndims == 3 ? ggml_new_tensor_3d(ctx, s.orig, s.ne0, s.ne1, s.ne2)
                            : ggml_new_tensor_2d(ctx, s.orig, s.ne0, s.ne1));
        ggml_set_name(t, s.name.c_str());
        tensors.push_back(t);
    }

    llama_quant_model_desc desc = {};
    desc.architecture  = "qwen3";
    desc.n_embd        = c.n_embd;
    desc.n_ff          = c.n_ff;
    desc.n_layer       = c.n_layer;
    desc.n_head        = c.n_head;
    desc.n_head_kv     = c.n_head_kv;
    desc.n_expert      = c.n_expert;
    desc.n_embd_head_k = c.n_embd_head_k;
    desc.n_embd_head_v = c.n_embd_head_v;

    llama_model * model = llama_quant_model_from_metadata(&desc);
    if (!model) {
        fail(c.id, "llama_quant_model_from_metadata failed");
        ggml_free(ctx);
        return;
    }

    llama_model_quantize_params qp = llama_model_quantize_default_params();
    qp.ftype                  = c.ftype;
    qp.token_embedding_type   = (ggml_type) c.token_type;
    qp.output_tensor_type     = (ggml_type) c.output_type;
    qp.quantize_output_tensor = c.quantize_output != 0;
    qp.only_copy              = c.only_copy != 0;
    qp.pure                   = c.pure != 0;

    quantize_state_impl * qs = llama_quant_init(model, &qp);

    std::vector<ggml_type> native_types(n);
    try {
        llama_quant_compute_types(qs, c.ftype, tensors.data(), native_types.data(), n);
    } catch (const std::exception & e) {
        fail(c.id + "/native-throw", e.what());
        llama_quant_free(qs);
        llama_model_free(model);
        ggml_free(ctx);
        return;
    }
    const int native_fb = qs->n_fallback;

    std::string names;
    std::vector<int64_t> off(n), len(n), ne0(n), ne1(n), ne2(n), ndims(n), otypes(n);
    std::vector<int64_t> out(n, -1);
    for (size_t i = 0; i < n; ++i) {
        off[i]   = (int64_t) names.size();
        len[i]   = (int64_t) c.tensors[i].name.size();
        names   += c.tensors[i].name;
        ne0[i]   = c.tensors[i].ne0;
        ne1[i]   = c.tensors[i].ne1;
        ne2[i]   = c.tensors[i].ne2;
        ndims[i] = c.tensors[i].ndims;
        otypes[i] = (int64_t) c.tensors[i].orig;
    }
    const int64_t n_gqa = c.n_head_kv == 0 ? 0 : (int64_t) c.n_head / c.n_head_kv;
    const int64_t port_fb = qt_compute(
        sl(names), sli(off), sli(len), sli(ne0), sli(ne1), sli(ne2),
        sli(ndims), sli(otypes), (int64_t) n, (int64_t) c.ftype,
        (int64_t) c.n_layer, (int64_t) c.n_expert, n_gqa,
        (int64_t) c.token_type, (int64_t) c.output_type,
        (int64_t) c.quantize_output, (int64_t) c.only_copy,
        (int64_t) c.pure, 0, sli(out));

    for (size_t i = 0; i < n; ++i) {
        ++g_values;
        if ((int64_t) native_types[i] != out[i]) {
            fail(c.id, c.tensors[i].name + ": native=" + std::to_string((int) native_types[i]) +
                      " port=" + std::to_string((int) out[i]));
        }
    }
    ++g_values;
    if ((int64_t) native_fb != port_fb) {
        fail(c.id + "/fallback", "native=" + std::to_string(native_fb) +
                                 " port=" + std::to_string(port_fb));
    }

    llama_quant_free(qs);
    llama_model_free(model);
    ggml_free(ctx);
}

// qt_default_type is not exported; the ftype table is covered indirectly by
// llama_quant_compute_types for every ftype (ftype/* cases).

static void run_policies() {
    static const char * names[] = {
        "token_embd.weight",
        "per_layer_token_embd.weight",
        "output.weight",
        "blk.0.attn_v.weight",
        "blk.7.ffn_down_exps.weight",
        "blk.0.ffn_up.weight",
        "blk.0.attn_q.weight",
        "output_norm.weight",
    };
    const int n_names = (int) (sizeof(names) / sizeof(names[0]));

    for (int t = 0; t < (int) GGML_TYPE_COUNT; ++t) {
        ++g_values;
        const int native = ggml_quantize_requires_imatrix((ggml_type) t) ? 1 : 0;
        const int port   = (int) qt_ggml_quantize_requires_imatrix(t);
        if (native != port) {
            fail("ggml_policy/type" + std::to_string(t),
                 "native=" + std::to_string(native) + " port=" + std::to_string(port));
        }
    }

    for (llama_ftype f : kFtypes) {
        for (int t = 0; t < (int) GGML_TYPE_COUNT; ++t) {
            for (int ni = 0; ni < n_names; ++ni) {
                ++g_values;
                std::string nm(names[ni]);
                const int native = tensor_requires_imatrix(names[ni], (ggml_type) t, f) ? 1 : 0;
                const int port   = (int) qt_tensor_requires_imatrix(sl(nm), (int64_t) t, (int64_t) f);
                if (native != port) {
                    fail("tensor_policy/f" + std::to_string((int) f) + "/t" + std::to_string(t),
                         std::string(names[ni]) + " native=" + std::to_string(native) +
                         " port=" + std::to_string(port));
                }
            }
        }
    }
}

int main(int argc, char ** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    goose_init(argc, argv);
    const std::vector<Case> cases = make_cases();
    for (const auto & c : cases) {
        run_case(c);
    }
    run_policies();
    printf("FG584_QUANT cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
