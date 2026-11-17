// EXP922: text generation driven entirely by the Goose unit.
//
// prompt text -> Goose tokenizer (bt_*/pt_*/stp_*) -> Goose trunk (q35_*) with
// the generic GGML CPU executor -> Goose sampler (sm_*) -> Goose detokenizer
// (bt_token_to_piece) -> stop on the end-of-generation set.
//
// No llama.h here: the native tokenizer, sampler and detokenizer are not part
// of this path. The mapping is borrowed, as in EXP919.
#include "runtime.h"

#include <ggml.h>
#include <ggml-cpu.h>
#include <gguf.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

[[noreturn]] static void die(const char * what) {
    std::fprintf(stderr, "FAIL %s\n", what);
    std::exit(1);
}
static void require(bool ok, const char * what) {
    if (!ok) die(what);
}
static int64_t handle(const void * p) { return (int64_t) (uintptr_t) p; }
// Diagnostic switch for EXP923: F32 KV instead of the profile's Q4_0, to
// separate the KV write/read path from the rest of the trunk graph.
static bool f32_kv() { return std::getenv("GOOSE_EXP923_F32_KV") != nullptr; }
static sl_u8 U(const void * p, long long n) {
    return sl_u8{const_cast<uint8_t *>(static_cast<const uint8_t *>(p)), (int64_t) n};
}
static int64_t bits(float f) {
    int32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}
static int64_t integer(gguf_context * m, const char * key) {
    const int64_t k = gguf_find_key(m, key);
    require(k >= 0, key);
    const gguf_type t = gguf_get_kv_type(m, k);
    if (t == GGUF_TYPE_UINT32) return (int64_t) gguf_get_val_u32(m, k);
    if (t == GGUF_TYPE_INT32) return (int64_t) gguf_get_val_i32(m, k);
    if (t == GGUF_TYPE_UINT64) return (int64_t) gguf_get_val_u64(m, k);
    if (t == GGUF_TYPE_INT64) return gguf_get_val_i64(m, k);
    die("integer key type");
}
static float real(gguf_context * m, const char * key) {
    const int64_t k = gguf_find_key(m, key);
    require(k >= 0, key);
    return gguf_get_val_f32(m, k);
}

struct Mapping {
    int fd;
    uint8_t * base;
    size_t size;
    gguf_context * metadata;
    ggml_context * meta_tensors;
    ggml_context * weights;
    int bindings = 0;

    explicit Mapping(const char * path) {
        struct stat st;
        fd = open(path, O_RDONLY);
        require(fd >= 0 && fstat(fd, &st) == 0, "model FD");
        size = st.st_size;
        base = (uint8_t *) mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        require(base != MAP_FAILED, "read-only mapping");
        meta_tensors = nullptr;
        metadata = gguf_init_from_file(path, {true, &meta_tensors});
        require(metadata && meta_tensors, "GGUF metadata");
        weights = ggml_init({8 * 1024 * 1024, nullptr, true});
        require(weights && ggml_get_no_alloc(weights), "metadata-only weight context");
    }
    ~Mapping() {
        ggml_free(weights);
        gguf_free(metadata);
        ggml_free(meta_tensors);
        munmap(base, size);
        close(fd);
    }
    ggml_tensor * weight(const std::string & name) {
        const int64_t id = gguf_find_tensor(metadata, name.c_str());
        require(id >= 0, name.c_str());
        const int64_t * ne = gguf_get_tensor_ne(metadata, id);
        const ggml_type ty = gguf_get_tensor_type(metadata, id);
        const uint64_t offset = gguf_get_data_offset(metadata) + gguf_get_tensor_offset(metadata, id);
        const size_t n = gguf_get_tensor_size(metadata, id);
        require(offset <= size && n <= size - offset, "native descriptor bounds");
        int64_t shape[4];
        std::memcpy(shape, ne, sizeof shape);
        auto * t = (ggml_tensor *) (uintptr_t) q35_bind_weight(handle(weights), (uint64_t) (uintptr_t) base, size,
                                                                offset, ty, {shape, 4});
        require(t && t->data == base + offset && ggml_nbytes(t) == n, "zero-copy tensor binding");
        ++bindings;
        return t;
    }
    void moe(int layer, ggml_tensor ** w) {
        const char * names[8] = {"ffn_gate_inp.weight", "ffn_up_exps.weight", "ffn_gate_exps.weight",
                                 "ffn_down_exps.weight", "ffn_up_shexp.weight", "ffn_gate_shexp.weight",
                                 "ffn_down_shexp.weight", "ffn_gate_inp_shexp.weight"};
        for (int i = 0; i < 8; ++i) w[i] = weight("blk." + std::to_string(layer) + "." + names[i]);
    }
};

struct Trunk {
    static constexpr int capacity = 2048;
    Mapping & model;
    ggml_context * owner;
    int64_t layer_count, hidden, acfg[14], gcfg[7], mcfg[3], state[6] = {0}, vocab;
    int sections[4];
    int native_past = 0;
    std::vector<int64_t> records;
    std::vector<std::array<ggml_tensor *, 11>> branch;
    std::vector<std::array<ggml_tensor *, 8>> moes;
    std::vector<std::array<ggml_tensor *, 4>> stores;
    ggml_tensor * emb, * head, * norm;

    explicit Trunk(Mapping & mapping) : model(mapping) {
        auto * m = model.metadata;
        layer_count = integer(m, "qwen35moe.block_count") - integer(m, "qwen35moe.nextn_predict_layers");
        // Diagnostic models (EXP923) carry fewer blocks; the Apex file has 40.
        require(layer_count >= 1 && layer_count <= 41, "trunk layer count");
        hidden = integer(m, "qwen35moe.embedding_length");
        vocab = gguf_find_key(m, "qwen35moe.vocab_size") >= 0 ? integer(m, "qwen35moe.vocab_size") : 248320;
        const auto h = integer(m, "qwen35moe.ssm.state_size");
        const auto nk = integer(m, "qwen35moe.ssm.group_count");
        const auto nv = integer(m, "qwen35moe.ssm.time_step_rank");
        const auto kernel = integer(m, "qwen35moe.ssm.conv_kernel");
        const auto heads = integer(m, "qwen35moe.attention.head_count");
        const auto hk = integer(m, "qwen35moe.attention.head_count_kv");
        const auto hd = integer(m, "qwen35moe.attention.key_length");
        const float eps = real(m, "qwen35moe.attention.layer_norm_rms_epsilon");
        require(q35_gdn_config(h, nk, nv, kernel, bits(eps), {gcfg, 7}) == 7, "trunk GDN config");
        int64_t a[14] = {heads, hk, integer(m, "qwen35moe.rope.dimension_count"), GGML_ROPE_TYPE_IMROPE,
                         integer(m, "qwen35moe.context_length"), bits(eps), bits(real(m, "qwen35moe.rope.freq_base")),
                         bits(1.0f), bits(0.0f), bits(1.0f), bits(32.0f), bits(1.0f), bits(1.0f / std::sqrt((float) hd)),
                         GGML_PREC_F32};
        std::memcpy(acfg, a, sizeof a);
        mcfg[0] = integer(m, "qwen35moe.expert_count");
        mcfg[1] = integer(m, "qwen35moe.expert_used_count");
        mcfg[2] = bits(0.0f);
        require(mcfg[0] == 256 && mcfg[1] == 8, "actual Apex routing");
        auto key = gguf_find_key(m, "qwen35moe.rope.dimension_sections");
        require(key >= 0 && gguf_get_arr_n(m, key) == 4, "rope sections");
        std::memcpy(sections, gguf_get_arr_data(m, key), sizeof sections);
        key = gguf_find_key(m, "qwen35moe.attention.recurrent_layers");
        require(key >= 0 && gguf_get_arr_type(m, key) == GGUF_TYPE_BOOL && gguf_get_arr_n(m, key) >= layer_count, "layer flags");
        auto * flags = (const uint8_t *) gguf_get_arr_data(m, key);
        records.resize(layer_count * 32);
        branch.resize(layer_count);
        moes.resize(layer_count);
        stores.resize(layer_count);
        owner = ggml_init({192 * 1024 * 1024, nullptr, false});
        require(owner, "persistent model-state arena");
        const char * recurrent[11] = {"attn_norm.weight", "attn_qkv.weight", "attn_gate.weight", "ssm_beta.weight",
                                      "ssm_alpha.weight", "ssm_dt.bias", "ssm_a", "ssm_conv1d.weight",
                                      "ssm_norm.weight", "ssm_out.weight", "post_attention_norm.weight"};
        const char * attention[8] = {"attn_norm.weight", "attn_q.weight", "attn_k.weight", "attn_v.weight",
                                     "attn_output.weight", "attn_q_norm.weight", "attn_k_norm.weight",
                                     "post_attention_norm.weight"};
        for (int i = 0; i < layer_count; ++i) {
            auto * row = records.data() + 32 * i;
            row[0] = flags[i] ? 1 : 0;
            for (int j = 0; j < (flags[i] ? 11 : 8); ++j) {
                const std::string name = "blk." + std::to_string(i) + "." + (flags[i] ? recurrent[j] : attention[j]);
                row[1 + j] = handle(model.weight(name));
                branch[i][j] = model.weight(name);
            }
            ggml_tensor * mw[8];
            model.moe(i, mw);
            for (int j = 0; j < 8; ++j) row[12 + j] = handle(mw[j]);
            auto & s = stores[i];
            s.fill(nullptr);
            if (flags[i]) {
                s[0] = ggml_new_tensor_2d(owner, GGML_TYPE_F32, kernel - 1, h * (2 * nk + nv));
                s[1] = ggml_new_tensor_4d(owner, GGML_TYPE_F32, h, h, nv, 1);
            } else {
                const ggml_type kvt = f32_kv() ? GGML_TYPE_F32 : GGML_TYPE_Q4_0;
                s[2] = ggml_new_tensor_2d(owner, kvt, hd * hk, capacity);
                s[3] = ggml_new_tensor_2d(owner, kvt, hd * hk, capacity);
            }
            for (auto * t : s)
                if (t) std::memset(t->data, 0, ggml_nbytes(t));
            for (int j = 0; j < 4; ++j) row[20 + j] = handle(s[j]);
        }
        emb = model.weight("token_embd.weight");
        const bool tied = gguf_find_tensor(m, "output.weight") < 0;
        head = tied ? emb : model.weight("output.weight");
        norm = model.weight("output_norm.weight");
        require(q35_kv_init(capacity, 32, {state, 6}) == 0, "model-step init");
        int64_t n_recr = 0;
        for (int i = 0; i < layer_count; ++i) n_recr += records[32 * i] ? 1 : 0;
        std::printf("MODEL layers=%lld GDN=%lld attention=%lld zero_copy_bindings=%d persistent_bytes=%zu\n",
                    (long long) layer_count, (long long) n_recr, (long long) (layer_count - n_recr),
                    model.bindings, ggml_used_mem(owner));
    }
    ~Trunk() { ggml_free(owner); }

    // One full-model step. Returns the logits tensor data, [vocab, count].
    std::vector<float> step(const std::vector<int32_t> & tokens) {
        require(state[2] == native_past && state[4] == 0, "whole-model prefix");
        const int count = (int) tokens.size();
        auto * c = ggml_init({1024 * 1024 * 1024, nullptr, false});
        require(c, "full trunk graph arena");
        const int64_t rows = q35_kv_read_rows({state, 6}, count);
        require(rows > 0, "KV read rows");
        auto * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, count);
        std::memcpy(ids->data, tokens.data(), count * 4);
        auto * positions = ggml_new_tensor_1d(c, GGML_TYPE_I32, count * 4);
        std::vector<int64_t> pos(count * 4);
        require(q35_kv_positions({state, 6}, count, {pos.data(), (int64_t) pos.size()}) == count * 4, "positions");
        for (int s = 0; s < 4; ++s)
            for (int t = 0; t < count; ++t) ((int32_t *) positions->data)[s * count + t] = (int32_t) pos[s * count + t];
        auto * indices = ggml_new_tensor_1d(c, GGML_TYPE_I64, count);
        require(q35_kv_indices({state, 6}, count, {(int64_t *) indices->data, count}) == count, "indices");
        auto * mask = ggml_new_tensor_2d(c, GGML_TYPE_F16, rows, count);
        std::vector<float> raw_mask(rows * count);
        require(q35_kv_mask({state, 6}, count, {raw_mask.data(), (int64_t) raw_mask.size()}) == rows, "mask");
        for (int t = 0; t < count; ++t)
            for (int k = 0; k < rows; ++k)
                ((ggml_fp16_t *) mask->data)[t * rows + k] = ggml_fp32_to_fp16(raw_mask[t * rows + k]);
        const auto ticket = q35_kv_begin({state, 6}, count);
        require(ticket > 0, "begin entire model step");
        int64_t work[16] = {0}, bn[40] = {0}, mn[42] = {0};
        std::vector<int64_t> frames(4 * layer_count + 4), roots(2 * layer_count + 2);
        const auto result = q35_trunk(handle(c), handle(ids), handle(emb), handle(head), handle(norm),
                                      {records.data(), (int64_t) records.size()}, layer_count, handle(positions),
                                      handle(mask), handle(indices), {acfg, 14}, {gcfg, 7}, {mcfg, 3}, handle(sections),
                                      {state, 6}, ticket, {work, 16}, {bn, 40}, {mn, 42},
                                      {frames.data(), (int64_t) frames.size()}, {roots.data(), (int64_t) roots.size()});
        require(result && result == frames.back() && state[4] == count && state[5] == ticket, "Goose full trunk build");
        auto * graph = ggml_new_graph_custom(c, 16384, false);
        for (int64_t i = 1; i <= roots[0]; ++i) ggml_build_forward_expand(graph, (ggml_tensor *) (uintptr_t) roots[i]);
        require(ggml_graph_compute_with_ctx(c, graph, 4) == GGML_STATUS_SUCCESS, "full trunk CPU compute");
        auto * logits = (ggml_tensor *) (uintptr_t) result;
        std::vector<float> answer((float *) logits->data, (float *) logits->data + ggml_nelements(logits));
        require(q35_kv_commit({state, 6}, ticket) == native_past + count, "single full-model commit");
        native_past += count;
        ggml_free(c);
        return answer;
    }
};

// ---- Goose vocabulary ----
struct Vocab {
    const uint8_t * base;
    int64_t bytes;
    std::vector<int64_t> state, kv, tens, kv_slots, ten_slots;
    std::vector<int64_t> tok_off, tok_len, mrg_off, mrg_len, tok_slots, mrg_slots, attr, spl;
    std::vector<int64_t> stats;
    int64_t n_tok = 0, n_spl = 0, n_eog = 0, eos = -1;
    std::vector<int64_t> eog_ids;

    explicit Vocab(Mapping & m) {
        base = m.base;
        bytes = (int64_t) m.size;
        state.assign(8, 0);
        kv.assign(64 * 8, 0);
        tens.assign(1024 * 12, 0);
        kv_slots.assign(256, 0);
        ten_slots.assign(4096, 0);
        const int64_t hdr = bytes < 67108864 ? bytes : 67108864;
        require(gr_parse(U(base, hdr), bytes, {state.data(), 8}, {kv.data(), (int64_t) kv.size()},
                         {tens.data(), (int64_t) tens.size()}, {kv_slots.data(), (int64_t) kv_slots.size()},
                         {ten_slots.data(), (int64_t) ten_slots.size()}) == 0, "Goose reader");
        auto find = [&](const char * key) {
            return gr_find_kv(U(base, hdr), {state.data(), 8}, {kv.data(), (int64_t) kv.size()},
                              {kv_slots.data(), (int64_t) kv_slots.size()},
                              U(key, std::strlen(key)));
        };
        const int64_t ti = find("tokenizer.ggml.tokens");
        const int64_t mi = find("tokenizer.ggml.merges");
        const int64_t tyi = find("tokenizer.ggml.token_type");
        const int64_t ei = find("tokenizer.ggml.eos_token_id");
        require(ti >= 0 && mi >= 0 && tyi >= 0 && ei >= 0, "tokenizer keys");
        n_tok = kv[ti * 8 + 5];
        const int64_t n_mrg = kv[mi * 8 + 5];
        tok_off.assign(n_tok, 0);
        tok_len.assign(n_tok, 0);
        mrg_off.assign(n_mrg, 0);
        mrg_len.assign(n_mrg, 0);
        std::vector<int64_t> pairs(2 * std::max(n_tok, n_mrg));
        require(gr_string_array_index(U(base, hdr), {state.data(), 8}, {kv.data(), (int64_t) kv.size()}, ti,
                                      {pairs.data(), (int64_t) pairs.size()}) == n_tok, "token spans");
        for (int64_t i = 0; i < n_tok; ++i) { tok_off[i] = pairs[2 * i]; tok_len[i] = pairs[2 * i + 1]; }
        require(gr_string_array_index(U(base, hdr), {state.data(), 8}, {kv.data(), (int64_t) kv.size()}, mi,
                                      {pairs.data(), (int64_t) pairs.size()}) == n_mrg, "merge spans");
        for (int64_t i = 0; i < n_mrg; ++i) { mrg_off[i] = pairs[2 * i]; mrg_len[i] = pairs[2 * i + 1]; }
        int64_t one = 0;
        require(gr_meta_int(U(base, hdr), {state.data(), 8}, {kv.data(), (int64_t) kv.size()}, ei, {&one, 1}) == 0,
                "eos id");
        eos = one;
        tok_slots.assign(1 << 20, 0);
        mrg_slots.assign(1 << 20, 0);
        stats.assign(4, 0);
        require(bt_build(U(base, hdr), {tok_off.data(), n_tok}, {tok_len.data(), n_tok}, {tok_slots.data(), (int64_t) tok_slots.size()},
                         {mrg_off.data(), n_mrg}, {mrg_len.data(), n_mrg}, {mrg_slots.data(), (int64_t) mrg_slots.size()},
                         {stats.data(), 4}) == 0, "token/merge tables");
        std::vector<int64_t> type(n_tok, 0), tmp(1);
        for (int64_t i = 0; i < n_tok; ++i) {
            require(gr_array_int(U(base, hdr), {state.data(), 8}, {kv.data(), (int64_t) kv.size()}, tyi, i,
                                 {tmp.data(), 1}) == 0, "token_type");
            type[i] = tmp[0];
        }
        attr.assign(n_tok, 0);
        require(bt_attrs(U(base, hdr), {tok_off.data(), n_tok}, {tok_len.data(), n_tok}, {type.data(), n_tok},
                         {attr.data(), n_tok}) == 0, "attributes");
        spl.assign(n_tok, 0);
        n_spl = bt_special_order(U(base, hdr), {tok_off.data(), n_tok}, {tok_len.data(), n_tok},
                                 {attr.data(), n_tok}, {spl.data(), n_tok});
        require(n_spl > 0, "special order");
        eog_ids.assign(n_tok, 0);
        n_eog = bt_eog_ids(U(base, bytes), {tok_off.data(), n_tok}, {tok_len.data(), n_tok}, eos, -1, -1,
                           {eog_ids.data(), (int64_t) eog_ids.size()});
        require(n_eog > 0, "eog ids");
        std::printf("VOCAB tokens=%lld merges=%lld specials=%lld eos=%lld eog=%lld\n", (long long) n_tok,
                    (long long) n_mrg, (long long) n_spl, (long long) eos, (long long) n_eog);
    }

    std::vector<int64_t> tokenize(const std::string & text) {
        const int64_t len = (int64_t) text.size();
        std::vector<int64_t> kind(len + 4), tok(len + 4), off(len + 4), ln(len + 4);
        std::vector<int64_t> ids(4 * len + 64), ids_out;
        std::vector<int64_t> cpts(len + 1), seg(4 * (len + 1));
        std::vector<uint8_t> enc(8 * (len + 1));
        std::vector<int64_t> sym(24 * (len + 1)), heap(15 * (len + 1) + 40), hc(2, 0), info(8, 0);
        const int64_t nfrag = stp_partition(U(text.data(), len), n_tok,
                                            {tok_off.data(), n_tok}, {tok_len.data(), n_tok}, U(base, bytes),
                                            {attr.data(), n_tok}, {spl.data(), n_spl}, 1,
                                            {kind.data(), (int64_t) kind.size()}, {tok.data(), (int64_t) tok.size()},
                                            {off.data(), (int64_t) off.size()}, {ln.data(), (int64_t) ln.size()});
        require(nfrag >= 0, "special partition");
        int64_t nout = 0;
        for (int64_t f = 0; f < nfrag; ++f) {
            if (kind[f] == 0) {
                ids[nout++] = tok[f];
            } else {
                const int64_t added = bt_bpe(U(text.data() + off[f], ln[f]), U(base, bytes),
                                             {tok_off.data(), n_tok}, {tok_len.data(), n_tok},
                                             {tok_slots.data(), (int64_t) tok_slots.size()},
                                             {mrg_off.data(), (int64_t) mrg_off.size()}, {mrg_len.data(), (int64_t) mrg_len.size()},
                                             {mrg_slots.data(), (int64_t) mrg_slots.size()},
                                             {ids.data(), (int64_t) ids.size()}, nout,
                                             {cpts.data(), (int64_t) cpts.size()}, {seg.data(), (int64_t) seg.size()},
                                             {enc.data(), (int64_t) enc.size()}, {sym.data(), (int64_t) sym.size()},
                                             {heap.data(), (int64_t) heap.size()}, {hc.data(), 2}, {info.data(), 8});
                require(added >= 0, "BPE fragment");
                nout += added;
            }
        }
        ids.resize(nout);
        return ids;
    }

    std::string piece(int64_t id) {
        std::vector<uint8_t> buf(4096);
        const int64_t n = bt_token_to_piece(U(base, bytes), {tok_off.data(), n_tok}, {tok_len.data(), n_tok},
                                            {attr.data(), n_tok}, id, U(buf.data(), buf.size()),
                                            0, 0, 1, 0);
        require(n >= 0, "token_to_piece");
        return std::string((const char *) buf.data(), (size_t) n);
    }

    bool is_eog(int64_t id) {
        return bt_is_eog({eog_ids.data(), n_eog}, n_eog, id) != 0;
    }
};

int main(int argc, char ** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: gate <model.gguf> <prompt> [max_tokens] [seed] [greedy|chain]\n");
        return 2;
    }
    setvbuf(stdout, nullptr, _IONBF, 0);
    goose_init(0, nullptr);
    const int max_tokens = argc > 3 ? std::atoi(argv[3]) : 8;
    const int64_t seed = argc > 4 ? std::atoll(argv[4]) : 42;
    const bool greedy = argc <= 5 || std::string(argv[5]) == "greedy";
    const bool dump_logits = argc > 5 && std::string(argv[5]) == "logits";

    Mapping model(argv[1]);
    Vocab vocab(model);
    Trunk trunk(model);

    std::vector<int64_t> prompt = vocab.tokenize(argv[2]);
    std::printf("PROMPT ids=%zu:", prompt.size());
    for (auto id : prompt) std::printf(" %lld", (long long) id);
    std::printf("\n");

    std::vector<int32_t> batch;
    for (auto id : prompt) batch.push_back((int32_t) id);
    auto logits = trunk.step(batch);
    const int64_t V = trunk.vocab;
    require((int64_t) logits.size() == V * (int64_t) prompt.size(), "logits shape");

    if (std::string(argv[5]) == "mcheck") {
        // Rebuild the attention branch by hand from the same tensors and compare
        // the resulting logits with the Goose path (in memory) and native (file).
        require(argc > 7, "mcheck needs <layer> <native logits file>");
        const int L = std::atoi(argv[6]);
        const std::string native_path = argv[7];
        auto * c = ggml_init({256 * 1024 * 1024, nullptr, false});
        require(c, "mcheck arena");
        const float eps = real(model.metadata, "qwen35moe.attention.layer_norm_rms_epsilon");
        auto * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, 1);
        ((int32_t *) ids->data)[0] = (int32_t) prompt[0];
        auto * emb = ggml_get_rows(c, trunk.emb, ids);
        auto * xn = ggml_mul(c, ggml_rms_norm(c, emb, eps), model.weight("blk." + std::to_string(L) + ".attn_norm.weight"));
        auto * qp = ggml_mul_mat(c, model.weight("blk." + std::to_string(L) + ".attn_q.weight"), xn);
        const int64_t hd = 256, nh = trunk.vocab > 0 ? 16 : 16, hk = 2;
        // gate: second hd of every 2*hd block, as native's view_3d does
        auto * gate_v = ggml_view_3d(c, qp, hd, nh, 1, (size_t) hd * 2 * 4, (size_t) hd * 2 * nh * 4, (size_t) hd * 4);
        auto * gate2 = ggml_cont_2d(c, gate_v, hd * nh, 1);
        auto * gsig = ggml_sigmoid(c, gate2);
        // V: dequantise cell 0 of the store into an F32 tensor
        auto * store = trunk.stores[L][3];
        auto * vt = ggml_new_tensor_1d(c, GGML_TYPE_F32, hd * hk);
        ggml_get_type_traits(GGML_TYPE_Q4_0)->to_float((const uint8_t *) store->data, (float *) vt->data, hd * hk);
        // GQA: query head h uses KV head h/(n_head/n_head_kv); n_head/n_head_kv = 8 here
        if (argc > 9 && std::string(argv[9]) == "f16v") {
            // emulate a kernel that dequantises V through F16
            vt = ggml_cast(c, ggml_cast(c, vt, GGML_TYPE_F16), GGML_TYPE_F32);
        }
        auto * v3 = ggml_reshape_4d(c, vt, hd, 1, hk, 1);
        auto * vr = ggml_repeat_4d(c, v3, hd, nh / hk, hk, 1);
        auto * v2 = ggml_cont_2d(c, vr, hd * nh, 1);
        auto * gv2 = ggml_reshape_2d(c, gsig, hd, nh);
        auto * gv3 = ggml_cont_2d(c, gv2, hd * nh, 1);
        auto * gated = ggml_mul(c, v2, gv3);
        auto * wo = ggml_mul_mat(c, model.weight("blk." + std::to_string(L) + ".attn_output.weight"), gated);
        auto * res = ggml_add(c, wo, emb);
        auto * pn = ggml_mul(c, ggml_rms_norm(c, res, eps), model.weight("blk." + std::to_string(L) + ".post_attention_norm.weight"));
        // the MoE of this block is zeroed in the diagnostic model, so the layer
        // output is the residual itself
        auto * fin = ggml_mul(c, ggml_rms_norm(c, res, eps), trunk.norm);
        auto * lg = ggml_mul_mat(c, trunk.head, fin);
        ggml_set_output(lg);
        struct ggml_cgraph * g = ggml_new_graph(c);
        ggml_build_forward_expand(g, lg);
        require(ggml_graph_compute_with_ctx(c, g, 4) == GGML_STATUS_SUCCESS, "mcheck compute");
        const float * mine = (const float *) lg->data;
        auto diff = [&](const float * other) {
            double mx = 0; int64_t bad = 0;
            for (int64_t i = 0; i < V; ++i) {
                double d = std::fabs((double) mine[i] - other[i]);
                if (d > mx) mx = d;
                if (d != 0.0) bad++;
            }
            return std::pair<double, int64_t>(mx, bad);
        };
        auto vs_goose = diff(logits.data());
        std::vector<float> nat((size_t) V);
        FILE * f = std::fopen(native_path.c_str(), "rb");
        require(f && std::fread(nat.data(), sizeof(float), (size_t) V, f) == (size_t) V, "native logits read");
        std::fclose(f);
        auto vs_native = diff(nat.data());
        if (argc > 8) {
            FILE * e = std::fopen(argv[8], "wb");
            require(e != nullptr, "emb file");
            const int64_t ne = ggml_nelements(fin);
            require(std::fwrite(fin->data, sizeof(float), (size_t) ne, e) == (size_t) ne, "emb write");
            std::fclose(e);
            std::printf("MCHECK_EMB_DIM %lld MCHECK_EMB %s\n", (long long) ne, argv[8]);
        }
        std::printf("MCHECK layer=%d manual_vs_goose max=%.9g bad=%lld manual_vs_native max=%.9g bad=%lld\n",
                    L, vs_goose.first, (long long) vs_goose.second, vs_native.first, (long long) vs_native.second);
        return 0;
    }

    if (std::string(argv[5]) == "vcheck") {
        // On the attention-only diagnostic model the layer-1 input is the token
        // embedding (block 0 is zeroed), so V can be recomputed here and its
        // canonical Q4_0 bytes compared with what the graph stored.
        require(argc > 6, "vcheck mode needs a layer index");
        const int layer = std::atoi(argv[6]);
        std::vector<int32_t> one = {(int32_t) prompt[0]};
        auto * c = ggml_init({64 * 1024 * 1024, nullptr, false});
        require(c, "vcheck arena");
        auto * ids = ggml_new_tensor_1d(c, GGML_TYPE_I32, 1);
        ((int32_t *) ids->data)[0] = one[0];
        auto * emb = ggml_get_rows(c, trunk.emb, ids);
        const float eps_norm = real(model.metadata, "qwen35moe.attention.layer_norm_rms_epsilon");
        auto * nrm = ggml_rms_norm(c, emb, eps_norm);
        auto * wn = model.weight("blk." + std::to_string(layer) + ".attn_norm.weight");
        auto * xn = ggml_mul(c, nrm, wn);
        auto * wv = model.weight("blk." + std::to_string(layer) + ".attn_v.weight");
        auto * v = ggml_mul_mat(c, wv, xn);
        auto * g = ggml_new_graph(c);
        ggml_build_forward_expand(g, v);
        require(ggml_graph_compute_with_ctx(c, g, 1) == GGML_STATUS_SUCCESS, "vcheck compute");
        const int64_t n = ggml_nelements(v);
        std::vector<uint8_t> q((size_t) ggml_row_size(GGML_TYPE_Q4_0, n));
        require(ggml_quantize_chunk(GGML_TYPE_Q4_0, (const float *) v->data, q.data(), 0, 1, n, nullptr) > 0, "quantize");
        auto * store = trunk.stores[layer][3];
        require(store && store->type == GGML_TYPE_Q4_0, "V store");
        const uint8_t * raw = (const uint8_t *) store->data;
        size_t same = 0;
        while (same < q.size() && raw[same] == q[same]) same++;
        std::printf("VCHECK layer=%d values=%lld qbytes=%zu store_bytes=%zu prefix_equal=%zu\n",
                    layer, (long long) n, q.size(), ggml_nbytes(store), same);
        std::printf("VCHECK canonical:"); for (int i = 0; i < 16 && i < (int) q.size(); i++) std::printf(" %02x", q[i]);
        std::printf("\nVCHECK stored   :"); for (int i = 0; i < 16 && i < (int) ggml_nbytes(store); i++) std::printf(" %02x", raw[i]);
        std::printf("\n");
        const float * f = (const float *) v->data;
        std::printf("VCHECK first values:"); for (int i = 0; i < 8; i++) std::printf(" %.6f", f[i]);
        std::printf("\n");
        return 0;
    }

    if (std::string(argv[5]) == "kvbytes") {
        require(argc > 6, "kvbytes mode needs an output path");
        // the store tensors of every layer, K then V, as written by the graph
        FILE * f = std::fopen(argv[6], "wb");
        require(f != nullptr, "kvbytes file");
        for (int i = 0; i < trunk.layer_count; ++i) {
            for (int s = 0; s < 4; ++s) {
                auto * t = trunk.stores[i][s];
                if (!t) continue;
                std::printf("STORE layer=%d slot=%d type=%d bytes=%zu\n", i, s, (int) t->type, ggml_nbytes(t));
                require(std::fwrite(t->data, 1, ggml_nbytes(t), f) == ggml_nbytes(t), "store write");
            }
        }
        std::fclose(f);
        std::printf("KVBYTES_DUMP %s\n", argv[6]);
        return 0;
    }

    if (dump_logits) {
        // last-position logits only, raw f32, for an independent comparison
        require(argc > 6, "logits mode needs an output path");
        const float * last = logits.data() + (int64_t) (prompt.size() - 1) * V;
        FILE * f = std::fopen(argv[6], "wb");
        require(f != nullptr, "logits file");
        require(std::fwrite(last, sizeof(float), (size_t) V, f) == (size_t) V, "logits write");
        std::fclose(f);
        int64_t best = 0;
        for (int64_t i = 1; i < V; ++i)
            if (last[i] > last[best]) best = i;
        std::printf("LOGITS_VOCAB %lld ARGMAX %lld ARGMAX_VALUE %.9g\n", (long long) V, (long long) best, last[best]);
        std::printf("LOGITS_DUMP %s\n", argv[6]);
        return 0;
    }

    std::vector<float> lv(V), pbuf(V);
    std::vector<int64_t> ids(V), out(3), ip(3), state(625);
    std::vector<float> fp = {0.95f, 0.0f, 1.0f};
    require(sm_mt_seed({state.data(), 625}, seed) == 0, "seed");

    std::string text;
    std::vector<int64_t> generated;
    for (int step = 0; step < max_tokens; ++step) {
        const float * last = logits.data() + (int64_t) (prompt.size() - 1) * V;
        std::copy(last, last + V, lv.begin());
        ip[0] = greedy ? 1 : 20;
        ip[1] = 1;
        ip[2] = greedy ? 0 : 1;
        const int64_t rc = sm_select({lv.data(), V}, {ids.data(), V}, {pbuf.data(), V}, V,
                                     {ip.data(), 3}, {fp.data(), 3}, {state.data(), 625}, {out.data(), 3});
        require(rc == 0, "sampler");
        const int64_t id = out[1];
        const std::string piece = vocab.piece(id);
        generated.push_back(id);
        text += piece;
        std::printf("TOKEN %d id=%lld piece_bytes=%zu\n", step, (long long) id, piece.size());
        if (vocab.is_eog(id)) {
            std::printf("STOP eog id=%lld at step %d\n", (long long) id, step);
            break;
        }
        auto next = trunk.step({(int32_t) id});
        require((int64_t) next.size() == V, "single-token logits");
        logits = next;
        batch = {(int32_t) id};
        prompt.clear();
        prompt.push_back(id);
    }

    std::printf("GENERATED ids=%zu:", generated.size());
    for (auto id : generated) std::printf(" %lld", (long long) id);
    std::printf("\nTEXT_BEGIN\n%s\nTEXT_END\n", text.c_str());
    return 0;
}
