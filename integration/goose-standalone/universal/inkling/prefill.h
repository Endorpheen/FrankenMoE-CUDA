enum { max_prefill_batch = INKLING_PREFILL_LIMIT };

struct prefill_token {
    float current[model_width];
    float prelude[model_width];
    float previous[model_width];
    float blocks[12 * model_width];
    float bank[12 * model_width];
    float messages[4][model_width];
    int count, prelude_count, bank_count;
};

_Static_assert(offsetof(struct prefill_token, prelude) == 9216 && offsetof(struct prefill_token, previous) == 18432 &&
    offsetof(struct prefill_token, blocks) == 27648 && offsetof(struct prefill_token, bank) == 138240 &&
    offsetof(struct prefill_token, messages) == 248832 && offsetof(struct prefill_token, count) == 285696 &&
    offsetof(struct prefill_token, prelude_count) == 285700 && offsetof(struct prefill_token, bank_count) == 285704,
    "Goose prefill repeat input ABI");

extern struct ggml_tensor *fg_i2_host_rows_input(struct ggml_context *ctx, float *rows, int width, int count);
extern void prefill_projection(struct decoder *d, int64_t id, float *input, int width, float *output, int rows, int count);
extern void prefill_loop_inputs(struct decoder *d, struct prefill_token *token, int layer, int pass, bool device_loop, bool device_prelude);

struct prefill_workspace {
    float inputs[3][max_prefill_batch][model_width];
    float projections[max_prefill_batch * 8664];
    float attention[max_prefill_batch][attn_width];
    float ff[max_prefill_batch][model_width];
    float (*linear)[model_width];
    float linear_storage[max_prefill_batch][model_width];
    float residual[max_prefill_batch][model_width];
    float control_input[max_prefill_batch][2 * model_width];
    float control_hidden[max_prefill_batch][64];
    float forget[max_prefill_batch][model_width];
    float gain[max_prefill_batch][model_width];
    float combined[max_prefill_batch][2 * model_width];
};

_Static_assert(sizeof(struct prefill_token) == 285708, "Goose prefill token stride ABI");
extern void fg_i2_prefill_control(struct decoder *d, const int64_t *aux, int which, int count,
    struct prefill_token *tokens, float *current, const uintptr_t *buffers);

static void prefill_control(struct decoder *d, const int64_t *aux, int which, int count,
                            struct prefill_token *tokens, float (*current)[model_width],
                            struct prefill_workspace *scratch) {
    const uintptr_t buffers[] = {(uintptr_t)scratch->control_input, (uintptr_t)scratch->control_hidden,
        (uintptr_t)scratch->gain, (uintptr_t)scratch->forget, (uintptr_t)scratch->combined};
    fg_i2_prefill_control(d, aux, which, count, tokens, current[0], buffers);
}

struct i2_prefill_work {
    struct decoder *decoder;
    struct prefill_token *tokens;
    struct prefill_workspace *scratch;
    const int64_t *aux;
    int layer, pass;
    bool sliding;
    float **message;
    bool after_attention;
    bool shared_attention_input;
    sl_f32 postconv_weights;
    float *postconv_history;
    int postconv_rows;
    float *logits;
    bool skip_host_mix, device_attention_mix, device_current, device_boundary, device_loop, device_prelude, poison_host;
};

extern int64_t inkling_row_major_i2_postconv_batch(sl_f32 input, sl_f32 weights,
    sl_f32 history, sl_f32 out, int64_t width, int64_t from_channel, int64_t to_channel);

void fg_i2_prefill_partition(uint64_t address, int64_t begin, int64_t end) {
    const struct i2_prefill_work *work = (const void *)(uintptr_t)address;
    if (work->logits) {
        float *values = work->logits + begin;
        const int64_t count = end - begin;
        assert(I2_CALL(i2_text_logits)(vec(values, count), count, 30.0f) == count);
        for (int64_t i = 0; i < count; ++i) assert(isfinite(values[i]));
        return;
    }
    struct decoder *d = work->decoder;
    struct prefill_token *tokens = work->tokens;
    struct prefill_workspace *scratch = work->scratch;
    const int64_t *aux = work->aux;
    int layer = work->layer, pass = work->pass;
    const bool sliding = work->sliding;
    float **message = work->message;
    if (work->postconv_rows) {
        const int64_t items = (int64_t)work->postconv_rows * model_width;
        assert(inkling_row_major_i2_postconv_batch(
            vec(scratch->linear[0], items),
            work->postconv_weights,
            vec(work->postconv_history, 3 * model_width),
            vec(scratch->residual[0], items), model_width, begin, end) == end - begin);
        return;
    }
    if (work->after_attention) {
        for (int t = begin; t < end; ++t) {
            struct prefill_token *token = tokens + t;
            float *next = scratch->residual[t];
            float *ffn = scratch->ff[t];
            if (layer % 6 == 0) {
                assert(token->count < 11);
                float *history = token->blocks + token->count++ * model_width;
                if (!work->device_boundary) save_history_row(history, token->current);
            } else if (!work->skip_host_mix) {
                for (int i = 0; i < model_width; ++i) {
                    next[i] += token->current[i];
                }
            }
            if (!work->skip_host_mix) {
                mix(token->blocks, token->count, next, coefficients(d, aux[9], model_width), 1, ffn);
            } else if (work->poison_host) {
                for (int i = 0; i < model_width; ++i) ffn[i] = NAN;
            }
        }
        return;
    }
    for (int t = begin; t < end; ++t) {
        struct prefill_token *token = tokens + t;
        prefill_loop_inputs(d, token, layer, pass, work->device_loop, work->device_prelude);
        if (work->shared_attention_input) {
            if (aux[10] >= 0 && !work->device_attention_mix) {
                mix(token->blocks, token->count, token->current,
                    coefficients(d, aux[10], model_width), 1, scratch->inputs[0][t]);
            } else if (!work->device_current) {
                memcpy(scratch->inputs[0][t], token->current, sizeof token->current);
            }
            if (message && !work->device_loop) {
                for (int i = 0; i < 3; ++i) {
                    memcpy(message[i] + t * model_width, token->messages[i], sizeof token->current);
                }
            }
            continue;
        }
        if (work->device_attention_mix) {
            if (!work->device_current) memcpy(scratch->inputs[0][t], token->current, sizeof token->current);
            if (message && !work->device_loop) {
                for (int i = 0; i < 3; ++i)
                    memcpy(message[i] + t * model_width, token->messages[i], sizeof token->current);
            }
            continue;
        }
        float selected[3][model_width];
        if (aux[10] >= 0) {
            const int projections = sliding ? 1 : 3;
            mix(token->blocks, token->count, token->current,
                coefficients(d, aux[10], projections * model_width), projections, selected[0]);
            if (sliding) {
                memcpy(selected[1], selected[0], sizeof selected[0]);
                memcpy(selected[2], selected[0], sizeof selected[0]);
            }
        } else {
            for (int i = 0; i < 3; ++i) {
                memcpy(selected[i], token->current, sizeof selected[i]);
            }
        }
        for (int i = 0; i < 3; ++i) {
            memcpy(scratch->inputs[i][t], selected[i], sizeof selected[i]);
            if (message && !work->device_loop) memcpy(message[i] + t * model_width, token->messages[i], sizeof selected[i]);
        }
    }
}

#include "prefill-team.h"

extern int64_t i2_parallel_prefill(uint64_t work, int64_t rows, int64_t tasks);

extern int fg_i2_prefill_task_count(int count);

static void prepare_prefill(struct i2_prefill_team *team, struct i2_prefill_work *work, int count) {
    if (team) {
        prefill_team_run(team, work, count);
        return;
    }
    const int tasks = fg_i2_prefill_task_count(count);
    if (tasks > 1) {
        (void)use_cached_history();
        assert(i2_parallel_prefill((uint64_t)(uintptr_t)work, count, tasks) == count);
    } else {
        fg_i2_prefill_partition((uint64_t)(uintptr_t)work, 0, count);
    }
}

extern void fg_i2_prefill_moe(struct decoder *d, const int64_t *core, const int64_t *aux, int slot, int count,
    float *ffn, float *projection, float *residual, struct prefill_token *tokens, int64_t *work);

extern void fg_i2_prefill_postconv(struct decoder *d, const int64_t *aux, int slot, int count, float *input, float *output);
extern void fg_i2_prefill_depthmix(struct decoder *d, const int64_t *aux, struct prefill_token *tokens,
    int count, bool save_history, bool resident, float *staging, float *residual, float *output);

extern bool fg_i2_prefill_resident_moe(struct decoder *d, const int64_t *core, const int64_t *aux,
    struct prefill_token *tokens, int slot, int begin, int layer, int pass, int count,
    bool device_loop, bool prune_host, bool retain, bool direct_depthmix, uint64_t output_slot,
    float *ffn, float *residual, float *output, float *gain);

_Static_assert(sizeof(((struct prefill_workspace *)0)->inputs[0]) == 37748736, "Goose prefill attention input stride");
extern void fg_i2_prefill_resident_attention(struct decoder *d, const int64_t *core, const int64_t *aux,
    int slot, int begin, int layer, int pass, int count, bool sliding, bool device_postconv,
    bool direct_depthmix, bool prune_host, bool device_loop, bool attention_mix, float *inputs, float **messages, float *output);

void fg_i2_prefill_layer(struct decoder *d, struct prefill_token *tokens, struct prefill_workspace *scratch,
                          int count, int begin, const int64_t *visit, struct i2_prefill_team *team,
                          bool *device_current, bool retain_current, uint64_t output_slot,
                          bool *boundary_history, bool *tail_history, bool device_loop, bool device_prelude) {
    const int layer = (int)visit[0], slot = (int)visit[1], pass = (int)visit[2];
    const bool sliding = visit[3] != 0;
    const int64_t *core = d->core + 10 * layer;
    const int64_t *aux = d->aux + 30 * layer;
    const bool resident_attention = fg_i2_serial_graph_enabled();
    const char *conv_setting = getenv("I2_PARALLEL_CUDA_POSTCONV");
    const bool device_postconv = resident_attention && count > 1 &&
        (!conv_setting || strcmp(conv_setting, "0") != 0);
    float *message[] = {scratch->control_input[0],
        scratch->control_input[0] + count * model_width, scratch->gain[0]};
    struct i2_prefill_work setup = {d, tokens, scratch, aux, layer, pass, sliding,
        resident_attention && pass > 0 ? message : NULL, false};
    const bool direct_depthmix = count == 4096 && device_postconv &&
        use_cached_history() && fg_i2_resident_moe() && prefill_direct_output;
    setup.shared_attention_input = resident_attention && share_attention_inputs(sliding, aux);
    const char *mix_setting = getenv("I2_SLIDING_DEVICE_DEPTHMIX");
    const char *full_setting = getenv("I2_FULL_DEVICE_DEPTHMIX");
    const bool mixed_path = sliding ? setup.shared_attention_input && (!mix_setting || strcmp(mix_setting, "0") != 0) :
                                      (!full_setting || strcmp(full_setting, "0") != 0);
    const bool device_attention_mix = direct_depthmix && layer != 0 && aux[10] >= 0 && mixed_path;
    setup.device_attention_mix = device_attention_mix;
    setup.device_current = *device_current;
    setup.device_loop = device_loop;
    setup.device_prelude = device_prelude;
    const bool device_boundary = *device_current && (layer == 12 || layer == 18 || (layer == 6 && device_prelude));
    setup.device_boundary = device_boundary;
    assert(!*device_current || (device_attention_mix && (layer % 6 != 0 || device_boundary)));
    if (device_boundary) {
        if (layer != 6) assert(tokens[0].count == layer / 6);
        if (layer == 12) *boundary_history = true;
        else if (layer == 18) *tail_history = true;
        fprintf(stderr, "Inkling resident block history begin=%d layer=%d pass=%d\n", begin, layer, pass);
    }
    retain_current = retain_current && direct_depthmix && (!mix_setting || strcmp(mix_setting, "0") != 0);
    const char *prune = getenv("I2_PRUNE_PREFILL_HOST_STAGING");
    const bool prune_host = !getenv("I2_CHECK_STATE") && (!prune || strcmp(prune, "0") != 0);
    setup.poison_host = !prune_host;
    const bool skip_attention_prep = prune_host && device_attention_mix && *device_current &&
        layer != 6 && (!setup.message || device_loop);
    if (!skip_attention_prep) prepare_prefill(team, &setup, count);
    if (device_attention_mix) {
        float *input = scratch->inputs[0][0];
        const size_t values = (size_t)count * model_width;
        const int queries = sliding ? 1 : 3;
        const uint64_t full_outputs = sliding ? 0 : inkling_prefill_full_scratch(queries * values * sizeof(float));
        assert(sliding || full_outputs);
        assert(inkling_arena_depthmix(*device_current ? NULL : input, NULL,
                                    coefficients(d, aux[10], queries * model_width).data,
                                    input, tokens[0].count, device_boundary, full_outputs));
        if (!prune_host)
            for (int q = 0; q < queries; ++q)
                for (int t = 0; t < count; ++t)
                    for (int c = 0; c < model_width; ++c) scratch->inputs[q][t][c] = NAN;
    }
    struct ggml_context *ctx;
    int64_t work[16] = {0};
    if (resident_attention) {
        fg_i2_prefill_resident_attention(d, core, aux, slot, begin, layer, pass, count, sliding,
            device_postconv, direct_depthmix, prune_host, device_loop, device_attention_mix,
            scratch->inputs[0][0], message, device_postconv ? scratch->residual[0] : scratch->linear[0]);
    } else {
        for (int i = 0; i < 3; ++i) {
            if (pass > 0) {
                prefill_control(d, aux, i, count, tokens, scratch->inputs[i], scratch);
            }
            for (int t = 0; t < count; ++t) {
                float *row = scratch->inputs[i][t];
                assert(i2_rms_gain(vec(row, model_width), coefficients(d, aux[sliding ? 11 : 12 + i], model_width),
                    1e-6f, vec(row, model_width)) == model_width);
            }
        }

        if (fg_i2_device_attention_input()) {
            native_attention_resident(d, core, aux, slot, begin, count, sliding,
                scratch->inputs[0][0], scratch->inputs[1][0], scratch->inputs[2][0], scratch->linear[0]);
        } else {
            float *rows[] = {scratch->inputs[0][0], scratch->inputs[1][0], scratch->inputs[2][0]};
            const int packed = fg_i2_project_attention_inputs(d, core, aux, sliding, rows, count, scratch->projections, work);
            struct native_attention_inputs args;
            fg_i2_native_workspace(d, count, &args);
            fg_i2_prepare_prefill_rows(d, &args, aux, slot, begin, count, sliding, scratch->projections, packed);
            if (fg_i2_resident_attention_projection()) {
                fg_i2_native_project(d, &args, aux, slot, begin, count, sliding, core[3], scratch->linear[0]);
            } else {
                fg_i2_native_submit(d, &args, aux, slot, begin, count, sliding, scratch->attention[0]);
                prefill_projection(d, core[3], scratch->attention[0], attn_width, scratch->linear[0], model_width, count);
            }
        }
    }
    const char *postconv_batch = getenv("I2_PREFILL_POSTCONV_BATCH");
    if (!device_postconv && team && (!postconv_batch || strcmp(postconv_batch, "0") != 0)) {
        setup.postconv_weights = coefficients(d, aux[2], 4 * model_width);
        setup.postconv_history = d->ha + slot * 3 * model_width;
        setup.postconv_rows = count;
        prefill_team_run(team, &setup, model_width);
        setup.postconv_rows = 0;
    } else if (!device_postconv) {
        fg_i2_prefill_postconv(d, aux, slot, count, scratch->linear[0], scratch->residual[0]);
    }
    setup.after_attention = true;
    setup.message = NULL;
    setup.skip_host_mix = direct_depthmix;
    const bool skip_moe_prep = prune_host && direct_depthmix && layer % 6 != 0;
    if (!skip_moe_prep) prepare_prefill(team, &setup, count);
    if (skip_attention_prep || skip_moe_prep)
        fprintf(stderr, "Inkling pruned host prep begin=%d layer=%d pass=%d attention=%d moe=%d\n",
                begin, layer, pass, skip_attention_prep, skip_moe_prep);
    if (begin == 4096 && count > 1 && slot == 0 && getenv("I2_DRAFT_PARITY")) {
        check_state("draft-postconv", count, scratch->residual[1], model_width);
        check_state("draft-moe-input", count, scratch->ff[1], model_width);
    }
    if (direct_depthmix) {
        fg_i2_prefill_depthmix(d, aux, tokens, count, layer % 6 == 0, *device_current,
            scratch->inputs[0][0], scratch->residual[0], scratch->ff[0]);
    }
    if (fg_i2_resident_moe()) {
        *device_current = fg_i2_prefill_resident_moe(d, core, aux, tokens, slot, begin, layer, pass, count,
            device_loop, prune_host, retain_current, direct_depthmix, output_slot,
            scratch->ff[0], scratch->residual[0], scratch->linear[0], scratch->gain[0]);
        return;
    }
    if (pass > 0) {
        prefill_control(d, aux, 3, count, tokens, scratch->ff, scratch);
    }
    fg_i2_prefill_moe(d, core, aux, slot, count, scratch->ff[0],
        scratch->linear[0], scratch->residual[0], tokens, work);
}

struct i2_prefill_visits {
    struct decoder *decoder;
    struct prefill_token *tokens;
    struct prefill_workspace *scratch;
    struct i2_prefill_team *workers;
    int count, begin;
};

_Static_assert(offsetof(struct i2_prefill_visits, tokens) == 8 &&
    offsetof(struct i2_prefill_visits, scratch) == 16 && offsetof(struct i2_prefill_visits, count) == 32 &&
    offsetof(struct i2_prefill_visits, begin) == 36 && offsetof(struct prefill_workspace, inputs) == 0,
    "Goose prefill visit ABI");

void fg_i2_prefill_trace_visit(int visit, const float *current) {
    check_state("draft-batch-visit", visit, current, model_width);
}

_Static_assert(sizeof(struct prefill_workspace) == 646316040 &&
    offsetof(struct prefill_workspace, linear) == 343277568 &&
    offsetof(struct prefill_workspace, linear_storage) == 343277576 &&
    offsetof(struct prefill_token, current) == 0 && sizeof(struct i2_prefill_team) <= 296 &&
    _Alignof(struct i2_prefill_team) <= 8, "Goose prefill driver ABI");

bool fg_i2_prefill_team_start(struct i2_prefill_team *team, int count) {
    const int tasks = fg_i2_prefill_task_count(count);
    if (tasks <= 1) return false;
    (void)use_cached_history();
    return prefill_team_open(team, tasks, true);
}

void fg_i2_prefill_set_readback(float *output, size_t elements) {
    prefill_direct_output = output;
    prefill_direct_output_size = elements;
}

void fg_i2_prefill_check_residual(int position, const float *current) {
    check_state("residual", position, current, model_width);
}
