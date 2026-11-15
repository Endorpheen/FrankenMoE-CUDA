// C1-02 slice M gate: qwen4exp metadata adapter against a transcription of
// the native reader.
//
// The oracle below is an independent transcription of
//   src/models/qwen4exp.cpp:5-107   (load_arch_hparams)
//   src/llama-arch.cpp:191-337      (the key table)
//   src/llama-hparams.cpp:243-250   (ple_conv_state)
//   src/llama-hparams.cpp:17-40     (set_recr_pattern)
//   src/llama-hparams.cpp:111-113   (n_embd_out)
// No native library is linked: the gate proves the port's rules and ordering,
// not native numeric execution.

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

static sl_i64 sli(std::vector<int64_t> & v) {
    return sl_i64{ v.data(), (int64_t) v.size() };
}

// ---- native transcription --------------------------------------------------

enum req_t { REQ_ALWAYS, REQ_OPT, REQ_PLE };

// llama-arch.cpp:191-337, restricted to the keys qwen4exp reads.
struct keyrow { const char * suffix; req_t req; int arity; };

static const keyrow K[] = {
    { "expert_feed_forward_length",        REQ_OPT,    0 }, // optional
    { "expert_shared_feed_forward_length", REQ_OPT,    0 }, // optional
    { "attention.layer_norm_rms_epsilon",  REQ_ALWAYS, 0 },
    { "rope.dimension_sections",           REQ_ALWAYS, 1 }, // get_key_or_arr(..., 4, true)
    { "ssm.conv_kernel",                   REQ_ALWAYS, 0 },
    { "ssm.inner_size",                    REQ_ALWAYS, 0 },
    { "ssm.state_size",                    REQ_ALWAYS, 0 },
    { "ssm.time_step_rank",                REQ_ALWAYS, 0 },
    { "ssm.group_count",                   REQ_ALWAYS, 0 },
    { "hyper_connection.count",            REQ_ALWAYS, 0 },
    { "hyper_connection.low_rank",         REQ_ALWAYS, 0 },
    { "attention.indexer.head_count",      REQ_ALWAYS, 0 },
    { "attention.indexer.key_length",      REQ_ALWAYS, 0 },
    { "attention.indexer.top_k",           REQ_ALWAYS, 0 },
    { "attention.compress_ratios",         REQ_OPT,    2 }, // get_key_or_arr(..., n_layer_all, false)
    { "ple.layers",                        REQ_OPT,    3 }, // get_arr_n(..., false)
    { "ple.ngram_size",                    REQ_PLE,   0 },
    { "ple.heads_per_ngram",               REQ_PLE,   0 },
    { "ple.conv_kernel",                   REQ_PLE,   0 },
    { "ple.layer_multipliers",             REQ_PLE,   3 },
    { "ple.head_offsets",                  REQ_PLE,   3 },
    { "ple.head_vocab_sizes",              REQ_PLE,   3 },
    { "ple.eos_token_id",                  REQ_PLE,   0 },
    { "ple.image_token_id",                REQ_OPT,   0 }, // ml.get_key(..., false)
    { "embedding_length_per_layer_input",  REQ_PLE,   0 },
    { "attention.recurrent_layers",        REQ_OPT,   2 }, // get_key_or_arr(..., n_layer_all, false)
    { "full_attention_interval",           REQ_OPT,   0 },
};
static const int NKEY = (int) (sizeof(K) / sizeof(K[0]));

// qwen4exp.cpp:44-72, in native order.
static int ple_error_oracle(int64_t n_ple, int64_t ple_layer, int64_t n_layer_all,
                            int64_t ngram, int64_t heads, int64_t conv, int64_t embd) {
    if (n_ple > 1) return 2;
    if (n_ple == 1) {
        if (ple_layer >= n_layer_all) return 1;
        if (!(conv > 0 && embd > 0)) return 3;
        if (!(ngram >= 2 && ngram <= 8)) return 4;
        if (!(heads > 0 && heads <= 64)) return 5;
    }
    return 0;
}

// qwen4exp.cpp:82-90.
static int64_t head_index_oracle(const std::vector<int64_t> & off,
                                const std::vector<int64_t> & voc, int64_t n) {
    for (int64_t h = 0; h < n; ++h) {
        if (voc[h] == 0 || off[h] > 2147483647LL || voc[h] > 2147483647LL ||
            off[h] + voc[h] > 2147483647LL) {
            return h;
        }
    }
    return -1;
}

// qwen4exp.cpp:132-137.
static int64_t head_row_oracle(const std::vector<int64_t> & off,
                               const std::vector<int64_t> & voc, int64_t n, int64_t rows) {
    for (int64_t h = 0; h < n; ++h) {
        if (off[h] + voc[h] > rows) return h;
    }
    return -1;
}

// qwen4exp.cpp:93-101.
static int64_t recr_oracle(int64_t il, int64_t n_layer, int64_t interval) {
    if (interval <= 0) return -1;
    return (il < n_layer && (il + 1) % interval != 0) ? 1 : 0;
}

extern "C" void q4m_fail(int64_t code) {
    ++g_fails;
    printf("FAIL shim: goose abort %lld\n", (long long) code);
}

int main() {
    goose_init(0, nullptr);

    // 1. key table identity
    ++g_cases;
    expect_i64("key-count", q4m_key_count(), NKEY);
    for (int k = 0; k < NKEY; ++k) {
        std::string buf(128, '\0');
        const int64_t n = q4m_key_name(k, u8buf(buf));
        expect_str(("key-name-" + std::to_string(k)).c_str(),
                   std::string(buf.data(), (size_t) (n < 0 ? 0 : n)), K[k].suffix);
    }

    // 2. full GGUF key
    ++g_cases;
    for (int k = 0; k < NKEY; ++k) {
        std::string buf(128, '\0');
        const int64_t n = q4m_key_full(k, u8buf(buf));
        expect_str(("key-full-" + std::to_string(k)).c_str(),
                   std::string(buf.data(), (size_t) (n < 0 ? 0 : n)),
                   std::string("qwen4exp.") + K[k].suffix);
    }

    // 3. requirement classes
    ++g_cases;
    for (int k = 0; k < NKEY; ++k) {
        expect_i64(("key-req-" + std::to_string(k)).c_str(), q4m_key_required(k), K[k].req);
    }

    // 4. arity classes
    ++g_cases;
    for (int k = 0; k < NKEY; ++k) {
        expect_i64(("key-arity-" + std::to_string(k)).c_str(), q4m_key_arity(k), K[k].arity);
    }

    // 5. out-of-range keys
    ++g_cases;
    {
        std::string buf(64, '\0');
        expect_i64("key-name-oob", q4m_key_name(-1, u8buf(buf)), -1);
        expect_i64("key-name-oob2", q4m_key_name(NKEY, u8buf(buf)), -1);
        expect_i64("key-req-oob", q4m_key_required(NKEY), -1);
        expect_i64("key-arity-oob", q4m_key_arity(-1), -1);
    }

    // 6. PLE group presence
    ++g_cases;
    expect_i64("ple-group-0", q4m_ple_group_present(0), 0);
    expect_i64("ple-group-1", q4m_ple_group_present(1), 1);
    expect_i64("ple-group-2", q4m_ple_group_present(2), 1);

    // 7. derived head count and head dim, (ngram-1)*per_ngram pre-check
    ++g_cases;
    {
        const int64_t ng[]  = { 2, 3, 4, 8 };
        const int64_t per[] = { 8, 8, 16, 8 };
        for (int i = 0; i < 4; ++i) {
            expect_i64("ple-heads", q4m_ple_heads(ng[i], per[i]), (ng[i] - 1) * per[i]);
        }
        expect_i64("ple-heads-zero-ngram", q4m_ple_heads(0, 8), -8);
        expect_i64("ple-head-dim", q4m_ple_head_dim(64), 64);
    }

    // 8. PLE validation order
    ++g_cases;
    {
        struct c { int64_t n_ple, layer, all, ngram, heads, conv, embd; } C[] = {
            { 0, 0, 49, 0, 0, 0, 0 },      // no PLE group: accepted
            { 2, 0, 49, 0, 0, 0, 0 },      // two layers: assert n_ple == 1
            { 1, 49, 49, 3, 16, 4, 64 },   // layer out of range
            { 1, 48, 49, 3, 16, 4, 64 },   // last layer: accepted
            { 1, 0, 49, 3, 16, 0, 64 },    // conv kernel not positive (before ngram)
            { 1, 0, 49, 1, 16, 4, 64 },    // ngram below 2
            { 1, 0, 49, 9, 16, 4, 64 },    // ngram above 8
            { 1, 0, 49, 3, 0, 4, 64 },     // no heads
            { 1, 0, 49, 3, 65, 4, 64 },    // too many heads
            { 1, 0, 49, 3, 64, 5, 128 },   // accepted upper bound
            { 1, 0, 49, 8, 64, 5, 128 },   // n-gram upper bound is accepted too
            { 1, 0, 49, 0, 0, 0, 0 },      // ngram 0 and conv 0: conv wins
        };
        for (const auto & c : C) {
            const int64_t want = ple_error_oracle(c.n_ple, c.layer, c.all, c.ngram,
                                                  c.heads, c.conv, c.embd);
            expect_i64("ple-error", q4m_ple_error(c.n_ple, c.layer, c.all, c.ngram,
                                                  c.heads, c.conv, c.embd), want);
        }
    }

    // 9. PLE head ranges must fit the int32 row index. Each guard is checked on
    // a single head as well, so a mutation cannot hide behind an earlier head.
    ++g_cases;
    {
        struct c { int64_t off, voc; } C[] = {
            { 0, 65536 },              // accepted
            { 0, 0 },                  // empty vocabulary
            { 2147483647LL, 1 },       // offset at the int32 maximum
            { 2147483646LL, 1 },       // last legal end
            { 1, 2147483647LL },       // vocabulary at the int32 maximum
            { 2147483648LL, 1 },       // offset one past the int32 maximum
        };
        for (const auto & c : C) {
            std::vector<int64_t> off = { c.off };
            std::vector<int64_t> voc = { c.voc };
            expect_i64("ple-idx-single", q4m_ple_head_index_errors(sli(off), sli(voc), 1),
                       head_index_oracle(off, voc, 1));
        }
        std::vector<int64_t> off = { 0, 65536, 2147483647, 0, 100 };
        std::vector<int64_t> voc = { 65536, 65536, 1, 0, 2147483647 };
        const int64_t n = 5;
        expect_i64("ple-idx-err", q4m_ple_head_index_errors(sli(off), sli(voc), n),
                   head_index_oracle(off, voc, n));
        std::vector<int64_t> ok_off = { 0, 65536, 131072 };
        std::vector<int64_t> ok_voc = { 65536, 65536, 65536 };
        expect_i64("ple-idx-ok", q4m_ple_head_index_errors(sli(ok_off), sli(ok_voc), 3),
                   head_index_oracle(ok_off, ok_voc, 3));
        expect_i64("ple-idx-short", q4m_ple_head_index_errors(sli(ok_off), sli(ok_voc), 4), -1);
    }

    // 10. PLE head ranges must stay inside the padded table rows
    ++g_cases;
    {
        std::vector<int64_t> off = { 0, 65536, 131072, 200000 };
        std::vector<int64_t> voc = { 65536, 65536, 65536, 65536 };
        const int64_t n = 4;
        expect_i64("ple-row-over", q4m_ple_row_errors({off.data(), n}, {voc.data(), n}, n, 250000),
                   head_row_oracle(off, voc, n, 250000));
        expect_i64("ple-row-fit", q4m_ple_row_errors({off.data(), n}, {voc.data(), n}, n, 265536),
                   head_row_oracle(off, voc, n, 265536));
    }

    // 11. conv history rows
    ++g_cases;
    {
        expect_i64("conv-state-zero", q4m_ple_conv_state(0, 4, 3, 4, 2048), 0);
        expect_i64("conv-state-zero-k", q4m_ple_conv_state(16, 0, 3, 4, 2048), 0);
        expect_i64("conv-state", q4m_ple_conv_state(16, 4, 3, 4, 2048), 3 * 3 * 4 * 2048);
        expect_i64("conv-state-2", q4m_ple_conv_state(24, 5, 4, 2, 4096), 4 * 4 * 2 * 4096);
    }

    // 12. hyper-connection, indexer, SSM guards
    ++g_cases;
    {
        expect_i64("hc-ok", q4m_hc_error(4, 512), 0);
        expect_i64("hc-zero", q4m_hc_error(0, 512), 1);
        expect_i64("hc-lr-zero", q4m_hc_error(4, 0), 1);
        expect_i64("idx-ok", q4m_indexer_error(4, 128, 8), 0);
        expect_i64("idx-zero", q4m_indexer_error(4, 0, 8), 1);
        expect_i64("idx-topk", q4m_indexer_error(4, 128, 0), 1);
        expect_i64("ssm-ok", q4m_ssm_error(4, 4096, 128, 32, 4), 0);
        expect_i64("ssm-group", q4m_ssm_error(4, 4096, 128, 32, 0), 1);
        expect_i64("ssm-inner", q4m_ssm_error(4, 0, 128, 32, 4), 1);
    }

    // 13. n_embd_out_impl
    ++g_cases;
    expect_i64("embd-out", q4m_embd_out(4, 2048), 8192);
    expect_i64("embd-out-2", q4m_embd_out(2, 4096), 8192);

    // 14. recurrent pattern, derived from the interval
    ++g_cases;
    {
        const int64_t interval = 4;
        for (int64_t il = 0; il < 49; ++il) {
            expect_i64("recr-flag", q4m_recr_flag(il, 48, interval), recr_oracle(il, 48, interval));
        }
        expect_i64("recr-interval0", q4m_recr_flag(0, 48, 0), -1);
        expect_i64("recr-interval-default", q4m_interval_default(), 4);
        // a different interval moves the full-attention layers
        for (int64_t il = 0; il < 8; ++il) {
            expect_i64("recr-flag-2", q4m_recr_flag(il, 4, 2), recr_oracle(il, 4, 2));
        }
    }

    // 15. model type
    ++g_cases;
    expect_i64("type-48", q4m_type(48), 1);
    expect_i64("type-47", q4m_type(47), 0);
    expect_i64("type-49", q4m_type(49), 0);

    printf("C1_02_Q4M cases=%d values=%d failures=%d\n", g_cases, g_values, g_fails);
    return g_fails == 0 ? 0 : 1;
}
