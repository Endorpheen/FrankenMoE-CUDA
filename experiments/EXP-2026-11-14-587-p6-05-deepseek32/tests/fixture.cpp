// Structure gate for the deepseek32 builder draft (P6-05 architecture 8).
//
// Expected values are the native loader declarations in
// src/models/deepseek32.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-gen.h"

static int g_cases = 0, g_values = 0, g_fails = 0;

static void bad(const char * what, const std::string & detail) {
    ++g_fails;
    if (g_fails <= 100) {
        printf("FAIL %s: %s\n", what, detail.c_str());
    }
}

static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

extern "C" void fg200g_fail(int64_t code) {
    ++g_fails;
    if (g_fails <= 100) {
        printf("FAIL shim: native abort %lld\n", (long long) code);
    }
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base; (void) index;
    return 0;
}

// DeepSeek V3.2-scale geometry. n_expert_shared is 2 here (the released model
// uses 1) so the shared-expert width multiplier is exercised by the gate.
static std::vector<int64_t> params() {
    std::vector<int64_t> p(22, 0);
    p[0]  = 7168;     // n_embd
    p[1]  = 129280;   // n_vocab
    p[2]  = 61;       // n_layer
    p[3]  = 62;       // n_layer_all (one MTP block)
    p[4]  = 1;        // n_layer_nextn
    p[5]  = 128;      // n_head
    p[6]  = 18432;    // n_ff (dense lead)
    p[7]  = 256;      // n_expert
    p[8]  = 8;        // n_expert_used
    p[9]  = 2048;     // n_ff_exp declared
    p[10] = 2;        // n_expert_shared
    p[11] = 3;        // n_layer_dense_lead
    p[12] = 1536;     // q_lora_rank
    p[13] = 512;      // kv_lora_rank
    p[14] = 192;      // n_embd_head_k_mla
    p[15] = 128;      // n_embd_head_v_mla
    p[16] = 64;       // n_rot
    p[17] = 64;       // indexer_n_head
    p[18] = 128;      // indexer_head_size
    p[19] = 2048;     // indexer_top_k
    p[20] = 0;        // expert gating func: softmax
    p[21] = 1;        // has output
    return p;
}

static void check_kind(const char * name, int64_t il, int64_t expected, std::vector<int64_t> & p) {
    ++g_cases;
    const int64_t got = ds3_kind(sli(p), il);
    ++g_values;
    if (got != expected) {
        bad(name, "kind got " + std::to_string((int) got) + " expected " + std::to_string((int) expected));
    }
}

static void check_shape(const char * name, std::vector<int64_t> & p, int64_t kind, int64_t tid,
                        int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = ds3_shape(sli(p), kind, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((int) out[0]) + "," + std::to_string((int) out[1]) + "," +
                  std::to_string((int) out[2]) + "} ok=" + std::to_string((int) ok) +
                  " expected {" + std::to_string((int) a) + "," + std::to_string((int) b) + "," +
                  std::to_string((int) c) + "}");
    }
}

// The kind does not own the tensor: shape must report 0.
static void check_absent(const char * name, std::vector<int64_t> & p, int64_t kind, int64_t tid) {
    ++g_cases;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = ds3_shape(sli(p), kind, tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "expected absent, ok=" + std::to_string((int) ok));
    }
}

static void check_dec(const char * name, int64_t got, int64_t expected) {
    ++g_cases;
    ++g_values;
    if (got != expected) {
        bad(name, "got " + std::to_string((int) got) + " expected " + std::to_string((int) expected));
    }
}

static void check_plan(std::vector<int64_t> & p) {
    ++g_cases;
    std::vector<int64_t> kinds(62, -2);
    const int64_t n = ds3_plan(sli(p), sli(kinds));
    g_values += 5;
    if (n != 62) {
        bad("plan-length", "got " + std::to_string((int) n) + " expected 62");
    }
    if (kinds[0] != 0)  { bad("plan-layer0", "kind " + std::to_string((int) kinds[0])); }
    if (kinds[2] != 0)  { bad("plan-layer2", "kind " + std::to_string((int) kinds[2])); }
    if (kinds[3] != 1)  { bad("plan-layer3", "kind " + std::to_string((int) kinds[3])); }
    if (kinds[61] != 3) { bad("plan-layer61", "kind " + std::to_string((int) kinds[61])); }
}

int main() {
    goose_init(0, nullptr);
    auto p = params();

    // Layer plan: deepseek32.cpp:81-83 and :109-112.
    check_kind("layer0-dense", 0, 0, p);
    check_kind("layer2-dense", 2, 0, p);
    check_kind("layer3-moe", 3, 1, p);
    check_kind("layer60-moe", 60, 1, p);
    check_kind("layer61-mtp", 61, 3, p);
    check_kind("layer62-out-of-range", 62, -1, p);
    check_plan(p);

    // Global tensors, deepseek32.cpp:71-79.
    check_shape("token_embd", p, 0, 0, 7168, 129280, 1);
    check_shape("output_norm", p, 0, 1, 7168, 1, 1);
    check_shape("output", p, 0, 2, 7168, 129280, 1);

    // MLA tensors, deepseek32.cpp:86-101.
    check_shape("attn_norm", p, 0, 3, 7168, 1, 1);
    check_shape("attn_q_a_norm", p, 0, 4, 1536, 1, 1);
    check_shape("attn_kv_a_norm", p, 0, 5, 512, 1, 1);
    check_shape("wq_a", p, 0, 6, 7168, 1536, 1);
    check_shape("wq_b", p, 0, 7, 1536, 24576, 1);
    check_shape("wkv_a_mqa", p, 0, 8, 7168, 576, 1);
    check_shape("wk_b", p, 0, 9, 128, 512, 128);
    check_shape("wv_b", p, 0, 10, 512, 128, 128);
    check_shape("wo", p, 0, 11, 16384, 7168, 1);
    check_shape("ffn_norm", p, 1, 12, 7168, 1, 1);

    // DSA indexer, deepseek32.cpp:103-108; also present on the MTP layer.
    check_shape("indexer_k_norm", p, 1, 13, 128, 1, 1);
    check_shape("indexer_k_norm_b", p, 1, 14, 128, 1, 1);
    check_shape("indexer_proj", p, 1, 15, 7168, 64, 1);
    check_shape("indexer_attn_k", p, 1, 16, 7168, 128, 1);
    check_shape("indexer_attn_q_b", p, 1, 17, 1536, 8192, 1);
    check_shape("mtp-indexer-attn_k", p, 3, 16, 7168, 128, 1);

    // Dense lead FFN, deepseek32.cpp:109-112.
    check_shape("dense-ffn-gate", p, 0, 18, 7168, 18432, 1);
    check_shape("dense-ffn-down", p, 0, 19, 18432, 7168, 1);
    check_shape("dense-ffn-up", p, 0, 20, 7168, 18432, 1);
    check_absent("moe-has-no-dense-gate", p, 1, 18);

    // Routed + shared experts, deepseek32.cpp:113-133.
    check_shape("moe-gate-inp", p, 1, 21, 7168, 256, 1);
    check_shape("moe-exp-probs-b", p, 1, 22, 256, 1, 1);
    check_shape("moe-gate-exps", p, 1, 23, 7168, 2048, 256);
    check_shape("moe-up-exps", p, 1, 25, 7168, 2048, 256);
    check_shape("moe-down-exps", p, 1, 24, 2048, 7168, 256);
    check_shape("shexp-gate", p, 1, 26, 7168, 4096, 1);
    check_shape("shexp-down", p, 1, 27, 4096, 7168, 1);
    check_shape("shexp-up", p, 1, 28, 7168, 4096, 1);

    // NextN/MTP tensors, deepseek32.cpp:135-145.
    check_shape("mtp-eh-proj", p, 3, 29, 14336, 7168, 1);
    check_shape("mtp-enorm", p, 3, 30, 7168, 1, 1);
    check_shape("mtp-hnorm", p, 3, 31, 7168, 1, 1);
    check_shape("mtp-shared-head-head", p, 3, 33, 7168, 129280, 1);
    check_shape("mtp-shared-head-norm", p, 3, 34, 7168, 1, 1);
    check_absent("trunk-has-no-eh-proj", p, 1, 29);

    // Decisions: MLA gate, indexer use, top-k clamp, head split, gating.
    check_dec("is-mla", ds3_is_mla(192, 128), 1);
    check_dec("is-not-mla", ds3_is_mla(0, 0), 0);
    check_dec("require-mla", ds3_require_mla(sli(p)), 1);
    check_dec("indexer-trunk", ds3_indexer_used(1), 1);
    check_dec("indexer-mtp", ds3_indexer_used(3), 0);
    check_dec("top-k-cached", ds3_top_k(1024, 2048), 1024);
    check_dec("top-k-cap", ds3_top_k(4096, 2048), 2048);
    check_dec("gating-softmax", ds3_gating(sli(p)), 0);
    check_dec("output-optional", ds3_output_optional(), 1);
    {
        ++g_cases;
        std::vector<int64_t> out(3, -1);
        const int64_t ok = ds3_qk_split(sli(p), sli(out));
        g_values += 3;
        if (ok != 1 || out[0] != 128 || out[1] != 64 || out[2] != 192) {
            bad("qk-split", "got {" + std::to_string((int) out[0]) + "," + std::to_string((int) out[1]) +
                            "," + std::to_string((int) out[2]) + "} ok=" + std::to_string((int) ok));
        }
    }

    printf("FG-ARCH8 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
