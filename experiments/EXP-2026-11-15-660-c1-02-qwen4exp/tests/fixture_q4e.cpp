// C1-02 slice E gate: qwen4exp expert and token-mixer layout against a
// transcription of the native loader.
//
// Oracle: independent transcription of
//   src/models/qwen4exp.cpp:139-145, 240-243, 272-281
//   src/llama-model.cpp:3138-3180
// No native library is linked. The slice imports the accepted
// base/model_graph.goose, so its shims are the base ones.

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

// ---- native transcription --------------------------------------------------

struct dims { int64_t a, b, c; };

// qwen4exp.cpp:139-140.
static int64_t n_ff_exp_oracle(int64_t declared, int64_t n_ff, int64_t used) {
    return declared ? declared : n_ff / used;
}
static int64_t n_ff_shexp_oracle(int64_t declared, int64_t n_ff) {
    return declared ? declared : n_ff;
}

// qwen4exp.cpp:240 -- create_tensor_qkv q width is n_embd_head_k * n_head * 2.
static int64_t q_width_oracle(int64_t head_k, int64_t n_head) {
    return head_k * n_head * 2;
}

extern "C" void fg200g_fail(int64_t code) {
    ++g_fails;
    printf("FAIL shim: goose abort %lld\n", (long long) code);
}
extern "C" void fg200g_st_i64(uint64_t base, int64_t byte, int64_t v) {
    (void) base; (void) byte; (void) v;
}
extern "C" int64_t fg200g_ld_u32(uint64_t base, int64_t index) {
    (void) base; (void) index;
    return 0;
}

static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

static void check3(const char * what, int64_t ok, const std::vector<int64_t> & out, dims want) {
    if (ok != 1 || out[0] != want.a || out[1] != want.b || out[2] != want.c) {
        bad(what, "ok=" + std::to_string((long long) ok) + " got {" +
                  std::to_string((long long) out[0]) + "," +
                  std::to_string((long long) out[1]) + "," +
                  std::to_string((long long) out[2]) + "} want {" +
                  std::to_string((long long) want.a) + "," +
                  std::to_string((long long) want.b) + "," +
                  std::to_string((long long) want.c) + "}");
    }
}

int main() {
    goose_init(0, nullptr);

    // 1. routed expert width resolution
    ++g_cases;
    {
        const int64_t D[] = { 0, 0, 1024, 2048 };
        const int64_t F[] = { 8192, 8192, 8192, 8192 };
        const int64_t U[] = { 8, 4, 8, 8 };
        for (int i = 0; i < 4; ++i) {
            expect_i64("n-ff-exp", q4e_n_ff_exp(D[i], F[i], U[i]),
                       n_ff_exp_oracle(D[i], F[i], U[i]));
        }
    }

    // 2. shared expert width
    ++g_cases;
    {
        expect_i64("n-ff-shexp-0", q4e_n_ff_shexp(0, 8192), n_ff_shexp_oracle(0, 8192));
        expect_i64("n-ff-shexp-1", q4e_n_ff_shexp(2048, 8192), n_ff_shexp_oracle(2048, 8192));
    }

    // 3. routed down shape, three configurations
    ++g_cases;
    {
        struct c { int64_t ff, embd, ne; } C[] = {
            { 1024, 2048, 256 }, { 512, 4096, 128 }, { 2048, 2048, 64 },
        };
        for (const auto & c : C) {
            std::vector<int64_t> out(3, -1);
            const int64_t n_ff_exp = n_ff_exp_oracle(0, 8192, 8);
            const int64_t ok = q4e_down_shape(n_ff_exp, c.embd, c.ne, sli(out));
            check3("down-shape", ok, out, { n_ff_exp, c.embd, c.ne });
        }
    }

    // 4. fused gate_up shape
    ++g_cases;
    {
        std::vector<int64_t> out(3, -1);
        const int64_t n_ff_exp = n_ff_exp_oracle(0, 8192, 8);
        const int64_t ok = q4e_gate_up_shape(2048, n_ff_exp, 256, sli(out));
        check3("gate-up-shape", ok, out, { 2048, n_ff_exp * 2, 256 });
    }

    // 5. split gate shape
    ++g_cases;
    {
        std::vector<int64_t> out(3, -1);
        const int64_t n_ff_exp = n_ff_exp_oracle(0, 8192, 4);
        const int64_t ok = q4e_gate_shape(4096, n_ff_exp, 128, sli(out));
        check3("gate-shape", ok, out, { 4096, n_ff_exp, 128 });
    }

    // 6. the gate_up presence probe decides fused or split
    ++g_cases;
    {
        expect_i64("gate-up-mode-1", q4e_gate_up_mode(1), 1);
        expect_i64("gate-up-mode-0", q4e_gate_up_mode(0), 0);
        expect_i64("routed-count-fused", q4e_routed_count(1), 2);
        expect_i64("routed-count-split", q4e_routed_count(0), 3);
    }

    // 7. shared expert set, four slots plus out-of-range
    ++g_cases;
    {
        const int64_t ne = 4096, nf = 2048;
        struct c { int64_t slot; dims want; } C[] = {
            { 0, { ne, 1, 1 } },        // gate_inp is 1-D
            { 1, { ne, nf, 1 } },
            { 2, { ne, nf, 1 } },
            { 3, { nf, ne, 1 } },       // down is transposed
        };
        for (const auto & c : C) {
            std::vector<int64_t> out(3, -1);
            const int64_t ok = q4e_shexp_shape(c.slot, ne, nf, sli(out));
            check3("shexp-shape", ok, out, c.want);
        }
        std::vector<int64_t> out(3, -1);
        expect_i64("shexp-oob", q4e_shexp_shape(4, ne, nf, sli(out)), 0);
    }

    // 8. element counts
    ++g_cases;
    {
        expect_i64("shexp-elems", q4e_shexp_elems(2048, 2048), 2048LL * 2048);
        expect_i64("routed-elems", q4e_routed_elems(1024, 2048, 256), 1024LL * 2048 * 256);
        expect_i64("routed-elems-2", q4e_routed_elems(512, 4096, 128), 512LL * 4096 * 128);
    }

    // 9. attention widths, three head configurations
    ++g_cases;
    {
        struct c { int64_t hk, nh, nkv; } C[] = {
            { 128, 16, 4 }, { 64, 32, 8 }, { 256, 8, 2 },
        };
        for (const auto & c : C) {
            std::vector<int64_t> out(5, -1);
            const int64_t ok = q4e_attn_widths(c.hk, c.nh, c.nkv, sli(out));
            ++g_values;
            if (ok != 1 || out[0] != c.hk || out[1] != c.hk ||
                out[2] != q_width_oracle(c.hk, c.nh) ||
                out[3] != c.hk * c.nkv || out[4] != c.hk * c.nkv) {
                bad("attn-widths", "config mismatch");
            }
        }
    }

    // 10. fused qkv row width
    ++g_cases;
    {
        expect_i64("qkv-width", q4e_qkv_width(5120, 512, 512), 6144);
        expect_i64("qkv-width-2", q4e_qkv_width(1024, 128, 128), 1280);
        expect_i64("qkv-mode-fused", q4e_qkv_mode(1), 1);
        expect_i64("qkv-mode-split", q4e_qkv_mode(0), 0);
    }

    // 11. the head-width equality the attention block asserts
    ++g_cases;
    {
        expect_i64("head-dim-eq", q4e_head_dim_ok(128, 128), 1);
        expect_i64("head-dim-ne", q4e_head_dim_ok(128, 64), 0);
    }

    // 12. experts exist on every built layer kind
    ++g_cases;
    {
        expect_i64("experts-0", q4e_experts_on(0), 1);
        expect_i64("experts-1", q4e_experts_on(1), 1);
        expect_i64("experts-2", q4e_experts_on(2), 1);
        expect_i64("experts-oob", q4e_experts_on(3), 0);
        expect_i64("experts-neg", q4e_experts_on(-1), 0);
    }

    printf("C1_02_Q4E cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
