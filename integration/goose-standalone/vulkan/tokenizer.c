#include "tokenizer.h"
#include "qwen2.h"
#include "bonsai-model.h"
#include "gguf.h"

#include <stdlib.h>
#include <string.h>

struct fg_vk_tokenizer {
    sl_u8 blob;
    sl_i64 offsets, lengths, tokens, merge_offsets, merge_lengths, merges, attributes, order, eog;
    int64_t special_count, eog_count;
    int include_marks;
};

static _Thread_local int include_marks;

int64_t fg_vk_tokenizer_marks(void) {
    return include_marks;
}

static sl_i64 table(int64_t count) {
    return (sl_i64){calloc((size_t)count, sizeof(int64_t)), count};
}

void fg_vk_tokenizer_close(fg_vk_tokenizer *t) {
    if (!t) return;
    free(t->blob.data);
    free(t->offsets.data); free(t->lengths.data); free(t->tokens.data);
    free(t->merge_offsets.data); free(t->merge_lengths.data); free(t->merges.data);
    free(t->attributes.data); free(t->order.data); free(t->eog.data);
    free(t);
}

static fg_vk_tokenizer *from_gguf(sl_u8 data, sl_i64 state, sl_i64 meta,
    int64_t tokens, int64_t merges, int64_t types, int64_t eos, int64_t vocabulary, int marks) {
    int64_t size[5];
    sl_i64 need = {size, 5};
    if (bt_prepare_need(data, state, meta, tokens, merges, need) ||
        size[0] <= 0 || size[0] > data.len || size[1] != vocabulary ||
        size[2] <= 0 || size[2] > 1 << 20 || gr_arr_n(state, meta, types) != size[1]) return NULL;
    fg_vk_tokenizer *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->include_marks = marks;
    t->blob = (sl_u8){malloc((size_t)size[0]), size[0]};
    t->offsets = table(size[1]); t->lengths = table(size[1]); t->tokens = table(size[3]);
    t->merge_offsets = table(size[2]); t->merge_lengths = table(size[2]); t->merges = table(size[4]);
    t->attributes = table(size[1]); t->order = table(size[1]); t->eog = table(size[1]);
    sl_i64 type = table(size[1]);
    int ok = t->blob.data && t->offsets.data && t->lengths.data && t->tokens.data &&
        t->merge_offsets.data && t->merge_lengths.data && t->merges.data &&
        t->attributes.data && t->order.data && t->eog.data && type.data;
    if (ok) ok = !bt_prepare_from_gguf(data, state, meta, tokens, merges, t->blob, t->offsets, t->lengths,
        t->tokens, t->merge_offsets, t->merge_lengths, t->merges, need);
    for (int64_t i = 0; i < type.len && ok; ++i) {
        ok = !gr_array_int(data, state, meta, types, i, (sl_i64){type.data + i, 1});
    }
    int64_t stats[4];
    if (ok) {
        ok = !bt_attrs(t->blob, t->offsets, t->lengths, type, t->attributes) &&
             !bt_build(t->blob, t->offsets, t->lengths, t->tokens, t->merge_offsets, t->merge_lengths,
                       t->merges, (sl_i64){stats, 4});
    }
    free(type.data);
    if (ok) {
        t->special_count = bt_special_order(t->blob, t->offsets, t->lengths, t->attributes, t->order);
        t->eog_count = bt_eog_ids(t->blob, t->offsets, t->lengths, eos, -1, -1, t->eog);
        ok = t->special_count >= 0 && t->eog_count > 0;
    }
    if (!ok) {
        fg_vk_tokenizer_close(t);
        return NULL;
    }
    return t;
}

fg_vk_tokenizer *fg_vk_model_tokenizer_open(const char *path, fg_vk_model_kind kind) {
    FILE *model = fopen(path, "rb");
    if (!model) {
        perror(path);
        return NULL;
    }
    fg_gguf file = {0};
    fg_vk_tokenizer *tokenizer = NULL;
    const int bonsai = kind == FG_VK_MODEL_BONSAI;
    const char *type = bonsai ? "qwen35" : "qwen2";
    const int64_t vocabulary = bonsai ? FG_BONSAI_VOCAB : FG_QWEN_VOCAB;
    int64_t eos;
    if (fg_gguf_read_header(&file, model) >= 0 && fg_gguf_metadata_is(&file, "general.architecture", type) &&
        fg_gguf_metadata_is(&file, "tokenizer.ggml.model", "gpt2") && fg_gguf_metadata_is(&file, "tokenizer.ggml.pre", type) &&
        !gr_meta_int(file.data, FG_GGUF_WORDS(file.state), FG_GGUF_WORDS(file.metadata),
                     fg_gguf_metadata(&file, "tokenizer.ggml.eos_token_id"), fg_gguf_words(&eos, 1)) &&
        eos >= 0 && eos < vocabulary) {
        tokenizer = from_gguf(file.data, FG_GGUF_WORDS(file.state), FG_GGUF_WORDS(file.metadata),
            fg_gguf_metadata(&file, "tokenizer.ggml.tokens"), fg_gguf_metadata(&file, "tokenizer.ggml.merges"),
            fg_gguf_metadata(&file, "tokenizer.ggml.token_type"), eos, vocabulary, bonsai);
    }
    free(file.data.data);
    fclose(model);
    return tokenizer;
}

int fg_vk_tokenizer_encode(const fg_vk_tokenizer *t, const char *text, size_t bytes,
    int special, uint32_t *ids, uint32_t capacity) {
    if (!t || !text || !bytes || bytes > FG_VK_TEXT_BYTES || !ids || !capacity || capacity > 32768) return -1;
    const int64_t n = (int64_t)bytes + 1;
    sl_i64 workspace[6];
    const int64_t lengths[] = {capacity, n, n * 4, n * 24, n * 15 + 40, 2};
    int64_t total = 0;
    for (size_t i = 0; i < sizeof(lengths) / sizeof(*lengths); ++i) total += lengths[i];
    int64_t *words = malloc((size_t)total * sizeof(*words));
    unsigned char *encoded = malloc((size_t)n * 8);
    if (!words || !encoded) {
        free(words); free(encoded);
        return -1;
    }
    int64_t at = 0;
    for (size_t i = 0; i < sizeof(lengths) / sizeof(*lengths); ++i) {
        workspace[i] = (sl_i64){words + at, lengths[i]};
        at += lengths[i];
    }
    int64_t info[8];
    const int previous = include_marks;
    include_marks = t->include_marks;
    int64_t count = bt_tokenize(t->blob, t->offsets, t->lengths, t->tokens, t->merge_offsets, t->merge_lengths,
        t->merges, t->order, t->special_count, (sl_u8){(unsigned char *)text, (int64_t)bytes},
        workspace[0], special, workspace[1], workspace[2], (sl_u8){encoded, n * 8},
        workspace[3], workspace[4], workspace[5], (sl_i64){info, 8});
    include_marks = previous;
    if (count < 0 || count > capacity) count = -1;
    for (int64_t i = 0; i < count; ++i) {
        if (words[i] < 0 || words[i] >= t->offsets.len) {
            count = -1;
            break;
        }
        ids[i] = (uint32_t)words[i];
    }
    free(encoded); free(words);
    return (int)count;
}

int fg_vk_tokenizer_piece(const fg_vk_tokenizer *t, uint32_t token, char *out, size_t bytes) {
    if (!t || token >= t->offsets.len || !out || bytes > INT32_MAX) return -1;
    return (int)bt_token_to_piece(t->blob, t->offsets, t->lengths, t->attributes, token,
        (sl_u8){(unsigned char *)out, (int64_t)bytes}, 0, 0, 1, 0);
}

int fg_vk_tokenizer_is_eog(const fg_vk_tokenizer *t, uint32_t token) {
    return t && bt_is_eog(t->eog, t->eog_count, token) != 0;
}
