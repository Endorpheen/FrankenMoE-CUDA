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
    struct prefill_token *tokens, float *current, struct prefill_workspace *scratch);
_Static_assert(offsetof(struct prefill_workspace, projections) == 113246208 &&
    offsetof(struct prefill_workspace, attention) == 255197184 &&
    offsetof(struct prefill_workspace, control_input) == 418775048 &&
    offsetof(struct prefill_workspace, control_hidden) == 494272520 &&
    offsetof(struct prefill_workspace, gain) == 533069832 && offsetof(struct prefill_workspace, forget) == 495321096 &&
    offsetof(struct prefill_workspace, combined) == 570818568, "Goose prefill scratch ABI");

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

_Static_assert(sizeof(struct i2_prefill_work) == 112 &&
    offsetof(struct prefill_workspace, ff) == 305528832 && offsetof(struct prefill_workspace, residual) == 381026312 &&
    sizeof(((struct prefill_workspace *)0)->inputs[0]) == 37748736 &&
    offsetof(struct i2_prefill_work, decoder) == 0 &&
    offsetof(struct i2_prefill_work, tokens) == 8 &&
    offsetof(struct i2_prefill_work, scratch) == 16 &&
    offsetof(struct i2_prefill_work, aux) == 24 &&
    offsetof(struct i2_prefill_work, layer) == 32 &&
    offsetof(struct i2_prefill_work, pass) == 36 &&
    offsetof(struct i2_prefill_work, sliding) == 40 &&
    offsetof(struct i2_prefill_work, message) == 48 &&
    offsetof(struct i2_prefill_work, after_attention) == 56 &&
    offsetof(struct i2_prefill_work, shared_attention_input) == 57 &&
    offsetof(struct i2_prefill_work, postconv_weights) == 58 &&
    offsetof(struct i2_prefill_work, postconv_history) == 80 &&
    offsetof(struct i2_prefill_work, postconv_rows) == 88 &&
    offsetof(struct i2_prefill_work, skip_host_mix) == 104 &&
    offsetof(struct i2_prefill_work, device_attention_mix) == 105 &&
    offsetof(struct i2_prefill_work, device_current) == 106 &&
    offsetof(struct i2_prefill_work, device_boundary) == 107 &&
    offsetof(struct i2_prefill_work, device_loop) == 108 &&
    offsetof(struct i2_prefill_work, device_prelude) == 109 &&
    offsetof(struct i2_prefill_work, poison_host) == 110, "Goose prefill layer ABI");

void fg_i2_prefill_prepare(struct i2_prefill_team *team, struct i2_prefill_work *work, int count) {
    prepare_prefill(team, work, count);
}

void fg_i2_prefill_postconv_team(struct i2_prefill_team *team, struct i2_prefill_work *work) {
    prefill_team_run(team, work, model_width);
}

void fg_i2_prefill_trace_work(int count, const float *residual, const float *moe) {
    check_state("draft-postconv", count, residual, model_width);
    check_state("draft-moe-input", count, moe, model_width);
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
