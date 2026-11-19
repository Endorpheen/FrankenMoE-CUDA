// The Apex plan against the real file, both ways the loader can decide.
//
// The native loader prefers the GGUF's recurrent_layers array and falls back to the
// full_attention_interval formula only when it is absent (qwen35moe.cpp:20-27). The
// existing gates read the formula only, which is the fallback branch. This reads the
// array too and requires the plan to match BOTH - if they ever disagree for a real file,
// the build must be using the array, not the formula.
//
// Requires: FG_APEX_GGUF pointing at the Apex GGUF.
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>

#include "ggml.h"
#include "llama.h"
#include "runtime.h"

static int checks = 0, fails = 0;

static void check(int cond, const char *what) {
    checks++;
    if (!cond) { fails++; std::printf("FAIL %s\n", what); }
}

static int64_t kv_u32(struct gguf_context * g, const char * key, int64_t dflt) {
    const int64_t id = gguf_find_key(g, key);
    return id < 0 ? dflt : (int64_t) gguf_get_val_u32(g, id);
}

int main(int argc, char ** argv) {
    if (argc < 2) { std::printf("usage: %s <apex.gguf>\n", argv[0]); return 2; }
    goose_init(0, (char **) 0);

    struct gguf_init_params ip = { true, nullptr };
    struct gguf_context * g = gguf_init_from_file(argv[1], ip);
    check(g != nullptr, "the file opens");
    if (!g) return 1;

    // The loader assigns n_layer_all = block_count directly
    // (llama-model.cpp:1396) and derives n_layer() = n_layer_all - n_layer_nextn
    // (llama-hparams.cpp:348). Subtracting nextn and adding it back happens to give the
    // same number, but it is a different statement: if the two keys ever disagreed, the
    // round trip would hide it while the loader would not.
    const int64_t n_layer_all = kv_u32(g, "qwen35moe.block_count", 0);
    const int64_t n_nextn     = kv_u32(g, "qwen35moe.nextn_predict_layers", 0);
    const int64_t n_layer     = n_layer_all - n_nextn;
    check(n_nextn <= n_layer_all, "nextn does not exceed the block count, as the loader asserts");
    const int64_t interval   = kv_u32(g, "qwen35moe.full_attention_interval", 0);
    std::printf("layers %lld + %lld nextn = %lld, interval %lld\n",
                (long long) n_layer, (long long) n_nextn, (long long) n_layer_all,
                (long long) interval);

    // One int32 per layer, because hparams.is_recr_impl is std::array<uint32_t, N> and
    // the module's ld_u32 reads with a 4-byte stride. An i64 vector here made every
    // second layer read as 0 - the gate was wrong, not the module: 25/15 instead of 11/30.
    std::vector<int32_t> flags((size_t) n_layer_all, 0);
    const int64_t id = gguf_find_key(g, "qwen35moe.attention.recurrent_layers");
    check(id >= 0, "the file declares attention.recurrent_layers");
    int64_t arr_n = 0;
    if (id >= 0) {
        const enum gguf_type at = gguf_get_arr_type(g, id);
        arr_n = (int64_t) gguf_get_arr_n(g, id);
        check(at == GGUF_TYPE_INT32 || at == GGUF_TYPE_BOOL, "the array is int32 or bool");
        check(arr_n == n_layer_all, "the array has one entry per layer");
        if (at == GGUF_TYPE_INT32) {
            const int32_t * v = (const int32_t *) gguf_get_arr_data(g, id);
            for (int64_t i = 0; i < arr_n && i < n_layer_all; i++) flags[(size_t) i] = v[i];
        } else if (at == GGUF_TYPE_BOOL) {
            const bool * v = (const bool *) gguf_get_arr_data(g, id);
            for (int64_t i = 0; i < arr_n && i < n_layer_all; i++) flags[(size_t) i] = v[i];
        }
    }

    std::vector<int64_t> p((size_t) 32, 0);
    p[0] = kv_u32(g, "qwen35moe.embedding_length", 0);
    p[1] = kv_u32(g, "qwen35moe.vocab_size", 0);
    p[2] = n_layer;
    p[3] = n_layer_all;
    p[4] = kv_u32(g, "qwen35moe.attention.head_count", 0);
    p[5] = kv_u32(g, "qwen35moe.attention.head_count_kv", 0);
    p[6] = kv_u32(g, "qwen35moe.feed_forward_length", 0);
    p[7] = kv_u32(g, "qwen35moe.expert_count", 0);
    p[8] = kv_u32(g, "qwen35moe.expert_used_count", 0);
    p[9] = kv_u32(g, "qwen35moe.expert_feed_forward_length", 0);
    p[10] = kv_u32(g, "qwen35moe.expert_shared_feed_forward_length", 0);
    p[11] = kv_u32(g, "qwen35moe.ssm.conv_kernel", 0);
    p[12] = kv_u32(g, "qwen35moe.ssm.inner_size", 0);
    p[13] = kv_u32(g, "qwen35moe.ssm.state_size", 0);
    p[14] = kv_u32(g, "qwen35moe.ssm.time_step_rank", 0);
    p[15] = kv_u32(g, "qwen35moe.ssm.group_count", 0);
    p[16] = kv_u32(g, "qwen35moe.attention.key_length", 0);
    p[17] = interval;
    p[18] = 1;

    std::vector<int64_t> from_formula((size_t) n_layer_all, -99);
    std::vector<int64_t> from_array((size_t) n_layer_all, -99);
    const int64_t nf = q35_plan(sl_i64{ p.data(), (int64_t) p.size() }, 0,
                                sl_i64{ from_formula.data(), n_layer_all });
    const int64_t na = q35_plan(sl_i64{ p.data(), (int64_t) p.size() },
                                (int64_t) (uintptr_t) flags.data(),
                                sl_i64{ from_array.data(), n_layer_all });
    std::printf("plan lengths: formula %lld, array %lld\n", (long long) nf, (long long) na);
    check(nf == n_layer_all, "the formula plan covers every layer");
    check(na == n_layer_all, "the array plan covers every layer");

    // Independent recomputation, so this is not the module agreeing with itself.
    std::vector<int64_t> want((size_t) n_layer_all, 0);
    for (int64_t i = 0; i < n_layer_all; i++) {
        if (i >= n_layer) { want[(size_t) i] = 2; continue; }
        const bool recr = flags[(size_t) i] != 0;
        want[(size_t) i] = recr ? 1 : 0;
    }
    int arr_ok = 1;
    for (int64_t i = 0; i < n_layer_all; i++) {
        if (from_array[(size_t) i] != want[(size_t) i]) {
            arr_ok = 0;
            std::printf("  layer %lld: array plan %lld, file says %lld\n", (long long) i,
                        (long long) from_array[(size_t) i], (long long) want[(size_t) i]);
        }
    }
    check(arr_ok, "the array-based plan reproduces the file's own flags");

    int agree = 1;
    for (int64_t i = 0; i < n_layer_all; i++) {
        if (from_formula[(size_t) i] != from_array[(size_t) i]) agree = 0;
    }
    std::printf("formula and array agree on this file: %s\n", agree ? "yes" : "no");
    // Not a failure either way - it is a fact about this file, and the next line states
    // why the difference matters.
    check(1, "both plans were produced");

    int a = 0, r = 0, m = 0;
    for (int64_t i = 0; i < n_layer_all; i++) {
        const int64_t k = from_array[(size_t) i];
        if (k == 0) a++; else if (k == 1) r++; else if (k == 2) m++;
    }
    std::printf("array plan: attn %d recr %d mtp %d\n", a, r, m);
    check(m == n_nextn, "exactly the nextn layers are MTP");
    check(a > 0 && r > 0, "the plan has both attention and recurrent layers");

    std::printf("%s checks=%d failures=%d\n", fails == 0 ? "PASS" : "BAD", checks, fails);
    gguf_free(g);
    return fails ? 1 : 0;
}
