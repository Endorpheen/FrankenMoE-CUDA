// Structure gate for the glm4 builder draft (P6-05 architecture 14).
//
// Expected values are the native loader's declarations in
// src/models/glm4.cpp. No native library is linked.

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "runtime-arch14.h"

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
    printf("FAIL shim: native abort %lld\n", (long long) code);
}

static std::vector<int64_t> params() {
    std::vector<int64_t> p(11, 0);
    p[0]  = 4096;     // n_embd
    p[1]  = 151552;   // n_vocab
    p[2]  = 40;       // n_layer (GLM-4-9B size class, glm4.cpp:13)
    p[3]  = 1;        // n_layer_nextn
    p[4]  = 32;       // n_head
    p[5]  = 2;        // n_head_kv
    p[6]  = 13696;    // n_ff
    p[7]  = 128;      // n_embd_head_k
    p[8]  = 128;      // n_embd_head_v
    p[9]  = 0;        // output absent -> duplicates token_embd
    p[10] = 0;        // no fused attention bias
    return p;
}

// Derived: n_layer_all = 41, q_width = 32*128 = 4096, k_width = v_width =
//          2*128 = 256, fused qkv = 4608, ffn_up = 2*13696 = 27392.

static void check_val(const char * name, int64_t got, int64_t want) {
    ++g_cases;
    ++g_values;
    if (got != want) {
        bad(name, "got " + std::to_string((long long) got) + " expected " + std::to_string((long long) want));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, int64_t kind,
                        int64_t tid, int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = gl4_shape(sli(pp), kind, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((long long) out[0]) + "," + std::to_string((long long) out[1]) +
                  "," + std::to_string((long long) out[2]) + "} ok=" + std::to_string((long long) ok) +
                  " expected {" + std::to_string((long long) a) + "," + std::to_string((long long) b) +
                  "," + std::to_string((long long) c) + "}");
    }
}

static void check_absent(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t tid) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = gl4_shape(sli(pp), kind, tid, sli(out));
    ++g_values;
    if (ok != 0) {
        bad(name, "tensor present (ok=" + std::to_string((long long) ok) + "), expected absent");
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();

    // 1. plan: 40 trunk blocks plus one NextN block (glm4.cpp:8-9, :32-64)
    {
        std::vector<int64_t> pp = p, kinds(41, -1);
        const int64_t n = gl4_plan(sli(pp), sli(kinds));
        check_val("plan-len", n, 41);
        check_val("plan-layer0-trunk", kinds[0], 0);
        check_val("plan-layer39-trunk", kinds[39], 0);
        check_val("plan-layer40-nextn", kinds[40], 1);
        check_val("n-layer-all", g4_n_layer_all(sli(pp)), 41);
    }
    // 2. per-block kind, including the out-of-range answers
    {
        std::vector<int64_t> pp = p;
        check_val("kind-layer0", g4_kind(sli(pp), 0), 0);
        check_val("kind-layer40", g4_kind(sli(pp), 40), 1);
        check_val("kind-layer41-rejected", g4_kind(sli(pp), 41), -1);
        check_val("kind-negative-rejected", g4_kind(sli(pp), -1), -1);
    }
    // 3. NextN assert: n_layer_nextn < n_layer_all (glm4.cpp:9)
    {
        std::vector<int64_t> pp = p;
        check_val("nextn-ok", g4_nextn_ok(sli(pp)), 1);
        std::vector<int64_t> bad_p = p;
        bad_p[2] = 0;   // no trunk blocks, one NextN block
        bad_p[3] = 0;   // and none declared: 0 < 0 is false
        check_val("nextn-not-below-all", g4_nextn_ok(sli(bad_p)), 0);
    }

    // 4. widths: fused QKV and the SWIGLU sequence row (glm4.cpp:42, :50)
    {
        std::vector<int64_t> pp = p;
        check_val("q-width", g4_q_width(sli(pp)), 4096);
        check_val("k-width", g4_k_width(sli(pp)), 256);
        check_val("v-width", g4_v_width(sli(pp)), 256);
        check_val("qkv-width", g4_qkv_width(sli(pp)), 4608);
        check_val("ffn-up-width", g4_ffn_up_width(sli(pp)), 27392);
        check_val("nextn-eh-proj", g4_nextn_eh_proj(sli(pp)), 8192);
        check_val("head-split-ok", g4_head_split_ok(sli(pp)), 1);
        std::vector<int64_t> pv = p;
        pv[8] = 64;
        check_val("head-split-rejected", g4_head_split_ok(sli(pv)), 0);
    }

    // 5. output is optional and falls back to the token embedding (:27-30)
    {
        std::vector<int64_t> pp = p;
        check_val("output-from-token-embd", g4_output_dup(sli(pp)), 1);
        pp[9] = 1;
        check_val("output-declared", g4_output_dup(sli(pp)), 0);
    }

    // 6. global tensors (glm4.cpp:22-30)
    check_shape("token-embd", p, 0, 0, 4096, 151552, 1);
    check_shape("output-norm", p, 0, 1, 4096, 1, 1);
    check_shape("output-dup-token-embd", p, 0, 2, 4096, 151552, 1);

    // 7. attention and the two post-norms (glm4.cpp:41-48)
    check_shape("attn-norm", p, 0, 3, 4096, 1, 1);
    check_shape("attn-qkv", p, 0, 4, 4096, 4608, 1);
    check_shape("attn-wo", p, 0, 9, 4096, 4096, 1);
    check_shape("attn-post-norm", p, 0, 10, 4096, 1, 1);

    // 8. FFN: no gate row, the up row carries gate and up (glm4.cpp:49-52)
    check_shape("ffn-norm", p, 0, 11, 4096, 1, 1);
    check_shape("ffn-up-fused", p, 0, 12, 4096, 27392, 1);
    check_shape("ffn-down", p, 0, 13, 13696, 4096, 1);
    check_shape("ffn-post-norm", p, 0, 14, 4096, 1, 1);
    check_absent("no-dense-gate-row", p, 0, 99);

    // 9. bias set: legacy q/k/v when no fused row exists (llama-model.cpp:3169-3174)
    check_absent("fused-bias-absent", p, 0, 5);
    check_shape("legacy-q-bias", p, 0, 6, 4096, 1, 1);
    check_shape("legacy-k-bias", p, 0, 7, 256, 1, 1);
    check_shape("legacy-v-bias", p, 0, 8, 256, 1, 1);
    {
        std::vector<int64_t> pb = p;
        pb[10] = 1;
        check_shape("fused-qkv-bias", pb, 0, 5, 4608, 1, 1);
        check_absent("legacy-q-bias-suppressed", pb, 0, 6);
    }

    // 10. a NextN block declares the same trunk rows and adds its own four
    //     (glm4.cpp:32-64)
    check_shape("nextn-block-attn-norm", p, 1, 3, 4096, 1, 1);
    check_shape("nextn-block-ffn-up", p, 1, 12, 4096, 27392, 1);
    check_shape("nextn-eh-proj", p, 1, 15, 8192, 4096, 1);
    check_shape("nextn-enorm", p, 1, 16, 4096, 1, 1);
    check_shape("nextn-hnorm", p, 1, 17, 4096, 1, 1);
    check_shape("nextn-embed", p, 1, 18, 4096, 151552, 1);
    check_shape("nextn-shared-head", p, 1, 19, 4096, 151552, 1);
    check_shape("nextn-shared-head-norm", p, 1, 20, 4096, 1, 1);

    // 11. the trunk block never claims a NextN row (glm4.cpp:54-55 guard)
    check_absent("trunk-no-eh-proj", p, 0, 15);
    check_absent("trunk-no-enorm", p, 0, 16);
    check_absent("trunk-no-shared-head", p, 0, 19);

    printf("FG-ARCH14 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
