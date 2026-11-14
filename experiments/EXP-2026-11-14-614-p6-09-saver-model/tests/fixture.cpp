// Differential gate for P6-09 S5 / P7-02 S5, llama_model_saver's model path.
//
// One binary, two implementations of the same synthetic model:
//   - native: llama_model_saver (src/llama-model-saver.cpp) over libllama,
//     add_kv_from_model + add_tensors_from_model on a TestModel;
//   - port:   savm_* from source/model-saver.goose over the accepted GGUF
//     writer/tensors port (S1-S4).
//
// There are no weights in this repository, so the model is synthetic:
//   - hparams are values installed on TestModel.hparams;
//   - the vocabulary is a real llama_vocab loaded by the real loader from a
//     tiny GGUF written by this fixture (tokens, scores, token types, merges,
//     special ids); a vocab-less case uses tokenizer.ggml.model = "no_vocab";
//   - tensors are empty ggml tensors with names and shapes; only descriptors
//     are compared, the data section belongs to the accepted S4 gate.
//
// Both sides receive the same values. Every case compares:
//   - the canonical KV dump (key, type, array element type, count, bytes);
//   - the tensor descriptor dump (name, type, offset, dims, ne[0..3]).
// supports_arch is compared over every llm_arch value.
//
// Output: FG_S5 cases=N checks=M failures=K
// Exit:   0 ok, 1 failures, 3 native setup failed.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <functional>
#include <stdexcept>

#include "ggml.h"
#include "gguf.h"
#include "llama.h"
#include "llama-arch.h"
#include "llama-model.h"
#include "llama-hparams.h"
#include "llama-vocab.h"
#include "llama-model-loader.h"
#include "llama-model-saver.h"

#include "model_saver.h"
#include "slots.h"

// The port's f32 bit accessor (declared extern in the Goose source).
extern "C" int64_t ms_bits(float v) {
    uint32_t b = 0;
    std::memcpy(&b, &v, 4);
    return (int64_t) b;
}

static sl_i64 A(int64_t * p, int64_t n) { return sl_i64{ p, n }; }
static sl_i64 AC(const int64_t * p, int64_t n) { return sl_i64{ (int64_t *) p, n }; }
static sl_u8  S(uint8_t * p, int64_t n) { return sl_u8{ p, n }; }
static sl_u8  SC(const uint8_t * p, int64_t n) { return sl_u8{ (uint8_t *) p, n }; }
static sl_f32 F(float * p, int64_t n) { return sl_f32{ p, n }; }
static sl_f32 FC(const float * p, int64_t n) { return sl_f32{ (float *) p, n }; }

static int checks = 0;
static int fails = 0;
static int cases = 0;
static std::string g_work = ".";

static void fail(const std::string & cs, const std::string & what) {
    fails++;
    std::printf("FAIL %s: %s\n", cs.c_str(), what.c_str());
}

static void expect(bool ok, const std::string & cs, const std::string & what) {
    checks++;
    if (!ok) {
        fail(cs, what);
    }
}

// ---------------------------------------------------------------- test model

struct TestModel : llama_model {
    explicit TestModel(const llama_model_params & params) : llama_model(params) {}

    void load_stats  (llama_model_loader &) override {}
    void load_hparams(llama_model_loader &) override {}
    void load_vocab  (llama_model_loader &) override {}
    bool load_tensors(llama_model_loader &) override { return false; }

    void load_arch_hparams(llama_model_loader &) override {}
    void load_arch_tensors(llama_model_loader &) override {}
    std::unique_ptr<llm_graph_context> build_arch_graph(const llm_graph_params &) const override {
        return nullptr;
    }
};

// ---------------------------------------------------------------- scenarios

struct TokSpec {
    std::string text;
    float       score = 0.0f;
    int32_t     type  = LLAMA_TOKEN_TYPE_NORMAL;
};

struct VocabSpec {
    std::string              model = "llama";
    std::string              pre   = "default";
    std::vector<TokSpec>     toks;
    std::vector<std::string> merges;
    bool                     with_types = true;
    int32_t                  type_count = 7;
    int32_t                  bos = -1, eos = -1, eot = -1, eom = -1;
    int32_t                  unk = -1, sep = -1, pad = -1;
    int32_t                  fim_pre = -1, fim_suf = -1, fim_mid = -1;
    int32_t                  fim_pad = -1, fim_rep = -1, fim_sep = -1;
    int                      add_bos = -1, add_eos = -1, add_sep = -1; // -1: key absent
    int                      add_prefix = -1, remove_ws = -1;
    int32_t                  vocab_size = 0; // no_vocab only
};

struct TSpec {
    std::string name;
    int         type = GGML_TYPE_F32;
    int64_t     ne[4] = {2, 1, 1, 1};
};

enum LField {
    LF_ATTN_NORM, LF_WQ, LF_WK, LF_WV, LF_WO,
    LF_FFN_NORM, LF_FFN_UP, LF_FFN_DOWN, LF_ATTN_Q_NORM,
};

struct Case {
    std::string                       name;
    llm_arch                          arch = LLM_ARCH_LLAMA;
    std::string                       model_name = "synthetic";
    std::vector<std::string>          labels;
    std::vector<int64_t>              prec;  // raw ggml_prec values, map order
    std::function<void(llama_hparams&)> hp;
    VocabSpec                         vocab;
    std::vector<TSpec>                tensors;
    std::vector<std::pair<int,int>>   globals;      // (G_* slot, tensor idx)
    std::vector<std::pair<int,int>>   layer_t;      // (layer*LF_COUNT + field, idx)
    bool                              port_only = false;
    int64_t                           charmap_len = 0;
    int64_t                           expect_rc = 0;
};

static std::vector<Case> g_cases;

static Case & add_case(const char * name) {
    g_cases.emplace_back();
    g_cases.back().name = name;
    return g_cases.back();
}

static void default_hparams(llama_hparams & h) {
    h.n_ctx_train = 256;
    h.n_embd = 64;
    h.n_layer_all = 2;
    h.n_layer_nextn = 0;
    h.n_layer_dense_lead = 0;
    for (int i = 0; i < LLAMA_MAX_LAYERS; ++i) {
        h.n_ff_arr[i] = 128;
        h.n_ff_exp_arr[i] = 64;
        h.n_expert_used_arr[i] = 1;
        h.n_head_arr[i] = 4;
        h.n_head_kv_arr[i] = 2;
        h.rope_pattern[i] = 1;
        h.is_swa_impl[i] = 0;
        h.is_recr_impl[i] = 0;
        h.is_indexer_full_impl[i] = 0;
        h.dsv4_compress_ratios[i] = 0;
        h.deepstack_mapping_arr[i] = -1;
        h.xielu_alpha_n[i] = 1.0f;
        h.xielu_alpha_p[i] = 0.5f;
        h.xielu_beta[i] = 2.0f;
        h.xielu_eps[i] = -1e-6f;
        h.swiglu_clamp_exp[i] = 0.0f;
        h.swiglu_clamp_shexp[i] = 0.0f;
        h.ple_layer_multipliers[i] = 0;
        h.ple_head_offsets[i] = 0;
        h.ple_head_vocab_sizes[i] = 0;
        h.is_ple_impl[i] = 0;
    }
    h.n_embd_head_k_full = 16;
    h.n_embd_head_v_full = 16;
    h.n_rot_full = 16;
    h.rope_freq_base_train = 10000.0f;
    h.rope_freq_scale_train = 1.0f;
    h.n_ctx_orig_yarn = 256;
    h.rope_sections = {0, 0, 0, 0};
    h.f_norm_eps = 1e-5f;
    h.f_norm_rms_eps = 1e-6f;
    h.n_swa = 0;
    h.swa_type = LLAMA_SWA_TYPE_NONE;
    h.dec_start_token_id = LLAMA_TOKEN_NULL;
    h.causal_attn = true;
    h.token_shift_count = 2;
    h.llm_ffn_op = LLM_FFN_SILU;
    h.rope_section_order = LLAMA_ROPE_SECTION_ORDER_TYXZ;
    h.rope_scaling_type_train = LLAMA_ROPE_SCALING_TYPE_NONE;
}

static const TokSpec * tok_at(const VocabSpec & v, int i) {
    if (i < 0 || i >= (int) v.toks.size()) {
        return nullptr;
    }
    return &v.toks[i];
}

// ---------------------------------------------------------------- vocab file

static std::string kvv(const LLM_KV & kv, llm_kv id) {
    return kv(id);
}

static void set_opt_u32(struct gguf_context * g, const LLM_KV & kv, llm_kv id, int32_t v) {
    if (v >= 0) {
        gguf_set_val_u32(g, kvv(kv, id).c_str(), (uint32_t) v);
    }
}

static void set_opt_bool(struct gguf_context * g, const LLM_KV & kv, llm_kv id, int v) {
    if (v >= 0) {
        gguf_set_val_bool(g, kvv(kv, id).c_str(), v != 0);
    }
}

static void write_vocab_file(const VocabSpec & v, const std::string & path) {
    struct gguf_context * g = gguf_init_empty();
    const LLM_KV kv(LLM_ARCH_LLAMA);

    gguf_set_val_str(g, kvv(kv, LLM_KV_GENERAL_ARCHITECTURE).c_str(), "llama");
    gguf_set_val_str(g, kvv(kv, LLM_KV_TOKENIZER_MODEL).c_str(), v.model.c_str());
    if (!v.pre.empty()) {
        gguf_set_val_str(g, kvv(kv, LLM_KV_TOKENIZER_PRE).c_str(), v.pre.c_str());
    }

    std::vector<const char *> words;
    std::vector<float>        scores;
    std::vector<int32_t>      types;
    for (const TokSpec & t : v.toks) {
        words.push_back(t.text.c_str());
        scores.push_back(t.score);
        types.push_back(t.type);
    }
    gguf_set_arr_str(g, kvv(kv, LLM_KV_TOKENIZER_LIST).c_str(),
                     words.empty() ? nullptr : words.data(), (int) words.size());
    gguf_set_arr_data(g, kvv(kv, LLM_KV_TOKENIZER_SCORES).c_str(), GGUF_TYPE_FLOAT32,
                      scores.empty() ? nullptr : scores.data(), (int) scores.size());
    if (v.with_types) {
        gguf_set_arr_data(g, kvv(kv, LLM_KV_TOKENIZER_TOKEN_TYPE).c_str(), GGUF_TYPE_INT32,
                          types.empty() ? nullptr : types.data(), (int) types.size());
        gguf_set_val_u32(g, kvv(kv, LLM_KV_TOKENIZER_TOKEN_TYPE_COUNT).c_str(),
                         (uint32_t) v.type_count);
    }
    if (!v.merges.empty()) {
        std::vector<const char *> ms;
        for (const std::string & m : v.merges) {
            ms.push_back(m.c_str());
        }
        gguf_set_arr_str(g, kvv(kv, LLM_KV_TOKENIZER_MERGES).c_str(), ms.data(), (int) ms.size());
    }
    if (v.vocab_size > 0) {
        gguf_set_val_u32(g, kvv(kv, LLM_KV_VOCAB_SIZE).c_str(), (uint32_t) v.vocab_size);
    }

    set_opt_u32(g, kv, LLM_KV_TOKENIZER_BOS_ID, v.bos);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_EOS_ID, v.eos);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_EOT_ID, v.eot);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_EOM_ID, v.eom);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_UNK_ID, v.unk);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_SEP_ID, v.sep);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_PAD_ID, v.pad);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_FIM_PRE_ID, v.fim_pre);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_FIM_SUF_ID, v.fim_suf);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_FIM_MID_ID, v.fim_mid);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_FIM_PAD_ID, v.fim_pad);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_FIM_REP_ID, v.fim_rep);
    set_opt_u32(g, kv, LLM_KV_TOKENIZER_FIM_SEP_ID, v.fim_sep);
    set_opt_bool(g, kv, LLM_KV_TOKENIZER_ADD_BOS, v.add_bos);
    set_opt_bool(g, kv, LLM_KV_TOKENIZER_ADD_EOS, v.add_eos);
    set_opt_bool(g, kv, LLM_KV_TOKENIZER_ADD_SEP, v.add_sep);
    set_opt_bool(g, kv, LLM_KV_TOKENIZER_ADD_PREFIX, v.add_prefix);
    set_opt_bool(g, kv, LLM_KV_TOKENIZER_REMOVE_EXTRA_WS, v.remove_ws);

    gguf_write_to_file(g, path.c_str(), false);
    gguf_free(g);
}

// ---------------------------------------------------------------- dump

static std::string to_hex(const uint8_t * p, size_t n) {
    static const char * d = "0123456789abcdef";
    std::string s;
    s.reserve(n * 2);
    for (size_t i = 0; i < n; ++i) {
        s.push_back(d[p[i] >> 4]);
        s.push_back(d[p[i] & 15]);
    }
    return s;
}

static size_t gguf_elem_size(gguf_type t) {
    switch (t) {
        case GGUF_TYPE_UINT8:   case GGUF_TYPE_INT8:  case GGUF_TYPE_BOOL: return 1;
        case GGUF_TYPE_UINT16:  case GGUF_TYPE_INT16: return 2;
        case GGUF_TYPE_UINT32:  case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: return 4;
        case GGUF_TYPE_UINT64:  case GGUF_TYPE_INT64: case GGUF_TYPE_FLOAT64: return 8;
        default: return 0;
    }
}

static uint64_t gguf_scalar_bits(struct gguf_context * g, int i, gguf_type t) {
    switch (t) {
        case GGUF_TYPE_UINT8:  return (uint8_t)  gguf_get_val_u8 (g, i);
        case GGUF_TYPE_INT8:   return (uint8_t)  gguf_get_val_i8 (g, i);
        case GGUF_TYPE_UINT16: return (uint16_t) gguf_get_val_u16(g, i);
        case GGUF_TYPE_INT16:  return (uint16_t) gguf_get_val_i16(g, i);
        case GGUF_TYPE_UINT32: return (uint32_t) gguf_get_val_u32(g, i);
        case GGUF_TYPE_INT32:  return (uint32_t) gguf_get_val_i32(g, i);
        case GGUF_TYPE_FLOAT32:{ float f = gguf_get_val_f32(g, i); uint32_t b; std::memcpy(&b, &f, 4); return b; }
        case GGUF_TYPE_BOOL:   return (uint8_t) (gguf_get_val_bool(g, i) ? 1 : 0);
        case GGUF_TYPE_UINT64: return (uint64_t) gguf_get_val_u64(g, i);
        case GGUF_TYPE_INT64:  return (uint64_t) gguf_get_val_i64(g, i);
        case GGUF_TYPE_FLOAT64:{ double f = gguf_get_val_f64(g, i); uint64_t b; std::memcpy(&b, &f, 8); return b; }
        default: return 0;
    }
}

static std::vector<std::string> dump_native_kv(struct gguf_context * g) {
    std::vector<std::string> out;
    const int n = (int) gguf_get_n_kv(g);
    for (int i = 0; i < n; ++i) {
        const char * key = gguf_get_key(g, i);
        const gguf_type t = gguf_get_kv_type(g, i);
        char head[160];
        if (t == GGUF_TYPE_ARRAY) {
            const gguf_type at = gguf_get_arr_type(g, i);
            const int an = (int) gguf_get_arr_n(g, i);
            std::snprintf(head, sizeof head, "KV %s t=%d a=%d n=%d ", key, (int) t, (int) at, an);
            std::string line = head;
            if (at == GGUF_TYPE_STRING) {
                for (int j = 0; j < an; ++j) {
                    const char * s = gguf_get_arr_str(g, i, j);
                    const size_t l = std::strlen(s);
                    line += std::to_string(l) + ":" + to_hex((const uint8_t *) s, l) + ";";
                }
            } else {
                const uint8_t * p = (const uint8_t *) gguf_get_arr_data(g, i);
                line += to_hex(p, gguf_elem_size(at) * (size_t) an);
            }
            out.push_back(line);
        } else if (t == GGUF_TYPE_STRING) {
            const char * s = gguf_get_val_str(g, i);
            std::snprintf(head, sizeof head, "KV %s t=%d ", key, (int) t);
            out.push_back(std::string(head) + std::to_string(std::strlen(s)) + ":" +
                          to_hex((const uint8_t *) s, std::strlen(s)));
        } else {
            std::snprintf(head, sizeof head, "KV %s t=%d v=%llx", key, (int) t,
                          (unsigned long long) gguf_scalar_bits(g, i, t));
            out.push_back(head);
        }
    }
    return out;
}

// port side ---------------------------------------------------------------

struct PortState {
    std::vector<int64_t> st = std::vector<int64_t>(8);
    std::vector<uint8_t> pool = std::vector<uint8_t>(1 << 20);
    std::vector<int64_t> key_off = std::vector<int64_t>(512);
    std::vector<int64_t> key_len = std::vector<int64_t>(512);
    std::vector<int64_t> types = std::vector<int64_t>(512);
    std::vector<int64_t> arr = std::vector<int64_t>(512);
    std::vector<int64_t> v0 = std::vector<int64_t>(512);
    std::vector<int64_t> v1 = std::vector<int64_t>(512);
    std::vector<int64_t> v2 = std::vector<int64_t>(512);
    std::vector<int64_t> tst = std::vector<int64_t>(8);
    std::vector<uint8_t> tpool = std::vector<uint8_t>(1 << 20);
    std::vector<int64_t> tne = std::vector<int64_t>(512 * 4);
    std::vector<int64_t> trec = std::vector<int64_t>(512 * 8);
    std::vector<int64_t> out = std::vector<int64_t>(2);

    // key table: resolved LLM_KV names in slot order
    std::vector<int64_t> k_off = std::vector<int64_t>(SLOT_NKEY);
    std::vector<int64_t> k_len = std::vector<int64_t>(SLOT_NKEY);
    std::vector<uint8_t> k_pool;

    // resolved string table
    std::vector<std::string> strings;
    std::vector<int64_t> s_off, s_len;
    std::vector<uint8_t> s_pool;

    std::vector<int64_t> ix = std::vector<int64_t>(32);
    std::vector<int64_t> hv = std::vector<int64_t>(512);
    std::vector<float>   hf = std::vector<float>(256);
    std::vector<int64_t> ha;
    std::vector<float>   haf;
    std::vector<int64_t> attrs;
    std::vector<float>   scores;
    std::vector<int64_t> prec;
    std::vector<uint8_t> scratch = std::vector<uint8_t>(1 << 20);
};

static void build_key_table(PortState & ps, llm_arch arch) {
    for (int i = 0; i < SLOT_NKEY; ++i) {
        const std::string s = LLM_KV(arch)(SLOT_KEY[i]);
        ps.k_off[i] = (int64_t) ps.k_pool.size();
        ps.k_len[i] = (int64_t) s.size();
        ps.k_pool.insert(ps.k_pool.end(), s.begin(), s.end());
    }
}

static void add_string(PortState & ps, const std::string & s, int & slot) {
    slot = (int) ps.strings.size();
    ps.strings.push_back(s);
}

static void finalize_strings(PortState & ps) {
    ps.s_off.resize(ps.strings.size());
    ps.s_len.resize(ps.strings.size());
    for (size_t i = 0; i < ps.strings.size(); ++i) {
        ps.s_off[i] = (int64_t) ps.s_pool.size();
        ps.s_len[i] = (int64_t) ps.strings[i].size();
        ps.s_pool.insert(ps.s_pool.end(), ps.strings[i].begin(), ps.strings[i].end());
    }
}

static uint64_t port_scalar_bits(PortState & ps, int i, int t) {
    const uint64_t v = (uint64_t) ps.v0[i];
    switch (t) {
        case GGUF_TYPE_UINT8: case GGUF_TYPE_INT8: case GGUF_TYPE_BOOL: return v & 0xffu;
        case GGUF_TYPE_UINT16: case GGUF_TYPE_INT16: return v & 0xffffu;
        case GGUF_TYPE_UINT32: case GGUF_TYPE_INT32: case GGUF_TYPE_FLOAT32: return v & 0xffffffffu;
        default: return v;
    }
}

static std::vector<std::string> dump_port_kv(PortState & ps) {
    std::vector<std::string> out;
    const int n = (int) ps.st[0];
    for (int i = 0; i < n; ++i) {
        const std::string key((const char *) ps.pool.data() + ps.key_off[i], (size_t) ps.key_len[i]);
        const int t = (int) ps.types[i];
        char head[200];
        if (ps.arr[i] != 0) {
            const int an = (int) ps.v1[i];
            const int at = t;
            std::snprintf(head, sizeof head, "KV %s t=%d a=%d n=%d ", key.c_str(), (int) GGUF_TYPE_ARRAY, at, an);
            std::string line = head;
            const uint8_t * p = ps.pool.data() + ps.v0[i];
            if (ps.arr[i] == 2) {
                int64_t o = 0;
                for (int j = 0; j < an; ++j) {
                    size_t l = 0;
                    while (o + (int64_t) l < ps.v2[i] && p[o + l] != 0) {
                        ++l;
                    }
                    line += std::to_string(l) + ":" + to_hex(p + o, l) + ";";
                    o += (int64_t) l + 1;
                }
            } else {
                line += to_hex(p, gguf_elem_size((gguf_type) at) * (size_t) an);
            }
            out.push_back(line);
        } else if (t == GGUF_TYPE_STRING) {
            std::snprintf(head, sizeof head, "KV %s t=%d ", key.c_str(), t);
            out.push_back(std::string(head) + std::to_string(ps.v2[i]) + ":" +
                          to_hex(ps.pool.data() + ps.v0[i], (size_t) ps.v2[i]));
        } else {
            std::snprintf(head, sizeof head, "KV %s t=%d v=%llx", key.c_str(), t,
                          (unsigned long long) port_scalar_bits(ps, i, t));
            out.push_back(head);
        }
    }
    return out;
}

static std::vector<std::string> dump_port_tensors(PortState & ps) {
    std::vector<std::string> out;
    const int n = (int) ps.tst[0];
    for (int i = 0; i < n; ++i) {
        const std::string name((const char *) ps.tpool.data() + ps.trec[i * 8 + 0],
                               (size_t) ps.trec[i * 8 + 1]);
        char buf[512];
        std::snprintf(buf, sizeof buf, "T %s type=%d off=%lld dims=%lld ne=%lld,%lld,%lld,%lld",
                      name.c_str(), (int) ps.trec[i * 8 + 2], (long long) ps.trec[i * 8 + 6],
                      (long long) ps.trec[i * 8 + 3],
                      (long long) ps.tne[i * 4 + 0], (long long) ps.tne[i * 4 + 1],
                      (long long) ps.tne[i * 4 + 2], (long long) ps.tne[i * 4 + 3]);
        out.push_back(buf);
    }
    return out;
}

// ---------------------------------------------------------------- runner

struct Built {
    TestModel                  model;
    ggml_context *             ctx = nullptr;
    gguf_context *             native = nullptr;
    std::vector<ggml_tensor*>  tensors;
    PortState                  ps;
    std::string                vocab_file;

    Built() : model(llama_model_default_params()) {}
    ~Built() {
        if (native != nullptr) {
            gguf_free(native);
        }
        if (ctx != nullptr) {
            ggml_free(ctx);
        }
    }
};

static void set_layer_field(llama_layer & l, int f, ggml_tensor * t) {
    switch (f) {
        case LF_ATTN_NORM:    l.attn_norm = t; break;
        case LF_WQ:           l.wq = t; break;
        case LF_WK:           l.wk = t; break;
        case LF_WV:           l.wv = t; break;
        case LF_WO:           l.wo = t; break;
        case LF_FFN_NORM:     l.ffn_norm = t; break;
        case LF_FFN_UP:       l.ffn_up = t; break;
        case LF_FFN_DOWN:     l.ffn_down = t; break;
        case LF_ATTN_Q_NORM:  l.attn_q_norm = t; break;
        default: break;
    }
}

static void set_global(llama_model & m, int slot, ggml_tensor * t) {
    switch (slot) {
        case G_TYPE_EMBD:          m.type_embd = t; break;
        case G_POS_EMBD:           m.pos_embd = t; break;
        case G_TOK_NORM:           m.tok_norm = t; break;
        case G_TOK_NORM_B:         m.tok_norm_b = t; break;
        case G_OUTPUT_NORM:        m.output_norm = t; break;
        case G_OUTPUT_NORM_B:      m.output_norm_b = t; break;
        case G_OUTPUT:             m.output = t; break;
        case G_OUTPUT_B:           m.output_b = t; break;
        case G_OUTPUT_NORM_ENC:    m.output_norm_enc = t; break;
        case G_OUTPUT_S:           m.output_s = t; break;
        case G_OUTPUT_IN_S:        m.output_in_s = t; break;
        case G_OUTPUT_RES_SCORE:   m.output_res_score = t; break;
        case G_NEXTN_PROJ_PRE:     m.nextn_proj_pre = t; break;
        case G_NEXTN_PROJ_POST:    m.nextn_proj_post = t; break;
        case G_CLS:                m.cls = t; break;
        case G_CLS_B:              m.cls_b = t; break;
        case G_CLS_OUT:            m.cls_out = t; break;
        case G_CLS_OUT_B:          m.cls_out_b = t; break;
        case G_CLS_NORM:           m.cls_norm = t; break;
        case G_HRM_Z_L_INIT:       m.hrm_z_l_init = t; break;
        case G_HC_HEAD_FN:         m.hc_head_fn = t; break;
        case G_HC_HEAD_BASE:       m.hc_head_base = t; break;
        case G_HC_HEAD_SCALE:      m.hc_head_scale = t; break;
        case G_PER_LAYER_TOK_EMBD: m.per_layer_tok_embd = t; break;
        case G_HC_HEAD_NORM:       m.hc_head_norm = t; break;
        case G_HC_HEAD_DOWN:       m.hc_head_down = t; break;
        case G_HC_HEAD_UP:         m.hc_head_up = t; break;
        case G_TOK_EMBD:           m.tok_embd = t; break;
        default: break;
    }
}

static bool build_native(Built & b, const Case & cs, int idx) {
    llama_model & m = b.model;
    m.arch = cs.arch;
    m.name = cs.model_name;
    default_hparams(m.hparams);
    if (cs.hp) {
        cs.hp(m.hparams);
    }
    m.classifier_labels = cs.labels;

    b.vocab_file = g_work + "/vocab_" + std::to_string(idx) + ".gguf";
    write_vocab_file(cs.vocab, b.vocab_file);
    try {
        std::vector<std::string> splits;
        llama_model_loader ml(nullptr, nullptr, nullptr, b.vocab_file, splits, nullptr,
                              LLAMA_LOAD_MODE_MMAP, false, true, false, nullptr, nullptr);
        m.vocab.load(ml, LLM_KV(m.arch));
    } catch (const std::exception & e) {
        std::printf("native setup failed (%s): %s\n", cs.name.c_str(), e.what());
        return false;
    }

    const size_t overhead = (cs.tensors.size() + 8) * ggml_tensor_overhead() + (1u << 20);
    ggml_init_params ip = { overhead, nullptr, true };
    b.ctx = ggml_init(ip);
    if (b.ctx == nullptr) {
        return false;
    }
    for (const TSpec & s : cs.tensors) {
        ggml_tensor * t = ggml_new_tensor_4d(b.ctx, (ggml_type) s.type,
                                             s.ne[0], s.ne[1], s.ne[2], s.ne[3]);
        if (t == nullptr) {
            return false;
        }
        ggml_set_name(t, s.name.c_str());
        b.tensors.push_back(t);
    }

    m.layers.resize(m.hparams.n_layer_all > 0 ? m.hparams.n_layer_all : 0);
    for (const auto & g : cs.globals) {
        set_global(m, g.first, b.tensors[g.second]);
    }
    for (const auto & lt : cs.layer_t) {
        const int layer = lt.first / (int) (LF_ATTN_Q_NORM + 1);
        const int field = lt.first % (int) (LF_ATTN_Q_NORM + 1);
        if (layer < (int) m.layers.size()) {
            set_layer_field(m.layers[layer], field, b.tensors[lt.second]);
        }
    }

    // prec policy: names come from the tensor table, the order is this order
    for (size_t p = 0; p < cs.prec.size(); ++p) {
        m.prec_policy.prec_src1[b.tensors[p]] = (enum ggml_prec) cs.prec[p];
    }
    return true;
}

static void run_native(Built & b, std::vector<std::string> & kv, std::vector<std::string> & td,
                       std::string & err) {
    try {
        llama_model_saver ns(&b.model);
        ns.add_kv_from_model();
        ns.add_tensors_from_model();
        kv = dump_native_kv(ns.gguf_ctx);
        const int n = (int) gguf_get_n_tensors(ns.gguf_ctx);
        for (int i = 0; i < n; ++i) {
            const char * name = gguf_get_tensor_name(ns.gguf_ctx, i);
            const int t = (int) gguf_get_tensor_type(ns.gguf_ctx, i);
            const size_t off = gguf_get_tensor_offset(ns.gguf_ctx, i);
            // find the tensor to read dims
            const ggml_tensor * gt = nullptr;
            for (const auto * x : b.tensors) {
                if (std::strcmp(ggml_get_name(x), name) == 0) {
                    gt = x;
                    break;
                }
            }
            char buf[512];
            if (gt != nullptr) {
                std::snprintf(buf, sizeof buf, "T %s type=%d off=%zu dims=%d ne=%lld,%lld,%lld,%lld",
                              name, t, off, ggml_n_dims(gt),
                              (long long) gt->ne[0], (long long) gt->ne[1],
                              (long long) gt->ne[2], (long long) gt->ne[3]);
            } else {
                std::snprintf(buf, sizeof buf, "T %s type=%d off=%zu dims=? ne=?", name, t, off);
            }
            td.push_back(buf);
        }
    } catch (const std::exception & e) {
        err = e.what();
    }
}

static int64_t run_port(Built & b, const Case & cs, int64_t charmap_len) {
    PortState & ps = b.ps;
    build_key_table(ps, cs.arch);

    // resolved string table: fixed entries, labels, prec names, tokens, merges
    int slot = -1;
    add_string(ps, b.model.arch_name(), slot);
    add_string(ps, b.model.name, slot);
    add_string(ps, llama_rope_section_order_name(b.model.hparams.rope_section_order), slot);
    add_string(ps, llama_rope_scaling_type_name(b.model.hparams.rope_scaling_type_train), slot);
    add_string(ps, b.model.vocab.get_tokenizer_model(), slot);
    add_string(ps, b.model.vocab.get_tokenizer_pre(), slot);
    const int label_first = (int) ps.strings.size();
    for (const std::string & l : b.model.classifier_labels) {
        ps.strings.push_back(l);
    }
    // native walks its map in address order, so take names and values from
    // exactly that container instead of the case's insertion order
    const int prec_first = (int) ps.strings.size();
    for (const auto & pr : b.model.prec_policy.prec_src1) {
        ps.strings.push_back(ggml_get_name(pr.first));
        ps.prec.push_back((int64_t) pr.second);
    }
    const int tok_first = (int) ps.strings.size();
    const int n_tok = (int) b.model.vocab.n_tokens();
    // get_token_data asserts for a vocab-less model; native never calls it there
    // either (the fill loop is guarded by get_type() != NONE) and leaves the
    // vectors zero-filled, so mirror that shape.
    const bool vocab_none = b.model.vocab.get_type() == LLAMA_VOCAB_TYPE_NONE;
    for (int i = 0; i < n_tok; ++i) {
        if (vocab_none) {
            ps.strings.push_back("");
            ps.attrs.push_back(LLAMA_TOKEN_ATTR_UNDEFINED);
            ps.scores.push_back(0.0f);
        } else {
            ps.strings.push_back(b.model.vocab.get_token_data(i).text);
            ps.attrs.push_back((int64_t) b.model.vocab.get_token_data(i).attr);
            ps.scores.push_back(b.model.vocab.get_token_data(i).score);
        }
    }
    const int merge_first = (int) ps.strings.size();
    std::vector<std::string> merges = b.model.vocab.get_bpe_merges();
    for (const std::string & x : merges) {
        ps.strings.push_back(x);
    }
    finalize_strings(ps);

    ps.ix.assign(32, 0);
    ps.ix[0] = n_tok;                 // I_NTOKENS
    ps.ix[1] = 0;                     // I_NTOKEN_TYPES (unused by the port)
    ps.ix[2] = (int64_t) merges.size();
    ps.ix[3] = (int64_t) b.model.classifier_labels.size();
    ps.ix[4] = (int64_t) ps.prec.size();
    ps.ix[5] = tok_first;
    ps.ix[6] = merge_first;
    ps.ix[7] = label_first;
    ps.ix[8] = prec_first;
    ps.ix[9] = b.model.hparams.n_layer_all;
    ps.ix[10] = b.model.hparams.n_layer_nextn;
    ps.ix[11] = (int64_t) b.model.arch;
    ps.ix[12] = (int64_t) LLM_ARCH_DEEPSEEK4;
    ps.ix[13] = (int64_t) LLAMA_SWA_TYPE_NONE;
    ps.ix[14] = (int64_t) LLAMA_TOKEN_NULL;
    ps.ix[15] = SLOT_AV;
    ps.ix[16] = 4;
    ps.ix[17] = b.model.hparams.ple_ngram_size;
    ps.ix[18] = b.model.hparams.ple_n_heads;

    ps.hv.assign(512, 0);
    ps.hf.assign(256, 0.0f);
    slot_fill_hv(b.model.hparams, ps.hv.data());
    slot_fill_hf(b.model.hparams, ps.hf.data());
    slot_fill_vocab(b.model.vocab, ps.hv.data());
    ps.ha.assign(512 * 64, 0);
    ps.haf.assign(512 * 64, 0.0f);
    slot_fill_ha(b.model.hparams, ps.ha.data());
    slot_fill_haf(b.model.hparams, ps.haf.data());

    const int r0 = (int) sav_init(A(ps.st.data(), 8), A(ps.key_off.data(), 512),
                                  A(ps.key_len.data(), 512), A(ps.types.data(), 512),
                                  A(ps.arr.data(), 512), A(ps.v0.data(), 512),
                                  A(ps.v1.data(), 512), A(ps.v2.data(), 512),
                                  A(ps.tst.data(), 8), 32, 0);
    if (r0 != 0) {
        return -100;
    }
    const int64_t rc = savm_add_kv_from_model(
        A(ps.st.data(), 8), S(ps.pool.data(), (int64_t) ps.pool.size()),
        A(ps.key_off.data(), 512), A(ps.key_len.data(), 512),
        A(ps.types.data(), 512), A(ps.arr.data(), 512),
        A(ps.v0.data(), 512), A(ps.v1.data(), 512), A(ps.v2.data(), 512),
        A(ps.tst.data(), 8),
        A(ps.k_off.data(), SLOT_NKEY), A(ps.k_len.data(), SLOT_NKEY), S(ps.k_pool.data(), (int64_t) ps.k_pool.size()),
        A(ps.s_off.data(), (int64_t) ps.s_off.size()), A(ps.s_len.data(), (int64_t) ps.s_len.size()),
        S(ps.s_pool.data(), (int64_t) ps.s_pool.size()),
        A(ps.ix.data(), 32), A(ps.hv.data(), 512), F(ps.hf.data(), 256),
        A(ps.ha.data(), (int64_t) ps.ha.size()), F(ps.haf.data(), (int64_t) ps.haf.size()),
        A(ps.attrs.data(), (int64_t) ps.attrs.size()), F(ps.scores.data(), (int64_t) ps.scores.size()),
        A(ps.prec.data(), (int64_t) ps.prec.size()),
        charmap_len, S(ps.scratch.data(), (int64_t) ps.scratch.size()), A(ps.out.data(), 2));
    return rc;
}

static int64_t run_port_tensors(Built & b, const Case & cs) {
    PortState & ps = b.ps;
    std::vector<int64_t> tname_off, tname_len;
    std::vector<uint8_t> tname_pool, tdata_pool;
    std::vector<int64_t> tnd, tne4, ttype, tdata_off, tdata_len;
    for (const auto & t : b.tensors) {
        tname_off.push_back((int64_t) tname_pool.size());
        const std::string nm = ggml_get_name(t);
        tname_len.push_back((int64_t) nm.size());
        tname_pool.insert(tname_pool.end(), nm.begin(), nm.end());
        tnd.push_back(ggml_n_dims(t));
        for (int d = 0; d < 4; ++d) {
            tne4.push_back(t->ne[d]);
        }
        ttype.push_back((int64_t) t->type);
        tdata_off.push_back((int64_t) tdata_pool.size());
        const size_t nb = ggml_nbytes(t);
        tdata_len.push_back((int64_t) nb);
        tdata_pool.insert(tdata_pool.end(), nb, 0);
    }

    std::vector<int64_t> gfix(64, -1);
    for (const auto & g : cs.globals) {
        gfix[g.first] = g.second;
    }
    const int ln = (int) b.model.layers.size();
    const int lnptrs = (int) (sizeof(llama_layer) / sizeof(ggml_tensor *));
    std::vector<int64_t> lptrs((size_t) ln * lnptrs, -1);
    for (int l = 0; l < ln; ++l) {
        auto * p = reinterpret_cast<ggml_tensor **>(&b.model.layers[l]);
        for (int i = 0; i < lnptrs; ++i) {
            if (p[i] != nullptr) {
                for (size_t k = 0; k < b.tensors.size(); ++k) {
                    if (b.tensors[k] == p[i]) {
                        lptrs[(size_t) l * lnptrs + i] = (int64_t) k;
                        break;
                    }
                }
            }
        }
    }

    return savm_add_tensors_from_model(
        A(ps.tst.data(), 8), S(ps.tpool.data(), (int64_t) ps.tpool.size()),
        A(ps.tne.data(), 512 * 4), A(ps.trec.data(), 512 * 8),
        A(gfix.data(), (int64_t) gfix.size()), A(lptrs.data(), (int64_t) lptrs.size()), ln, lnptrs,
        A(tname_off.data(), (int64_t) tname_off.size()), A(tname_len.data(), (int64_t) tname_len.size()),
        S(tname_pool.data(), (int64_t) tname_pool.size()),
        A(tnd.data(), (int64_t) tnd.size()), A(tne4.data(), (int64_t) tne4.size()),
        A(ttype.data(), (int64_t) ttype.size()),
        A(tdata_off.data(), (int64_t) tdata_off.size()), A(tdata_len.data(), (int64_t) tdata_len.size()),
        S(tdata_pool.data(), (int64_t) tdata_pool.size()),
        A(ps.out.data(), 2));
}

static void compare(const std::string & cs, const std::vector<std::string> & a,
                    const std::vector<std::string> & b, const char * what) {
    expect(a.size() == b.size(), cs, std::string(what) + " count " +
           std::to_string(a.size()) + " vs " + std::to_string(b.size()));
    const size_t n = std::min(a.size(), b.size());
    for (size_t i = 0; i < n; ++i) {
        checks++;
        if (a[i] != b[i]) {
            fail(cs, std::string(what) + " #" + std::to_string(i) + "\n  native: " + a[i] +
                 "\n  port:   " + b[i]);
        }
    }
}

static void run_case(Case & cs, int idx) {
    cases++;
    Built b;
    if (!build_native(b, cs, idx)) {
        fail(cs.name, "native setup");
        return;
    }

    // the charmap case hands the port a non-empty map: native would abort on
    // the char overload, so it is a port-only contract check
    const int64_t prc = run_port(b, cs, cs.charmap_len);
    if (cs.port_only) {
        expect(prc == cs.expect_rc, cs.name, "port rc " + std::to_string(prc));
        return;
    }

    std::vector<std::string> nkv, ntd;
    std::string err;
    run_native(b, nkv, ntd, err);
    if (!err.empty()) {
        fail(cs.name, "native: " + err);
        return;
    }
    expect(prc == 0, cs.name, "port rc " + std::to_string(prc));

    std::vector<std::string> pkv = dump_port_kv(b.ps);
    compare(cs.name, nkv, pkv, "kv");

    const int64_t tn = run_port_tensors(b, cs);
    std::vector<std::string> ptd = dump_port_tensors(b.ps);
    expect((size_t) tn == ntd.size(), cs.name, "tensor count " + std::to_string(tn) +
           " vs " + std::to_string(ntd.size()));
    compare(cs.name, ntd, ptd, "tensor");
}

// ---------------------------------------------------------------- cases

static VocabSpec simple_vocab(int n, const char * model = "llama") {
    VocabSpec v;
    v.model = model;
    for (int i = 0; i < n; ++i) {
        TokSpec t;
        t.text = "<t" + std::to_string(i) + ">";
        t.score = (float) i * 0.5f;
        t.type = LLAMA_TOKEN_TYPE_NORMAL;
        v.toks.push_back(t);
    }
    return v;
}

static void add_llama_tensors(Case & c, int n_layer) {
    // tok_embd, norm, output, plus per-layer blocks
    c.tensors.push_back({"tok_embd.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
    c.globals.push_back({G_TOK_EMBD, 0});
    c.tensors.push_back({"output_norm.weight", GGML_TYPE_F32, {8, 1, 1, 1}});
    c.globals.push_back({G_OUTPUT_NORM, 1});
    c.tensors.push_back({"output.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
    c.globals.push_back({G_OUTPUT, 2});
    for (int l = 0; l < n_layer; ++l) {
        const int base = (int) c.tensors.size();
        const std::string p = "blk." + std::to_string(l) + ".";
        c.tensors.push_back({p + "attn_norm.weight", GGML_TYPE_F32, {8, 1, 1, 1}});
        c.tensors.push_back({p + "attn_q.weight", GGML_TYPE_F16, {8, 8, 1, 1}});
        c.tensors.push_back({p + "attn_k.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.tensors.push_back({p + "attn_v.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.tensors.push_back({p + "attn_o.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.tensors.push_back({p + "ffn_norm.weight", GGML_TYPE_F32, {8, 1, 1, 1}});
        c.tensors.push_back({p + "ffn_up.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.tensors.push_back({p + "ffn_down.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_ATTN_NORM, base + 0});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_WQ, base + 1});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_WK, base + 2});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_WV, base + 3});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_WO, base + 4});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_FFN_NORM, base + 5});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_FFN_UP, base + 6});
        c.layer_t.push_back({l * (LF_ATTN_Q_NORM + 1) + LF_FFN_DOWN, base + 7});
    }
}

static void build_cases() {
    // 1: minimal, one layer, tok_embd and output share a name -> only output saved
    {
        Case & c = add_case("c01_minimal_shared_output");
        c.hp = [](llama_hparams & h) { h.n_layer_all = 1; };
        c.vocab = simple_vocab(8);
        c.tensors.push_back({"tok_embd.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.globals.push_back({G_TOK_EMBD, 0});
        c.globals.push_back({G_OUTPUT, 0});      // same pointer and name
        c.tensors.push_back({"output_norm.weight", GGML_TYPE_F32, {8, 1, 1, 1}});
        c.globals.push_back({G_OUTPUT_NORM, 1});
    }
    // 2: output differs from tok_embd -> alias branch saves tok_embd first
    {
        Case & c = add_case("c02_output_alias");
        c.hp = [](llama_hparams & h) { h.n_layer_all = 1; };
        c.vocab = simple_vocab(8);
        add_llama_tensors(c, 1);
    }
    // 3: no tensor at all (null model) -> empty descriptor list
    {
        Case & c = add_case("c03_no_tensors");
        c.vocab = simple_vocab(4);
    }
    // 4: nextn layers reduce the per-layer count
    {
        Case & c = add_case("c04_nextn_per_layer");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 4;
            h.n_layer_nextn = 1;
            h.n_ff_arr[0] = 100;
            h.n_ff_arr[1] = 200;
            h.n_ff_arr[2] = 300;
            h.n_ff_arr[3] = 400;
            h.n_head_arr[0] = 2;
            h.n_head_arr[1] = 4;
            h.n_head_arr[2] = 6;
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 4);
    }
    // 5: equal per-layer values collapse to one scalar
    {
        Case & c = add_case("c05_per_layer_collapse");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 3;
            for (int i = 0; i < 3; ++i) {
                h.n_ff_arr[i] = 77;
                h.n_ff_exp_arr[i] = 33;
                h.rope_pattern[i] = 1;
            }
            h.n_head_arr[0] = 8;
            h.n_head_arr[1] = 8;
            h.n_head_arr[2] = 8;
            h.n_head_kv_arr[0] = 1;
            h.n_head_kv_arr[1] = 2;
            h.n_head_kv_arr[2] = 3;
        };
        c.vocab = simple_vocab(5);
        add_llama_tensors(c, 3);
    }
    // 6: embedding_length_out present
    {
        Case & c = add_case("c06_embd_out");
        c.hp = [](llama_hparams & h) { h.n_embd_out_impl = 96; };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 7: embedding_length_out absent (zero)
    {
        Case & c = add_case("c07_embd_out_zero");
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 8: sliding window off
    {
        Case & c = add_case("c08_swa_off");
        c.hp = [](llama_hparams & h) { h.n_swa = 0; h.swa_type = LLAMA_SWA_TYPE_NONE; };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 9: sliding window on, mixed pattern (array, never collapsed)
    {
        Case & c = add_case("c09_swa_mixed");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 4;
            h.n_swa = 32;
            h.swa_type = LLAMA_SWA_TYPE_STANDARD;
            for (int i = 0; i < 4; ++i) {
                h.is_swa_impl[i] = (i % 2 == 0) ? 1 : 0;
            }
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 4);
    }
    // 10: prec policy with Q8 and F32
    {
        Case & c = add_case("c10_prec_q8_f32");
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
        c.prec = {GGML_PREC_Q8, GGML_PREC_F32, GGML_PREC_Q8};
    }
    // 11: prec policy all Q8 -> an array of zero bools, not a scalar
    {
        Case & c = add_case("c11_prec_all_q8");
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 1);
        c.prec = {GGML_PREC_Q8, GGML_PREC_Q8};
    }
    // 12: deepseek4 arch writes one compress ratio per layer
    {
        Case & c = add_case("c12_dsv4_arch");
        c.arch = LLM_ARCH_DEEPSEEK4;
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 3;
            for (int i = 0; i < 3; ++i) {
                h.dsv4_compress_ratios[i] = 4 + i;
            }
            h.dsv4_hc_mult = 0;
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 3);
    }
    // 13: hc_mult > 0 on a non-dsv4 arch takes the same branch
    {
        Case & c = add_case("c13_dsv4_hc_mult");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 2;
            h.dsv4_compress_ratios[0] = 2;
            h.dsv4_compress_ratios[1] = 7;
            h.dsv4_hc_mult = 2;
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 14: non-dsv4 compress ratios go through the per-layer reduction
    {
        Case & c = add_case("c14_compress_per_layer");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 3;
            h.dsv4_compress_ratios[0] = 5;
            h.dsv4_compress_ratios[1] = 5;
            h.dsv4_compress_ratios[2] = 5;
            h.dsv4_hc_mult = 0;
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 3);
    }
    // 15: PLE group present
    {
        Case & c = add_case("c15_ple_group");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 4;
            h.ple_n_heads = 3;
            h.ple_ngram_size = 4;
            h.ple_heads_per_ngram = 1;
            h.ple_conv_kernel = 2;
            h.ple_eos_token_id = 3;
            h.ple_head_dim = 16;
            h.is_ple_impl[0] = 1;
            h.is_ple_impl[2] = 1;
            h.ple_layer_multipliers[0] = 1;
            h.ple_layer_multipliers[1] = 1000000007ULL;
            h.ple_layer_multipliers[2] = 3;
            h.ple_layer_multipliers[3] = 0;
            h.ple_head_offsets[0] = 0;
            h.ple_head_offsets[1] = 8;
            h.ple_head_offsets[2] = 16;
            h.ple_head_vocab_sizes[0] = 32;
            h.ple_head_vocab_sizes[1] = 64;
            h.ple_head_vocab_sizes[2] = 128;
        };
        c.vocab = simple_vocab(8);
        add_llama_tensors(c, 4);
    }
    // 16: PLE absent
    {
        Case & c = add_case("c16_ple_absent");
        c.hp = [](llama_hparams & h) { h.ple_n_heads = 0; h.ple_ngram_size = 0; };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 17: SPM vocab, every attr class, scores, special ids and flags
    {
        Case & c = add_case("c17_vocab_spm_attrs");
        c.vocab = simple_vocab(12);
        const int32_t kinds[12] = {
            LLAMA_TOKEN_TYPE_UNKNOWN, LLAMA_TOKEN_TYPE_CONTROL, LLAMA_TOKEN_TYPE_NORMAL,
            LLAMA_TOKEN_TYPE_USER_DEFINED, LLAMA_TOKEN_TYPE_UNUSED, LLAMA_TOKEN_TYPE_BYTE,
            LLAMA_TOKEN_TYPE_UNDEFINED, LLAMA_TOKEN_TYPE_NORMAL, LLAMA_TOKEN_TYPE_NORMAL,
            LLAMA_TOKEN_TYPE_NORMAL, LLAMA_TOKEN_TYPE_NORMAL, LLAMA_TOKEN_TYPE_NORMAL,
        };
        for (int i = 0; i < 12; ++i) {
            c.vocab.toks[i].type = kinds[i];
            c.vocab.toks[i].score = (float) (i * i) * 0.25f;
        }
        c.vocab.bos = 1;
        c.vocab.eos = 2;
        c.vocab.eot = 2;
        c.vocab.eom = 3;
        c.vocab.unk = 0;
        c.vocab.sep = 4;
        c.vocab.pad = 5;
        c.vocab.fim_pre = 6;
        c.vocab.fim_suf = 7;
        c.vocab.fim_mid = 8;
        c.vocab.fim_pad = 9;
        c.vocab.fim_rep = 10;
        c.vocab.fim_sep = 11;
        c.vocab.add_bos = 1;
        c.vocab.add_eos = 0;
        c.vocab.add_sep = 1;
        c.vocab.add_prefix = 0;
        c.vocab.remove_ws = 1;
        add_llama_tensors(c, 2);
    }
    // 18: BPE vocab with merges, no special ids, no add flags
    {
        Case & c = add_case("c18_vocab_bpe_merges");
        c.vocab = simple_vocab(10, "gpt2");
        c.vocab.pre = "default";
        c.vocab.merges = {"a b", "c d", "e f"};
        add_llama_tensors(c, 2);
    }
    // 19: vocab-less model: token texts empty, token types all UNDEFINED
    {
        Case & c = add_case("c19_vocab_none");
        c.vocab.model = "no_vocab";
        c.vocab.pre.clear();
        c.vocab.vocab_size = 8;
        add_llama_tensors(c, 2);
    }
    // 20: looped arch: the same pointer in several layers is saved once
    {
        Case & c = add_case("c20_layer_alias");
        c.hp = [](llama_hparams & h) { h.n_layer_all = 3; };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 3);
        // layer 1 and layer 2 share the layer-0 ffn_down tensor
        const int shared = 2 + 0 * 8 + 7;
        c.layer_t.push_back({1 * (LF_ATTN_Q_NORM + 1) + LF_FFN_DOWN, shared});
        c.layer_t.push_back({2 * (LF_ATTN_Q_NORM + 1) + LF_FFN_DOWN, shared});
    }
    // 21: duplicate rope tensor name across layers is skipped, not refused
    {
        Case & c = add_case("c21_rope_duplicate");
        c.hp = [](llama_hparams & h) { h.n_layer_all = 2; };
        c.vocab = simple_vocab(6);
        c.tensors.push_back({"tok_embd.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.globals.push_back({G_TOK_EMBD, 0});
        c.tensors.push_back({"output.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.globals.push_back({G_OUTPUT, 1});
        c.tensors.push_back({"rope_freqs.weight", GGML_TYPE_F32, {4, 1, 1, 1}});
        c.tensors.push_back({"rope_freqs.weight", GGML_TYPE_F32, {4, 1, 1, 1}});
        c.layer_t.push_back({0 * (LF_ATTN_Q_NORM + 1) + LF_WQ, 2});
        c.layer_t.push_back({1 * (LF_ATTN_Q_NORM + 1) + LF_WQ, 3});
    }
    // 22: every model-level global slot occupied
    {
        Case & c = add_case("c22_all_globals");
        c.vocab = simple_vocab(6);
        const char * names[] = {
            "type_embd", "pos_embd", "tok_norm", "tok_norm_b", "output_norm", "output_norm_b",
            "output", "output_b", "output_norm_enc", "output_s", "output_in_s", "output_res_score",
            "nextn_proj_pre", "nextn_proj_post", "cls", "cls_b", "cls_out", "cls_out_b",
            "cls_norm", "hrm_z_l_init", "hc_head_fn", "hc_head_base", "hc_head_scale",
            "per_layer_tok_embd", "hc_head_norm", "hc_head_down", "hc_head_up",
        };
        int idx = 0;
        for (const char * n : names) {
            c.tensors.push_back({std::string(n) + ".weight", GGML_TYPE_F32, {4, 2, 1, 1}});
            c.globals.push_back({G_TYPE_EMBD + idx, idx});
            ++idx;
        }
        c.tensors.push_back({"tok_embd.weight", GGML_TYPE_F32, {8, 8, 1, 1}});
        c.globals.push_back({G_TOK_EMBD, idx});
    }
    // 23: zero layers: the native saver aborts in n_expert_used(), so only the
    // port contract is checked (nothing per-layer is written)
    {
        Case & c = add_case("c23_zero_layers_port");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 0;
            h.n_layer_nextn = 0;
            h.ple_n_heads = 0;
        };
        c.vocab = simple_vocab(4);
        c.port_only = true;
        c.expect_rc = 0;
    }
    // 24: rope scaling factor 0 for a 1.0 train scale
    {
        Case & c = add_case("c24_rope_factor_one");
        c.hp = [](llama_hparams & h) { h.rope_freq_scale_train = 1.0f; };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 25: rope scaling factor 1/scale
    {
        Case & c = add_case("c25_rope_factor_quarter");
        c.hp = [](llama_hparams & h) {
            h.rope_freq_scale_train = 4.0f;
            h.rope_scaling_type_train = LLAMA_ROPE_SCALING_TYPE_YARN;
            h.rope_section_order = LLAMA_ROPE_SECTION_ORDER_TYXZ;
            h.rope_sections = {8, 4, 2, 2};
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 2);
    }
    // 26: classifier labels, deepstack mapping, xielu and float arrays
    {
        Case & c = add_case("c26_labels_and_arrays");
        c.labels = {"positive", "negative", "neutral"};
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 3;
            h.n_deepstack_layers = 2;
            for (int i = 0; i < 3; ++i) {
                h.deepstack_mapping_arr[i] = i * 2 - 1;
                h.xielu_alpha_n[i] = 1.5f + (float) i;
                h.swiglu_clamp_exp[i] = 10.0f;
                h.swiglu_clamp_shexp[i] = 20.0f;
            }
            h.indexer_n_head = 4;
            h.indexer_kpool_select_tail = false;
            h.kda_gate_lower_bound = -3.0f;
            h.ssm_dt_b_c_rms = true;
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 3);
    }
    // 27: smol model with every scalar hparam left at its default
    {
        Case & c = add_case("c27_defaults");
        c.vocab = simple_vocab(3);
        add_llama_tensors(c, 1);
    }
    // 28: f16 and i32 tensors keep their type and size in the descriptor
    {
        Case & c = add_case("c28_tensor_types");
        c.hp = [](llama_hparams & h) { h.n_layer_all = 1; };
        c.vocab = simple_vocab(4);
        c.tensors.push_back({"tok_embd.weight", GGML_TYPE_F16, {16, 16, 1, 1}});
        c.globals.push_back({G_TOK_EMBD, 0});
        c.tensors.push_back({"output.weight", GGML_TYPE_I32, {16, 16, 1, 1}});
        c.globals.push_back({G_OUTPUT, 1});
        c.tensors.push_back({"blk.0.attn_norm.weight", GGML_TYPE_F32, {16, 1, 1, 1}});
        c.layer_t.push_back({LF_ATTN_NORM, 2});
        c.tensors.push_back({"blk.0.wq.weight", GGML_TYPE_F16, {16, 16, 1, 1}});
        c.layer_t.push_back({LF_WQ, 3});
    }
    // 29: three layers of mixed per-layer and whole arrays
    {
        Case & c = add_case("c29_layered_mix");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 3;
            h.n_layer_nextn = 1;
            h.n_ff_shexp = 55;
            h.n_expert = 4;
            h.n_expert_shared = 1;
            for (int i = 0; i < 3; ++i) {
                h.is_recr_impl[i] = (i == 1) ? 1 : 0;
                h.is_indexer_full_impl[i] = 1;
                h.n_head_arr[i] = 2 + i;
                h.n_head_kv_arr[i] = 1;
            }
        };
        c.vocab = simple_vocab(7);
        add_llama_tensors(c, 3);
    }
    // 31: hc_mult > 0 must take the full-array branch: with nextn layers and
    // equal ratios the per-layer path would collapse to one scalar instead
    {
        Case & c = add_case("c31_dsv4_hc_mult_collapse");
        c.hp = [](llama_hparams & h) {
            h.n_layer_all = 3;
            h.n_layer_nextn = 1;
            for (int i = 0; i < 3; ++i) {
                h.dsv4_compress_ratios[i] = 5;
            }
            h.dsv4_hc_mult = 2;
        };
        c.vocab = simple_vocab(6);
        add_llama_tensors(c, 3);
    }
    // 32: empty model name is skipped (std::string template early return)
    {
        Case & c = add_case("c32_empty_name");
        c.model_name = "";
        c.vocab = simple_vocab(5);
        add_llama_tensors(c, 1);
    }
    // 30: port-only charmap refusal (native GGML_ABORTs on the char overload)
    {
        Case & c = add_case("c30_charmap_refusal");
        c.vocab = simple_vocab(4);
        c.port_only = true;
        c.charmap_len = 1;
        c.expect_rc = -1;
    }
}

// ---------------------------------------------------------------- support arch

static void support_arch_check() {
    cases++;
    for (int a = 0; a <= LLM_ARCH_UNKNOWN; ++a) {
        const int64_t native = llama_model_saver_supports_arch((llm_arch) a);
        const int64_t port = savm_supports_arch(a,
            LLM_ARCH_GEMMA3N, LLM_ARCH_BITNET, LLM_ARCH_T5, LLM_ARCH_APERTUS,
            LLM_ARCH_STEP35, LLM_ARCH_CLEF, LLM_ARCH_VLLM_DECISION2);
        checks++;
        if (native != port) {
            fail("supports_arch", "arch " + std::to_string(a) + " native " +
                 std::to_string(native) + " port " + std::to_string(port));
        }
    }
}

static void quiet_log(ggml_log_level level, const char * text, void * ud) {
    (void) ud;
    if (level == GGML_LOG_LEVEL_ERROR) {
        std::fputs(text, stderr);
    }
}
static void quiet_llama_log(ggml_log_level level, const char * text, void * ud) {
    (void) ud;
    if (level == GGML_LOG_LEVEL_ERROR) {
        std::fputs(text, stderr);
    }
}

int main(int argc, char ** argv) {
    if (argc > 1) {
        g_work = argv[1];
    }
    goose_init(argc, argv);
    llama_log_set(quiet_llama_log, nullptr);
    ggml_log_set(quiet_log, nullptr);
    llama_backend_init();
    build_cases();

    for (size_t i = 0; i < g_cases.size(); ++i) {
        run_case(g_cases[i], (int) i);
    }
    support_arch_check();

    std::printf("FG_S5 cases=%d checks=%d failures=%d\n", cases, checks, fails);
    llama_backend_free();
    return fails == 0 ? 0 : 1;
}
