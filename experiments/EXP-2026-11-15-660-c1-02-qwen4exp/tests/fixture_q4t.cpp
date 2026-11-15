// C1-02 slice T gate: qwen4exp tensor map against a transcription of the
// native name/flag tables.
//
// The oracle is an independent transcription of
//   src/llama-arch.cpp:425-575    (tensor names)
//   src/llama-arch.cpp:709-780    (layer bucket per tensor)
//   src/models/qwen4exp.cpp:109-284 (which suffix each create_tensor passes,
//                                   and which loader flags)
//   src/llama-model.cpp:3138-3180   (fused gate_up / qkv presence probes)
// No native library is linked.
//
// T_NEXTN_HC_HEAD_* (49..51) have no entry in the pinned llama-arch.cpp: their
// names come from the integration tree and are reported as a separate group.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gen.h"

static int g_cases = 0, g_values = 0, g_fails = 0;

static void bad(const char * what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 200) {
        printf("FAIL %s: %s\n", what, detail.c_str());
    }
}

static void expect_i64(const char * what, int64_t got, int64_t want) {
    ++g_values;
    if (got != want) {
        bad(what, "got " + std::to_string((long long) got) +
                  " want " + std::to_string((long long) want));
    }
}

static void expect_str(const char * what, const std::string & got, const std::string & want) {
    ++g_values;
    if (got != want) {
        bad(what, "got '" + got + "' want '" + want + "'");
    }
}

static sl_u8 u8buf(std::string & s) {
    return sl_u8{ (uint8_t *) s.data(), (int64_t) s.size() };
}

// ---- native transcription --------------------------------------------------

enum bucket_t { B_IN, B_REP, B_OUT };
enum suffix_t { S_W, S_B, S_N };

struct trow {
    const char * name;
    bucket_t bucket;
    suffix_t suffix;
    int not_required;
    int read_lazy;
    int skip_if_virtual;
};

// Index == the canon/qwen4exp.goose tensor id.
static const trow T[] = {
    { "token_embd",                 B_IN,  S_W, 0, 0, 0 }, // 0
    { "output_hc_norm",             B_OUT, S_W, 0, 0, 0 }, // 1
    { "output_hc_down",             B_OUT, S_W, 0, 0, 0 }, // 2
    { "output_hc_up",               B_OUT, S_W, 0, 0, 0 }, // 3
    { "output",                     B_OUT, S_W, 1, 0, 0 }, // 4
    { "per_layer_token_embd",       B_IN,  S_W, 0, 1, 0 }, // 5
    { "blk.%d.hc_attn_norm",        B_REP, S_W, 0, 0, 0 }, // 6
    { "blk.%d.hc_attn_down",        B_REP, S_W, 0, 0, 0 }, // 7
    { "blk.%d.hc_attn_up",          B_REP, S_W, 0, 0, 0 }, // 8
    { "blk.%d.hc_attn_inject",      B_REP, S_W, 0, 0, 0 }, // 9
    { "blk.%d.hc_ffn_norm",         B_REP, S_W, 0, 0, 0 }, // 10
    { "blk.%d.hc_ffn_down",         B_REP, S_W, 0, 0, 0 }, // 11
    { "blk.%d.hc_ffn_up",           B_REP, S_W, 0, 0, 0 }, // 12
    { "blk.%d.hc_ffn_inject",       B_REP, S_W, 0, 0, 0 }, // 13
    { "blk.%d.attn_qkv",            B_REP, S_W, 0, 0, 1 }, // 14
    { "blk.%d.attn_output",         B_REP, S_W, 0, 0, 0 }, // 15
    { "blk.%d.attn_q_norm",         B_REP, S_W, 0, 0, 0 }, // 16
    { "blk.%d.attn_k_norm",         B_REP, S_W, 0, 0, 0 }, // 17
    { "blk.%d.indexer.q_proj",      B_REP, S_W, 0, 0, 0 }, // 18
    { "blk.%d.indexer.k_proj",      B_REP, S_W, 0, 0, 0 }, // 19
    { "blk.%d.indexer.q_norm",      B_REP, S_W, 0, 0, 0 }, // 20
    { "blk.%d.indexer.k_norm",      B_REP, S_W, 0, 0, 0 }, // 21
    { "blk.%d.attn_qkv",            B_REP, S_W, 0, 0, 1 }, // 22
    { "blk.%d.attn_gate",           B_REP, S_W, 0, 0, 0 }, // 23
    { "blk.%d.ssm_conv1d",          B_REP, S_W, 0, 0, 0 }, // 24
    { "blk.%d.ssm_dt",              B_REP, S_B, 0, 0, 0 }, // 25
    { "blk.%d.ssm_a",               B_REP, S_N, 0, 0, 0 }, // 26
    { "blk.%d.ssm_beta",            B_REP, S_W, 0, 0, 0 }, // 27
    { "blk.%d.ssm_alpha",           B_REP, S_W, 0, 0, 0 }, // 28
    { "blk.%d.ssm_norm",            B_REP, S_W, 0, 0, 0 }, // 29
    { "blk.%d.ssm_out",             B_REP, S_W, 0, 0, 0 }, // 30
    { "blk.%d.ple_key",             B_REP, S_W, 0, 0, 0 }, // 31
    { "blk.%d.ple_value",           B_REP, S_W, 0, 0, 0 }, // 32
    { "blk.%d.ple_norm_key",        B_REP, S_W, 0, 0, 0 }, // 33
    { "blk.%d.ple_norm_query",      B_REP, S_W, 0, 0, 0 }, // 34
    { "blk.%d.ple_norm_conv",       B_REP, S_W, 0, 0, 0 }, // 35
    { "blk.%d.ple_conv1d",          B_REP, S_W, 0, 0, 0 }, // 36
    { "blk.%d.ffn_gate_inp",        B_REP, S_W, 0, 0, 0 }, // 37
    { "blk.%d.ffn_down_exps",       B_REP, S_W, 0, 0, 0 }, // 38
    { "blk.%d.ffn_gate_up_exps",    B_REP, S_W, 1, 0, 0 }, // 39
    { "blk.%d.ffn_gate_exps",       B_REP, S_W, 0, 0, 0 }, // 40
    { "blk.%d.ffn_up_exps",         B_REP, S_W, 0, 0, 0 }, // 41
    { "blk.%d.ffn_gate_inp_shexp",  B_REP, S_W, 0, 0, 0 }, // 42
    { "blk.%d.ffn_gate_shexp",      B_REP, S_W, 0, 0, 0 }, // 43
    { "blk.%d.ffn_up_shexp",        B_REP, S_W, 0, 0, 0 }, // 44
    { "blk.%d.ffn_down_shexp",      B_REP, S_W, 0, 0, 0 }, // 45
    { "blk.%d.nextn.eh_proj",       B_REP, S_W, 0, 0, 0 }, // 46
    { "blk.%d.nextn.enorm",         B_REP, S_W, 0, 0, 0 }, // 47
    { "blk.%d.nextn.hnorm",         B_REP, S_W, 0, 0, 0 }, // 48
    { "blk.%d.nextn.hc_head_norm",  B_REP, S_W, 0, 0, 0 }, // 49 (integration tree)
    { "blk.%d.nextn.hc_head_down",  B_REP, S_W, 0, 0, 0 }, // 50 (integration tree)
    { "blk.%d.nextn.hc_head_up",    B_REP, S_W, 0, 0, 0 }, // 51 (integration tree)
};
static const int NT = (int) (sizeof(T) / sizeof(T[0]));
static const int NT_PINNED = 49;  // 0..48 are in the pinned llama-arch.cpp

// qwen4exp.cpp:189-284 -- which tensor ids a layer kind declares.
static bool present_oracle(int kind, int t) {
    if (kind < 0 || kind > 2) return false;
    if (t <= 4) return true;
    if (t == 5) return false;                 // gated by the PLE metadata
    if (t >= 6 && t <= 13) return true;       // both HC modules, every layer
    if (t >= 14 && t <= 21) return kind != 1; // attention branch
    if (t >= 22 && t <= 30) return kind == 1; // gated delta net branch only
    if (t >= 31 && t <= 36) return kind == 1; // PLE rides the recurrent layer
    if (t >= 46) return kind == 2;            // nextn head
    return true;                              // experts, every built layer
}

// tn(id, suffix, il).str() for the name table above.
static std::string tn_str(const char * pattern, suffix_t s, int il) {
    char buf[128];
    if (strstr(pattern, "%d")) {
        snprintf(buf, sizeof buf, pattern, il);
    } else {
        snprintf(buf, sizeof buf, "%s", pattern);
    }
    std::string out = buf;
    if (s == S_W) out += ".weight";
    else if (s == S_B) out += ".bias";
    return out;
}

extern "C" void q4t_fail(int64_t code) {
    ++g_fails;
    printf("FAIL shim: goose abort %lld\n", (long long) code);
}

static void check_name(int t, int il, const char * tag) {
    std::string buf(128, '\0');
    const int64_t n = q4t_pattern(t, u8buf(buf));
    std::string pat(buf.data(), (size_t) (n < 0 ? 0 : n));
    // format the pattern the way tn().str() would, then compare whole names
    char fmt[128];
    if (strstr(T[t].name, "%d")) {
        snprintf(fmt, sizeof fmt, pat.c_str(), il);
    } else {
        snprintf(fmt, sizeof fmt, "%s", pat.c_str());
    }
    std::string got = fmt;
    if (T[t].suffix == S_W) got += ".weight";
    else if (T[t].suffix == S_B) got += ".bias";
    expect_str(tag, got, tn_str(T[t].name, T[t].suffix, il));
}

int main() {
    goose_init(0, nullptr);

    // 1. id space identity
    ++g_cases;
    expect_i64("tensor-count", q4t_count(), NT);

    // 2. name patterns, all 49 pinned tensors
    ++g_cases;
    for (int t = 0; t < NT_PINNED; ++t) {
        std::string buf(128, '\0');
        const int64_t n = q4t_pattern(t, u8buf(buf));
        expect_str(("pattern-" + std::to_string(t)).c_str(),
                   std::string(buf.data(), (size_t) (n < 0 ? 0 : n)), T[t].name);
    }

    // 3. fully formatted names for a real layer index
    ++g_cases;
    for (int t = 0; t < NT; ++t) {
        check_name(t, 7, ("name7-" + std::to_string(t)).c_str());
    }

    // 4. layer arity from the bucket
    ++g_cases;
    for (int t = 0; t < NT; ++t) {
        expect_i64(("has-layer-" + std::to_string(t)).c_str(),
                   q4t_has_layer(t), T[t].bucket == B_REP ? 1 : 0);
    }

    // 5. bucket
    ++g_cases;
    for (int t = 0; t < NT; ++t) {
        expect_i64(("bucket-" + std::to_string(t)).c_str(), q4t_bucket(t), T[t].bucket);
    }

    // 6. declared suffix
    ++g_cases;
    for (int t = 0; t < NT; ++t) {
        expect_i64(("suffix-" + std::to_string(t)).c_str(), q4t_suffix(t), T[t].suffix);
    }

    // 7. loader flags: required unless the native call says otherwise
    ++g_cases;
    {
        const int64_t F_NOT_REQUIRED = 1, F_READ_LAZY = 4;
        for (int t = 0; t < NT; ++t) {
            int64_t want = 0;
            if (T[t].not_required) want |= F_NOT_REQUIRED;
            if (T[t].read_lazy) want |= F_READ_LAZY;
            expect_i64(("flags-" + std::to_string(t)).c_str(), q4t_flags(t), want);
        }
    }

    // 8. TENSOR_SKIP_IF_VIRTUAL on the fused forms
    ++g_cases;
    for (int t = 0; t < NT; ++t) {
        expect_i64(("skipv-" + std::to_string(t)).c_str(), q4t_skip_if_virtual(t),
                   T[t].skip_if_virtual);
    }

    // 9. the output fallback duplicates token_embd
    ++g_cases;
    expect_i64("output-fallback", q4t_output_fallback(), 1);

    // 10. presence per layer kind
    ++g_cases;
    for (int kind = 0; kind < 3; ++kind) {
        for (int t = 0; t < NT; ++t) {
            expect_i64(("present-" + std::to_string(kind) + "-" + std::to_string(t)).c_str(),
                       q4t_present(kind, t), present_oracle(kind, t) ? 1 : 0);
        }
    }

    // 11. the PLE table is global and metadata-gated
    ++g_cases;
    expect_i64("ple-table-0", q4t_present_ple_table(0), 0);
    expect_i64("ple-table-16", q4t_present_ple_table(16), 1);

    // 12. out of range ids
    ++g_cases;
    {
        std::string buf(64, '\0');
        expect_i64("pattern-oob", q4t_pattern(NT, u8buf(buf)), -1);
        expect_i64("pattern-oob2", q4t_pattern(-1, u8buf(buf)), -1);
        expect_i64("has-layer-oob", q4t_has_layer(NT), -1);
        expect_i64("bucket-oob", q4t_bucket(-1), -1);
        expect_i64("suffix-oob", q4t_suffix(NT), -1);
        expect_i64("flags-oob", q4t_flags(-1), -1);
    }

    // 13. the integration-tree extension group, named separately
    ++g_cases;
    {
        int ext_ok = 0;
        for (int t = NT_PINNED; t < NT; ++t) {
            std::string buf(128, '\0');
            const int64_t n = q4t_pattern(t, u8buf(buf));
            if (std::string(buf.data(), (size_t) (n < 0 ? 0 : n)) == T[t].name) ++ext_ok;
        }
        expect_i64("nextn-hc-group", ext_ok, NT - NT_PINNED);
    }

    printf("C1_02_Q4T cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
