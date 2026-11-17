// Reuse the frozen independent GGUF reader and native projection oracle.
#define main exp913_unused_main
#include "../../EXP-2026-11-17-913-goose-real-attention/tests/gate.cpp"
#undef main

struct Data {
    std::vector<Weight> raw;
    std::vector<float> embeddings;
    int64_t cfg[14];
    int sections[4];
    int64_t hidden, width;
};

static Data load_data(const char * path) {
    int fd = open(path, O_RDONLY);
    require(fd >= 0, "model open");
    struct stat st; require(fstat(fd, &st) == 0 && st.st_size == 21713463136LL, "model size");
    ggml_context * tensors = nullptr;
    gguf_context * meta = gguf_init_from_file(path, {true, &tensors});
    require(meta != nullptr, "GGUF metadata");
    Data d;
    const char * names[8] = {"blk.3.attn_norm.weight", "blk.3.attn_q.weight", "blk.3.attn_k.weight", "blk.3.attn_v.weight",
                             "blk.3.attn_output.weight", "blk.3.attn_q_norm.weight", "blk.3.attn_k_norm.weight", "blk.3.post_attention_norm.weight"};
    for (const char * name : names) d.raw.push_back(load_weight(meta, fd, st.st_size, name));
    d.hidden = integer(meta, "qwen35moe.embedding_length");
    int64_t heads = integer(meta, "qwen35moe.attention.head_count"), kv_heads = integer(meta, "qwen35moe.attention.head_count_kv");
    int64_t hd = d.raw[1].ne[1] / (2 * heads);
    d.width = hd * kv_heads;
    int64_t cfg[14] = {heads, kv_heads, integer(meta, "qwen35moe.rope.dimension_count"), GGML_ROPE_TYPE_IMROPE,
                      integer(meta, "qwen35moe.context_length"), bits(real(meta, "qwen35moe.attention.layer_norm_rms_epsilon")),
                      bits(real(meta, "qwen35moe.rope.freq_base")), bits(1.0f), bits(0.0f), bits(1.0f), bits(32.0f), bits(1.0f),
                      bits(1.0f / std::sqrt((float) hd)), GGML_PREC_F32};
    std::memcpy(d.cfg, cfg, sizeof cfg);
    int64_t section = gguf_find_key(meta, "qwen35moe.rope.dimension_sections");
    require(section >= 0 && gguf_get_arr_n(meta, section) == 4, "sections");
    std::memcpy(d.sections, gguf_get_arr_data(meta, section), sizeof d.sections);
    int64_t id = gguf_find_tensor(meta, "token_embd.weight");
    require(id >= 0, "embedding");
    ggml_type type = gguf_get_tensor_type(meta, id);
    size_t row = ggml_row_size(type, d.hidden);
    uint64_t offset = gguf_get_data_offset(meta) + gguf_get_tensor_offset(meta, id);
    int ids[4] = {1, 42, 100, 123};
    d.embeddings.resize(d.hidden * 4);
    for (int i = 0; i < 4; ++i) {
        auto data = read_range(fd, st.st_size, offset + ids[i] * row, row);
        const auto * traits = ggml_get_type_traits(type);
        require(traits && traits->to_float, "embedding dequant");
        traits->to_float(data.data(), d.embeddings.data() + i * d.hidden, d.hidden);
    }
    close(fd); gguf_free(meta); ggml_free(tensors);
    return d;
}

static std::vector<uint8_t> bytes_of(ggml_tensor * t) {
    auto * p = (uint8_t *) t->data;
    return {p, p + ggml_nbytes(t)};
}

static bool graph_has(ggml_cgraph * graph, ggml_tensor * tensor) {
    for (int i = 0; i < ggml_graph_n_nodes(graph); ++i) if (ggml_graph_node(graph, i) == tensor) return true;
    return false;
}

static bool same_float_bytes(const std::vector<float> & a, const std::vector<float> & b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

static int planner_checks() {
    int checks = 0;
    for (auto shape : {std::pair<int64_t, int64_t>{8, 8}, {512, 32}, {1024, 512}}) {
        int64_t state[6];
        require(q35_kv_init(shape.first, shape.second, {state, 6}) == 0, "planner state init");
        for (int64_t valid : {INT64_C(0), INT64_C(1), INT64_C(255), INT64_C(256), INT64_C(257), shape.first - 1}) {
            if (valid >= shape.first) continue;
            state[2] = valid;
            int64_t pad = std::max<int64_t>(256, shape.second);
            int64_t expected = std::min(shape.first, std::max(pad, ((valid + 1 + pad - 1) / pad) * pad));
            require(q35_kv_read_rows({state, 6}, 1) == expected, "native padding boundary");
            int64_t output[3] = {-777, -777, -777};
            require(q35_kv_indices({state, 6}, 1, {output + 1, 1}) == 1 && output[1] == valid && output[0] == output[2], "index guards");
            int64_t before[6]; std::memcpy(before, state, sizeof state);
            require(q35_kv_begin({state, 6}, INT64_MAX) == -1 && std::memcmp(before, state, sizeof state) == 0, "overflow begin mutation");
            checks += 3;
        }
    }
    for (auto shape : {std::pair<int64_t, int64_t>{0, 1}, {512, 3}, {7, 2}, {INT64_MAX, 32}}) {
        int64_t state[6] = {-777, -777, -777, -777, -777, -777}, before[6]; std::memcpy(before, state, sizeof state);
        require(q35_kv_init(shape.first, shape.second, {state, 6}) == -1 && std::memcmp(before, state, sizeof state) == 0, "invalid init writes");
        ++checks;
    }
    return checks;
}

struct Session {
    Data & d;
    ggml_context * owner;
    ggml_tensor * gk, * gv, * nk, * nv;
    int64_t state[6] = {0};
    int native_past = 0;
    int completed = 0, aborted = 0, invalid_layouts = 0;
    static constexpr int capacity = 512;
    size_t row;

    explicit Session(Data & data) : d(data) {
        owner = ggml_init({2 * 1024 * 1024, nullptr, false});
        require(owner != nullptr, "cache owner");
        gk = ggml_new_tensor_2d(owner, GGML_TYPE_Q4_0, d.width, capacity);
        gv = ggml_new_tensor_2d(owner, GGML_TYPE_Q4_0, d.width, capacity);
        nk = ggml_new_tensor_2d(owner, GGML_TYPE_Q4_0, d.width, capacity);
        nv = ggml_new_tensor_2d(owner, GGML_TYPE_Q4_0, d.width, capacity);
        row = ggml_row_size(GGML_TYPE_Q4_0, d.width);
        for (auto * t : {gk, gv, nk, nv}) std::memset(t->data, 0, ggml_nbytes(t));
        require(q35_kv_init(capacity, 32, {state, 6}) == 0, "state init");
    }
    ~Session() { ggml_free(owner); }

    std::vector<float> step(int first_input, int tokens, bool abort_only = false) {
        require(state[2] == native_past && state[4] == 0, "prefix before step");
        const int old_past = native_past;
        int64_t native_rows = std::min<int64_t>(capacity, std::max<int64_t>(256, ((native_past + tokens + 255) / 256) * 256));
        require(q35_kv_read_rows({state, 6}, tokens) == native_rows, "padding policy");
        auto old_k = bytes_of(gk), old_v = bytes_of(gv);
        ggml_context * c = ggml_init({192 * 1024 * 1024, nullptr, false});
        require(c != nullptr, "step graph owner");
        ggml_tensor * w[8]; int64_t wh[8];
        for (int i = 0; i < 8; ++i) {
            auto & raw = d.raw[i];
            w[i] = ggml_new_tensor_4d(c, raw.type, raw.ne[0], raw.ne[1], raw.ne[2], raw.ne[3]);
            std::memcpy(w[i]->data, raw.data.data(), raw.data.size()); wh[i] = (int64_t) (uintptr_t) w[i];
        }
        auto * x = ggml_new_tensor_2d(c, GGML_TYPE_F32, d.hidden, tokens);
        require(first_input >= 0 && first_input + tokens <= 4, "input rows");
        std::memcpy(x->data, d.embeddings.data() + first_input * d.hidden, d.hidden * tokens * sizeof(float));
        auto * pos = ggml_new_tensor_1d(c, GGML_TYPE_I32, tokens * 4);
        std::vector<int64_t> positions(tokens * 4);
        require(q35_kv_positions({state, 6}, tokens, {positions.data(), (int64_t) positions.size()}) == tokens * 4, "Goose positions");
        for (int s = 0; s < 4; ++s) for (int i = 0; i < tokens; ++i) {
            int expected = s < 3 ? native_past + i : 0;
            require(positions[s * tokens + i] == expected, "position policy");
            ((int32_t *) pos->data)[s * tokens + i] = (int32_t) positions[s * tokens + i];
        }
        auto * idx = ggml_new_tensor_1d(c, GGML_TYPE_I64, tokens);
        auto * ni = ggml_new_tensor_1d(c, GGML_TYPE_I64, tokens);
        require(q35_kv_indices({state, 6}, tokens, {(int64_t *) idx->data, tokens}) == tokens, "Goose indices");
        for (int i = 0; i < tokens; ++i) { ((int64_t *) ni->data)[i] = native_past + i; require(((int64_t *) idx->data)[i] == native_past + i, "index policy"); }
        auto * mask = ggml_new_tensor_2d(c, GGML_TYPE_F16, native_rows, tokens);
        auto * native_mask = ggml_new_tensor_2d(c, GGML_TYPE_F16, native_rows, tokens);
        std::vector<float> fm(native_rows * tokens);
        require(q35_kv_mask({state, 6}, tokens, {fm.data(), (int64_t) fm.size()}) == native_rows, "Goose mask");
        for (int i = 0; i < tokens; ++i) for (int j = 0; j < native_rows; ++j) {
            size_t index = i * native_rows + j;
            ((ggml_fp16_t *) mask->data)[index] = ggml_fp32_to_fp16(fm[index]);
            ((ggml_fp16_t *) native_mask->data)[index] = ggml_fp32_to_fp16(j <= native_past + i ? 0.0f : -INFINITY);
        }
        require(std::memcmp(mask->data, native_mask->data, ggml_nbytes(mask)) == 0, "mask policy");
        int64_t work[16] = {0}, nodes[35] = {0};
        int64_t packet[7] = {0, (int64_t) (uintptr_t) c, (int64_t) (uintptr_t) x, 0, 0, GGML_OP_DUP, 0};
        packet_reads = 0;
        require(gb_common((int64_t) (uintptr_t) packet) != 0 && packet_reads > 0, "positive packet witness");
        packet_reads = 0;
        int64_t ticket = q35_kv_begin({state, 6}, tokens);
        require(ticket > 0 && state[4] == tokens && state[2] == native_past, "begin model step");
        auto * wide = ggml_new_tensor_2d(c, GGML_TYPE_Q4_0, d.width + 32, capacity);
        auto * strided = ggml_view_2d(c, wide, d.width, capacity, wide->nb[1], 0);
        auto * multi_indices = ggml_new_tensor_2d(c, GGML_TYPE_I64, tokens, 2);
        auto * multi_mask = ggml_new_tensor_3d(c, GGML_TYPE_F16, native_rows, tokens, 2);
        auto reject_layout = [&](ggml_tensor * ck, ggml_tensor * cv, ggml_tensor * ix, ggml_tensor * ma, int64_t tk) {
            const auto used = ggml_used_mem(c);
            int64_t prior[6]; std::memcpy(prior, state, sizeof prior);
            require(q35_attn_cached((int64_t) (uintptr_t) c, (int64_t) (uintptr_t) x, (int64_t) (uintptr_t) pos,
                (int64_t) (uintptr_t) ma, {wh, 8}, {d.cfg, 14}, (int64_t) (uintptr_t) d.sections,
                (int64_t) (uintptr_t) ck, (int64_t) (uintptr_t) cv, (int64_t) (uintptr_t) ix,
                {state, 6}, tk, {work, 16}, {nodes, 35}) == 0, "invalid cache geometry accepted");
            require(ggml_used_mem(c) == used && std::memcmp(prior, state, sizeof prior) == 0, "invalid geometry allocated or changed state");
            for (auto v : work) require(v == 0, "invalid geometry wrote work");
            for (auto v : nodes) require(v == 0, "invalid geometry wrote nodes");
            ++invalid_layouts;
        };
        reject_layout(strided, gv, idx, mask, ticket);
        reject_layout(gk, strided, idx, mask, ticket);
        reject_layout(gk, gv, multi_indices, mask, ticket);
        reject_layout(gk, gv, idx, multi_mask, ticket);
        reject_layout(gk, gv, idx, mask, ticket + 1);
        reject_layout(wide, wide, idx, mask, ticket);
        int64_t result = q35_attn_cached((int64_t) (uintptr_t) c, (int64_t) (uintptr_t) x, (int64_t) (uintptr_t) pos,
            (int64_t) (uintptr_t) mask, {wh, 8}, {d.cfg, 14}, (int64_t) (uintptr_t) d.sections,
            (int64_t) (uintptr_t) gk, (int64_t) (uintptr_t) gv, (int64_t) (uintptr_t) idx,
            {state, 6}, ticket, {work, 16}, {nodes, 35});
        require(result != 0 && result == nodes[28] && packet_reads == 0, "Goose cached build");
        require(state[2] == native_past && state[4] == tokens, "build advanced valid prefix before compute");
        require(state[5] == ticket, "layer build advanced the model ticket");
        require(q35_kv_commit({state, 6}, ticket + 1) == -1 && state[4] == tokens, "stale commit accepted");
        int64_t pending_meta[8] = {-777, -777, -777, -777, -777, -777, -777, -777};
        require(q35_kv_host_save({state, 6}, d.width, {(uint8_t *) gk->data, (int64_t) ggml_nbytes(gk)},
                 {(uint8_t *) gv->data, (int64_t) ggml_nbytes(gv)}, {pending_meta, 8}, {nullptr, 0}) == -1 && pending_meta[0] == -777,
                "pending snapshot accepted");
        if (abort_only) {
            require(q35_kv_abort({state, 6}, ticket) == native_past, "abort");
            require(q35_kv_commit({state, 6}, ticket) == -1, "aborted ticket committed");
            require(bytes_of(gk) == old_k && bytes_of(gv) == old_v, "unexecuted graph changed cache");
            ggml_free(c); ++aborted; return {};
        }
        auto * graph = ggml_new_graph(c);
        auto * output = (ggml_tensor *) (uintptr_t) result;
        ggml_build_forward_expand(graph, output);
        require(graph_has(graph, (ggml_tensor *) (uintptr_t) nodes[31]) && graph_has(graph, (ggml_tensor *) (uintptr_t) nodes[32]), "writes are not dependencies of attention");
        ggml_tensor * n[29];
        native_branch(c, x, pos, native_mask, w, d.cfg, d.sections, n);
        auto * nk_cur = ggml_view_2d(c, n[14], d.width, tokens, n[14]->nb[2], 0);
        auto * nv_cur = ggml_view_2d(c, n[8], d.width, tokens, n[8]->nb[2], 0);
        auto * nk_write = ggml_set_rows(c, nk, nk_cur, ni);
        auto * nv_write = ggml_set_rows(c, nv, nv_cur, ni);
        // The independent native protocol expands writes explicitly, as the real
        // C++ cache does, rather than copying the candidate's dependency strategy.
        ggml_build_forward_expand(graph, nk_write); ggml_build_forward_expand(graph, nv_write);
        int64_t hd = d.width / d.cfg[1];
        auto * kr = ggml_view_4d(c, nk, hd, d.cfg[1], native_rows, 1, ggml_row_size(GGML_TYPE_Q4_0, hd), row, row * capacity, 0);
        auto * vr = ggml_view_4d(c, nv, hd, d.cfg[1], native_rows, 1, ggml_row_size(GGML_TYPE_Q4_0, hd), row, row * capacity, 0);
        auto * qp = ggml_permute(c, n[13], 0, 2, 1, 3);
        auto * kp = ggml_permute(c, kr, 0, 2, 1, 3);
        auto * vp = ggml_permute(c, vr, 0, 2, 1, 3);
        auto * fa = ggml_flash_attn_ext(c, qp, kp, vp, native_mask, unbits(d.cfg[12]), 0.0f, 0.0f);
        require(ggml_prec_set_acc(fa, GGML_PREC_F32), "native cached precision");
        auto * flat = ggml_reshape_2d(c, fa, hd * d.cfg[0], tokens);
        auto * gated = ggml_mul(c, flat, ggml_sigmoid(c, n[15]));
        auto * residual = ggml_add(c, ggml_mul_mat(c, w[4], gated), x);
        auto * reference = ggml_mul(c, ggml_rms_norm(c, residual, unbits(d.cfg[5])), w[7]);
        ggml_build_forward_expand(graph, reference);
        require(ggml_graph_compute_with_ctx(c, graph, 4) == GGML_STATUS_SUCCESS, "cached compute");
        const size_t active = (native_past + tokens) * row;
        require(std::memcmp(gk->data, nk->data, active) == 0 && std::memcmp(gv->data, nv->data, active) == 0, "active Q4 KV bytes differ");
        require(std::memcmp(gk->data, old_k.data(), native_past * row) == 0 && std::memcmp(gv->data, old_v.data(), native_past * row) == 0, "old prefix was overwritten");
        require(std::memcmp((uint8_t *) gk->data + active, old_k.data() + active, old_k.size() - active) == 0 &&
                std::memcmp((uint8_t *) gv->data + active, old_v.data() + active, old_v.size() - active) == 0, "write escaped token range");
        require(ggml_are_same_shape(output, reference) && std::memcmp(output->data, reference->data, ggml_nbytes(reference)) == 0, "cached attention output differs");
        std::vector<float> answer((float *) output->data, (float *) output->data + ggml_nelements(output));
        for (float value : answer) require(std::isfinite(value), "nonfinite cached output");
        require(q35_kv_commit({state, 6}, ticket) == native_past + tokens && state[4] == 0, "commit");
        require(q35_kv_commit({state, 6}, ticket) == -1, "duplicate commit");
        native_past += tokens; ggml_free(c); ++completed;
        std::printf("STEP old_prefix=%d tokens=%d prefix=%d read_rows=%lld graph_destroyed=1 PASS\n", old_past, tokens, native_past, (long long) native_rows);
        return answer;
    }
};

int main(int argc, char ** argv) {
    if (argc != 2) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0); goose_init(0, nullptr);
    int plans = planner_checks();
    Data data = load_data(argv[1]); Session s(data);
    int64_t empty_meta[8] = {0}, saved_meta[8] = {0};
    std::vector<uint8_t> empty, saved(2 * 3 * s.row);
    auto k = [&]() { return sl_u8{(uint8_t *) s.gk->data, (int64_t) ggml_nbytes(s.gk)}; };
    auto v = [&]() { return sl_u8{(uint8_t *) s.gv->data, (int64_t) ggml_nbytes(s.gv)}; };
    require(q35_kv_host_save({s.state, 6}, data.width, k(), v(), {empty_meta, 8}, {nullptr, 0}) == 0, "empty save");
    auto prefill = s.step(0, 3);
    require(q35_kv_host_save({s.state, 6}, data.width, k(), v(), {saved_meta, 8}, {saved.data(), (int64_t) saved.size()}) == (int64_t) saved.size(), "prefix save");
    std::vector<uint8_t> nk_save((uint8_t *) s.nk->data, (uint8_t *) s.nk->data + 3 * s.row);
    std::vector<uint8_t> nv_save((uint8_t *) s.nv->data, (uint8_t *) s.nv->data + 3 * s.row);
    auto decode = s.step(3, 1);
    require(q35_kv_host_restore({s.state, 6}, data.width, k(), v(), {saved_meta, 8}, {saved.data(), (int64_t) saved.size()}) == 3, "restore prefix3");
    std::memcpy(s.nk->data, nk_save.data(), nk_save.size()); std::memcpy(s.nv->data, nv_save.data(), nv_save.size()); s.native_past = 3;
    require(same_float_bytes(s.step(3, 1), decode), "restore replay output differs");
    require(q35_kv_rewind({s.state, 6}, 1) == 1, "rewind"); s.native_past = 1; s.step(1, 1);
    auto before_k = bytes_of(s.gk), before_v = bytes_of(s.gv); int64_t before_state[6]; std::memcpy(before_state, s.state, sizeof before_state);
    saved_meta[2] = Session::capacity + 1;
    require(q35_kv_host_restore({s.state, 6}, data.width, k(), v(), {saved_meta, 8}, {saved.data(), (int64_t) saved.size()}) == -1, "invalid checkpoint accepted");
    require(bytes_of(s.gk) == before_k && bytes_of(s.gv) == before_v && std::memcmp(s.state, before_state, sizeof before_state) == 0, "invalid restore was not transactional"); saved_meta[2] = 3;
    require(q35_kv_host_restore({s.state, 6}, data.width, k(), v(), {empty_meta, 8}, {nullptr, 0}) == 0, "empty restore");
    std::memset(s.nk->data, 0, ggml_nbytes(s.nk)); std::memset(s.nv->data, 0, ggml_nbytes(s.nv)); s.native_past = 0;
    require(same_float_bytes(s.step(0, 3), prefill), "same chunking prefix replay differs");
    s.step(3, 1, true); require(s.state[2] == 3 && s.state[4] == 0, "abort advanced prefix");
    require(same_float_bytes(s.step(3, 1), decode), "retry after abort differs");
    std::printf("PASS persistent Q4 KV computed_steps=%d aborted_builds=%d planner_checks=%d invalid_layouts=%d restore_replay=1 rewind=1 exact_KV_and_outputs=1 model_server_runs=0\n", s.completed, s.aborted, plans, s.invalid_layouts);
}
