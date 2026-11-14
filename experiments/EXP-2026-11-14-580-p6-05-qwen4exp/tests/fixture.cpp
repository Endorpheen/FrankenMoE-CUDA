// Structure gate for the qwen4exp builder draft (P6-05 architecture 2).
//
// Expected values are the native loader's declarations in
// src/models/qwen4exp.cpp. No native library is linked.

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
    printf("FAIL shim: native abort %lld\n", (long long) code);
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base; (void) index;
    return 0;
}

static std::vector<int64_t> params() {
    std::vector<int64_t> p(27, 0);
    p[0]  = 2048;     // n_embd
    p[1]  = 151936;   // n_vocab
    p[2]  = 48;       // n_layer
    p[3]  = 49;       // n_layer_all
    p[4]  = 16;       // n_head
    p[5]  = 4;        // n_head_kv
    p[6]  = 8192;     // n_ff
    p[7]  = 256;      // n_expert
    p[8]  = 8;        // n_expert_used
    p[9]  = 0;        // n_ff_exp declared
    p[10] = 0;        // n_ff_shexp declared
    p[11] = 4;        // ssm_d_conv
    p[12] = 4096;     // ssm_d_inner
    p[13] = 128;      // ssm_d_state
    p[14] = 32;       // ssm_dt_rank
    p[15] = 4;        // ssm_n_group
    p[16] = 128;      // n_embd_head_k
    p[17] = 4;        // full_attention_interval
    p[18] = 4;        // hc mult
    p[19] = 512;      // hc low rank
    p[20] = 4;        // indexer n_head
    p[21] = 128;      // indexer head size / idx_dim
    p[22] = 64;       // ple head dim
    p[23] = 65536;    // ple rows
    p[24] = 4;        // ple conv kernel
    p[25] = 0;        // ple layer
    p[26] = 1;        // has output
    return p;
}

static void check_kind(const char * name, int64_t il, int64_t expected, const std::vector<int64_t> & p) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    const int64_t got = q4_kind(sli(pp), il, 0);
    ++g_values;
    if (got != expected) {
        bad(name, "kind got " + std::to_string((int) got) + " expected " + std::to_string((int) expected));
    }
}

static void check_shape(const char * name, const std::vector<int64_t> & p, int64_t kind, int64_t tid,
                        int64_t a, int64_t b, int64_t c) {
    ++g_cases;
    std::vector<int64_t> pp = p;
    std::vector<int64_t> out(3, -1);
    const int64_t ok = q4_shape(sli(pp), kind, tid, sli(out));
    g_values += 3;
    if (ok != 1 || out[0] != a || out[1] != b || out[2] != c) {
        bad(name, "got {" + std::to_string((int) out[0]) + "," + std::to_string((int) out[1]) + "," +
                  std::to_string((int) out[2]) + "} ok=" + std::to_string((int) ok) +
                  " expected {" + std::to_string((int) a) + "," + std::to_string((int) b) + "," +
                  std::to_string((int) c) + "}");
    }
}

int main() {
    goose_init(0, nullptr);
    const auto p = params();

    // 1. token_embd.weight, qwen4exp.cpp:182
    check_shape("token_embd", p, 0, 0, 2048, 151936, 1);
    // 2. hc_head_norm.weight, qwen4exp.cpp:186
    check_shape("hc-head-norm", p, 0, 1, 2048, 4, 1);
    // 3. layer 0 recurrent: (0+1)%4 != 0, qwen4exp.cpp:173-181
    check_kind("layer0-recurrent", 0, 1, p);
    // 4. layer 3 attention: (3+1)%4 == 0
    check_kind("layer3-attention", 3, 0, p);
    // 5. recurrent attn_qkv.weight, qwen4exp.cpp:255
    check_shape("recurrent-wqkv", p, 1, 22, 2048, 5120, 1);
    // 6. attention fused attn_qkv.weight, qwen4exp.cpp:243
    check_shape("attention-qkv", p, 0, 14, 2048, 5120, 1);
    // 7. indexer.q_proj.weight, qwen4exp.cpp:249
    check_shape("indexer-q-proj", p, 0, 18, 2048, 512, 1);
    // 8. routed ffn_down_exps.weight, qwen4exp.cpp:276
    check_shape("ffn-down-exps", p, 0, 38, 1024, 2048, 256);
    // 9. hc_attn_down.weight, qwen4exp.cpp:233
    check_shape("hc-attn-down", p, 0, 7, 8192, 512, 1);
    // 10. MTP nextn.eh_proj.weight, qwen4exp.cpp:294
    check_shape("mtp-eh-proj", p, 2, 46, 4096, 2048, 1);

    printf("FG579_ARCH2 cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
