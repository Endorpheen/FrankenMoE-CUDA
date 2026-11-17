#include "ggml.h"
#include "ggml-cpu.h"
#include "gguf.h"
#include "runtime.h"
#include <openssl/sha.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

static_assert(GGML_OP_RMS_NORM == 25 && GGML_OP_MUL == 7 && GGML_OP_ADD == 2, "pinned graph ABI");
static_assert(GGML_OP_RESHAPE == 36 && GGML_OP_PERMUTE == 38, "pinned tensor ABI");
static_assert(GGML_UNARY_OP_SIGMOID == 7 && GGML_ROPE_TYPE_IMROPE == 40, "Qwen3.5 ABI");

static uint64_t packet_reads;
extern "C" int64_t fg158_arg(int64_t p, int64_t i) {
    ++packet_reads;
    return (int64_t) ((const uint64_t *) (uintptr_t) p)[i];
}

static void require(bool value, const char * message) {
    if (!value) { std::fprintf(stderr, "FAIL %s\n", message); std::exit(3); }
}

static int64_t bits(float value) {
    int32_t result;
    std::memcpy(&result, &value, sizeof result);
    return result;
}

static float unbits(int64_t value) {
    int32_t word = (int32_t) value;
    float result;
    std::memcpy(&result, &word, sizeof result);
    return result;
}

static int64_t integer(gguf_context * metadata, const char * name) {
    int64_t id = gguf_find_key(metadata, name);
    require(id >= 0, name);
    switch (gguf_get_kv_type(metadata, id)) {
        case GGUF_TYPE_UINT32: return gguf_get_val_u32(metadata, id);
        case GGUF_TYPE_INT32: return gguf_get_val_i32(metadata, id);
        case GGUF_TYPE_UINT64: return (int64_t) gguf_get_val_u64(metadata, id);
        case GGUF_TYPE_INT64: return gguf_get_val_i64(metadata, id);
        default: require(false, "metadata integer type"); return 0;
    }
}

static float real(gguf_context * metadata, const char * name) {
    int64_t id = gguf_find_key(metadata, name);
    require(id >= 0 && gguf_get_kv_type(metadata, id) == GGUF_TYPE_FLOAT32, name);
    return gguf_get_val_f32(metadata, id);
}

static std::vector<uint8_t> read_range(int fd, uint64_t file_bytes, uint64_t offset, size_t bytes) {
    require(bytes <= 64 * 1024 * 1024 && offset <= file_bytes && bytes <= file_bytes - offset, "bounded tensor read");
    std::vector<uint8_t> result(bytes);
    size_t done = 0;
    while (done < bytes) {
        ssize_t got = pread(fd, result.data() + done, bytes - done, (off_t) (offset + done));
        require(got > 0, "short GGUF read");
        done += (size_t) got;
    }
    return result;
}

static std::string digest(const std::vector<uint8_t> & data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(data.data(), data.size(), hash);
    char text[SHA256_DIGEST_LENGTH * 2 + 1];
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) std::snprintf(text + i * 2, 3, "%02x", hash[i]);
    return text;
}

struct Weight {
    std::string name;
    ggml_type type;
    int64_t ne[4];
    uint64_t offset;
    std::vector<uint8_t> data;
};

static Weight load_weight(gguf_context * metadata, int fd, uint64_t file_bytes, const char * name) {
    int64_t id = gguf_find_tensor(metadata, name);
    require(id >= 0, name);
    Weight w;
    w.name = name;
    w.type = gguf_get_tensor_type(metadata, id);
    std::memcpy(w.ne, gguf_get_tensor_ne(metadata, id), sizeof w.ne);
    w.offset = gguf_get_data_offset(metadata) + gguf_get_tensor_offset(metadata, id);
    w.data = read_range(fd, file_bytes, w.offset, gguf_get_tensor_size(metadata, id));
    std::printf("WEIGHT name=%s type=%d ne=%lld,%lld,%lld,%lld offset=%llu bytes=%zu sha256=%s\n",
                name, w.type, (long long) w.ne[0], (long long) w.ne[1], (long long) w.ne[2], (long long) w.ne[3],
                (unsigned long long) w.offset, w.data.size(), digest(w.data).c_str());
    return w;
}

static void native_branch(ggml_context * c, ggml_tensor * x, ggml_tensor * pos, ggml_tensor * mask,
                          ggml_tensor ** w, const int64_t * cfg, int * sections, ggml_tensor ** n) {
    const int64_t heads = cfg[0], kv_heads = cfg[1], hd = w[1]->ne[1] / (2 * heads), tokens = x->ne[1];
    n[0] = ggml_rms_norm(c, x, unbits(cfg[5]));
    n[1] = ggml_mul(c, n[0], w[0]);
    n[2] = ggml_mul_mat(c, w[1], n[1]);
    n[3] = ggml_mul_mat(c, w[2], n[1]);
    n[4] = ggml_mul_mat(c, w[3], n[1]);
    n[5] = ggml_view_4d(c, n[2], hd, heads, tokens, 1, hd * 8, hd * 8 * heads, hd * 8 * heads * tokens, 0);
    n[6] = ggml_view_4d(c, n[2], hd, heads, tokens, 1, hd * 8, hd * 8 * heads, hd * 8 * heads * tokens, hd * 4);
    n[7] = ggml_reshape_4d(c, n[3], hd, kv_heads, tokens, 1);
    n[8] = ggml_reshape_4d(c, n[4], hd, kv_heads, tokens, 1);
    n[9] = ggml_rms_norm(c, n[5], unbits(cfg[5]));
    n[10] = ggml_mul(c, n[9], w[5]);
    n[11] = ggml_rms_norm(c, n[7], unbits(cfg[5]));
    n[12] = ggml_mul(c, n[11], w[6]);
    n[13] = ggml_rope_multi(c, n[10], pos, nullptr, cfg[2], sections, cfg[3], cfg[4], unbits(cfg[6]), unbits(cfg[7]),
                           unbits(cfg[8]), unbits(cfg[9]), unbits(cfg[10]), unbits(cfg[11]));
    n[14] = ggml_rope_multi(c, n[12], pos, nullptr, cfg[2], sections, cfg[3], cfg[4], unbits(cfg[6]), unbits(cfg[7]),
                           unbits(cfg[8]), unbits(cfg[9]), unbits(cfg[10]), unbits(cfg[11]));
    n[15] = ggml_cont_2d(c, n[6], hd * heads, tokens);
    n[16] = ggml_permute(c, n[13], 0, 2, 1, 3);
    n[17] = ggml_permute(c, n[14], 0, 2, 1, 3);
    n[18] = ggml_permute(c, n[8], 0, 2, 1, 3);
    n[19] = ggml_cast(c, n[17], GGML_TYPE_F16);
    n[20] = ggml_cast(c, n[18], GGML_TYPE_F16);
    n[21] = ggml_flash_attn_ext(c, n[16], n[19], n[20], mask, unbits(cfg[12]), 0.0f, 0.0f);
    require(ggml_prec_set_acc(n[21], GGML_PREC_F32), "native flash F32 precision");
    n[22] = ggml_reshape_4d(c, n[21], hd * heads, tokens, 1, 1);
    n[23] = ggml_sigmoid(c, n[15]);
    n[24] = ggml_mul(c, n[22], n[23]);
    n[25] = ggml_mul_mat(c, w[4], n[24]);
    n[26] = ggml_add(c, n[25], x);
    n[27] = ggml_rms_norm(c, n[26], unbits(cfg[5]));
    n[28] = ggml_mul(c, n[27], w[7]);
}

static void compare_metadata(ggml_tensor ** candidate, ggml_tensor ** reference, ggml_tensor ** weights,
                             ggml_tensor * x, ggml_tensor * positions, ggml_tensor * mask) {
    std::unordered_map<ggml_tensor *, ggml_tensor *> map;
    map[nullptr] = nullptr;
    map[x] = x; map[positions] = positions; map[mask] = mask;
    for (int i = 0; i < 8; ++i) map[weights[i]] = weights[i];
    for (int i = 0; i < 29; ++i) map[reference[i]] = candidate[i];
    for (int i = 0; i < 29; ++i) {
        auto * a = candidate[i]; auto * b = reference[i];
        require(a && a->type == b->type && a->op == b->op && a->flags == b->flags, "node type/op/flags");
        require(std::memcmp(a->ne, b->ne, sizeof a->ne) == 0 && std::memcmp(a->nb, b->nb, sizeof a->nb) == 0, "shape/strides");
        require(std::strcmp(a->name, b->name) == 0, "node debug names");
        if (std::memcmp(a->op_params, b->op_params, sizeof a->op_params)) {
            std::fprintf(stderr, "node=%d op=%d params differ\n", i, a->op);
            require(false, "node params");
        }
        for (int j = 0; j < GGML_MAX_SRC; ++j) require(map.count(b->src[j]) && a->src[j] == map[b->src[j]], "node source edges");
        require(map.count(b->view_src) && a->view_src == map[b->view_src] && a->view_offs == b->view_offs, "node view owner/offset");
    }
}

int main(int argc, char ** argv) {
    if (argc != 2) return 2;
    setvbuf(stdout, nullptr, _IONBF, 0);
    goose_init(0, nullptr);
    int fd = open(argv[1], O_RDONLY);
    require(fd >= 0, "GGUF open");
    struct stat before, after;
    require(fstat(fd, &before) == 0 && before.st_size == 21713463136LL, "pinned GGUF size");
    ggml_context * metadata_tensors = nullptr;
    gguf_context * metadata = gguf_init_from_file(argv[1], {true, &metadata_tensors});
    require(metadata != nullptr, "GGUF metadata");
    int64_t architecture = gguf_find_key(metadata, "general.architecture");
    require(architecture >= 0 && std::strcmp(gguf_get_val_str(metadata, architecture), "qwen35moe") == 0, "GGUF architecture");
    const char * names[8] = {"blk.3.attn_norm.weight", "blk.3.attn_q.weight", "blk.3.attn_k.weight", "blk.3.attn_v.weight",
                             "blk.3.attn_output.weight", "blk.3.attn_q_norm.weight", "blk.3.attn_k_norm.weight", "blk.3.post_attention_norm.weight"};
    std::vector<Weight> raw;
    size_t weight_bytes = 0;
    for (const char * name : names) { raw.push_back(load_weight(metadata, fd, before.st_size, name)); weight_bytes += raw.back().data.size(); }
    const int64_t heads = integer(metadata, "qwen35moe.attention.head_count");
    const int64_t kv_heads = integer(metadata, "qwen35moe.attention.head_count_kv");
    const int64_t hidden = integer(metadata, "qwen35moe.embedding_length");
    const int64_t hd = raw[1].ne[1] / (2 * heads);
    int64_t cfg[14] = {heads, kv_heads, integer(metadata, "qwen35moe.rope.dimension_count"), GGML_ROPE_TYPE_IMROPE,
                      integer(metadata, "qwen35moe.context_length"), bits(real(metadata, "qwen35moe.attention.layer_norm_rms_epsilon")),
                      bits(real(metadata, "qwen35moe.rope.freq_base")), bits(1.0f), bits(0.0f), bits(1.0f), bits(32.0f), bits(1.0f),
                      bits(1.0f / std::sqrt((float) hd)), GGML_PREC_F32};
    int64_t section_id = gguf_find_key(metadata, "qwen35moe.rope.dimension_sections");
    require(section_id >= 0 && gguf_get_arr_n(metadata, section_id) == 4, "rope sections");
    require(gguf_get_arr_type(metadata, section_id) == GGUF_TYPE_INT32 || gguf_get_arr_type(metadata, section_id) == GGUF_TYPE_UINT32, "rope section type");
    int sections[4]; std::memcpy(sections, gguf_get_arr_data(metadata, section_id), sizeof sections);
    int64_t embedding_id = gguf_find_tensor(metadata, "token_embd.weight");
    require(embedding_id >= 0 && gguf_get_tensor_ne(metadata, embedding_id)[0] == hidden, "embedding table");
    ggml_type embedding_type = gguf_get_tensor_type(metadata, embedding_id);
    const size_t row_bytes = ggml_row_size(embedding_type, hidden);
    const uint64_t embedding_offset = gguf_get_data_offset(metadata) + gguf_get_tensor_offset(metadata, embedding_id);
    std::vector<float> inputs((size_t) hidden * 3);
    int64_t token_ids[3] = {1, 42, 100};
    for (int i = 0; i < 3; ++i) {
        require(token_ids[i] < gguf_get_tensor_ne(metadata, embedding_id)[1], "embedding token range");
        auto data = read_range(fd, before.st_size, embedding_offset + token_ids[i] * row_bytes, row_bytes);
        if (embedding_type == GGML_TYPE_F32) std::memcpy(inputs.data() + i * hidden, data.data(), row_bytes);
        else {
            const auto * traits = ggml_get_type_traits(embedding_type);
            require(traits && traits->to_float, "embedding dequantizer");
            traits->to_float(data.data(), inputs.data() + i * hidden, hidden);
        }
        std::printf("EMBED token=%lld offset=%llu bytes=%zu sha256=%s\n", (long long) token_ids[i],
                    (unsigned long long) (embedding_offset + token_ids[i] * row_bytes), row_bytes, digest(data).c_str());
    }
    std::printf("CONFIG hidden=%lld heads=%lld kv_heads=%lld head_dim=%lld rope=%lld n_rot=%lld context=%lld sections=%d,%d,%d,%d weight_bytes=%zu\n",
                (long long) hidden, (long long) heads, (long long) kv_heads, (long long) hd, (long long) cfg[3], (long long) cfg[2],
                (long long) cfg[4], sections[0], sections[1], sections[2], sections[3], weight_bytes);
    gguf_free(metadata); ggml_free(metadata_tensors);
    int cases = 0;
    int64_t values_compared = 0;
    int invalid_cases = 0;
    for (bool materialized : {true, false}) for (int tokens : {1, 3}) for (int position : {0, 17}) {
        ggml_context * context = ggml_init({192 * 1024 * 1024, nullptr, false});
        require(context != nullptr, "component context");
        ggml_tensor * w[8]; int64_t handles[8];
        for (int i = 0; i < 8; ++i) {
            w[i] = ggml_new_tensor_4d(context, raw[i].type, raw[i].ne[0], raw[i].ne[1], raw[i].ne[2], raw[i].ne[3]);
            require(ggml_nbytes(w[i]) == raw[i].data.size(), "tensor byte geometry");
            std::memcpy(w[i]->data, raw[i].data.data(), raw[i].data.size());
            ggml_set_name(w[i], raw[i].name.c_str()); handles[i] = (int64_t) (uintptr_t) w[i];
        }
        ggml_tensor * x = ggml_new_tensor_2d(context, GGML_TYPE_F32, hidden, tokens);
        std::memcpy(x->data, inputs.data(), hidden * tokens * sizeof(float));
        ggml_tensor * pos = ggml_new_tensor_1d(context, GGML_TYPE_I32, 4 * tokens);
        for (int s = 0; s < 4; ++s) for (int i = 0; i < tokens; ++i) ((int32_t *) pos->data)[s * tokens + i] = s < 3 ? position + i : 0;
        ggml_tensor * mask = ggml_new_tensor_2d(context, GGML_TYPE_F16, tokens, tokens);
        for (int i = 0; i < tokens; ++i) for (int j = 0; j < tokens; ++j)
            ((ggml_fp16_t *) mask->data)[i * tokens + j] = ggml_fp32_to_fp16(j <= i ? 0.0f : -INFINITY);
        int64_t work[16] = {0}, nodes[29] = {0};
        int64_t packet[7] = {0, (int64_t) (uintptr_t) context, (int64_t) (uintptr_t) x, 0, 0, GGML_OP_DUP, 0};
        packet_reads = 0;
        require(gb_common((int64_t) (uintptr_t) packet) != 0 && packet_reads > 0, "positive packet witness");
        packet_reads = 0;
        size_t used = ggml_used_mem(context);
        require(q35_attn_block((int64_t) (uintptr_t) context, (int64_t) (uintptr_t) x, (int64_t) (uintptr_t) pos,
                              (int64_t) (uintptr_t) mask, {handles, 8}, {cfg, 14}, (int64_t) (uintptr_t) sections, {work, 15}, {nodes, 29}) == 0,
                "short workspace accepted");
        require(used == ggml_used_mem(context), "short workspace mutated context"); ++invalid_cases;
        int64_t result = q35_attn_block((int64_t) (uintptr_t) context, (int64_t) (uintptr_t) x, (int64_t) (uintptr_t) pos,
                                       (int64_t) (uintptr_t) mask, {handles, 8}, {cfg, 14}, (int64_t) (uintptr_t) sections, {work, 16}, {nodes, 29});
        require(result == nodes[28] && result != 0 && packet_reads == 0, "Goose branch did not answer without packet reads");
        ggml_tensor * candidate[29], * reference[29];
        for (int i = 0; i < 29; ++i) candidate[i] = (ggml_tensor *) (uintptr_t) nodes[i];
        native_branch(context, x, pos, mask, w, cfg, sections, reference);
        compare_metadata(candidate, reference, w, x, pos, mask);
        if (materialized) for (int i = 0; i < 29; ++i) { ggml_set_output(candidate[i]); ggml_set_output(reference[i]); }
        ggml_cgraph * graph = ggml_new_graph(context);
        ggml_build_forward_expand(graph, candidate[28]); ggml_build_forward_expand(graph, reference[28]);
        require(ggml_graph_compute_with_ctx(context, graph, 4) == GGML_STATUS_SUCCESS, "component CPU compute");
        for (int i = 0; i < 29; ++i) {
            if (!materialized && (i == 0 || i == 9 || i == 11 || i == 27)) continue;
            const size_t bytes = ggml_nbytes(reference[i]);
            // View payloads may have holes. Compare only addressed F32/F16 elements.
            const int64_t count = ggml_nelements(reference[i]);
            const size_t element = ggml_type_size(reference[i]->type);
            require(!ggml_is_quantized(reference[i]->type) && bytes > 0, "observed intermediate type");
            for (int64_t index = 0; index < count; ++index) {
                int64_t z = index; size_t oa = 0, ob = 0;
                for (int d = 0; d < 4; ++d) { int64_t coordinate = z % reference[i]->ne[d]; z /= reference[i]->ne[d]; oa += coordinate * candidate[i]->nb[d]; ob += coordinate * reference[i]->nb[d]; }
                if (std::memcmp((char *) candidate[i]->data + oa, (char *) reference[i]->data + ob, element)) {
                    std::fprintf(stderr, "node=%d tokens=%d position=%d materialized=%d element=%lld\n", i, tokens, position, materialized, (long long) index);
                    require(false, "intermediate numerical bytes differ");
                }
                if (reference[i]->type == GGML_TYPE_F32) require(std::isfinite(*(float *) ((char *) candidate[i]->data + oa)), "nonfinite intermediate");
            }
            values_compared += count;
        }
        require(std::memcmp(candidate[26]->data, x->data, ggml_nbytes(x)) != 0, "attention residual is an input copy");
        std::printf("CASE tokens=%d position=%d materialized=%d nodes=29 packet_reads=%llu PASS\n", tokens, position, materialized, (unsigned long long) packet_reads);
        ggml_free(context); ++cases;
    }
    require(fstat(fd, &after) == 0 && before.st_size == after.st_size && before.st_mtim.tv_sec == after.st_mtim.tv_sec && before.st_mtim.tv_nsec == after.st_mtim.tv_nsec, "model changed during component gate");
    close(fd);
    std::printf("PASS real-weight Goose attention cases=%d nodes_per_case=29 values=%lld invalid_cases=%d quantized_weight_bytes=%zu model_server_runs=0\n",
                cases, (long long) values_compared, invalid_cases, weight_bytes);
}
