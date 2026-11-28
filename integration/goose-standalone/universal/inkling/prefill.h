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

extern void fg_i2_prefill_partition(uint64_t address, int64_t begin, int64_t end);

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
    offsetof(struct i2_prefill_work, postconv_rows) == 88 && offsetof(struct i2_prefill_work, logits) == 96 &&
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
