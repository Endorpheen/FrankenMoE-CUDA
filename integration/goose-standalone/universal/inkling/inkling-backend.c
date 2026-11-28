#include <inttypes.h>
#include "main.h"
#ifdef I2_NUMERIC_MODULE
void i2_candidate_init(int argc, char **argv);
int64_t inkling_row_major_i2_situ(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, float gs_export_arg2, float gs_export_arg3, sl_f32 gs_export_arg4);
#define i2_situ inkling_row_major_i2_situ
int64_t inkling_row_major_i2_relative_positions(int64_t gs_export_arg0, sl_i64 gs_export_arg1, int64_t gs_export_arg2, sl_i32 gs_export_arg3);
#define i2_relative_positions inkling_row_major_i2_relative_positions
int64_t inkling_row_major_i2_shortconv_decode(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4);
#define i2_shortconv_decode inkling_row_major_i2_shortconv_decode
int64_t inkling_row_major_i2_attention_decode(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5, sl_i64 gs_export_arg6, int64_t gs_export_arg7, int64_t gs_export_arg8, sl_f32 gs_export_arg9, sl_f32 gs_export_arg10);
#define i2_attention_decode inkling_row_major_i2_attention_decode
int64_t inkling_row_major_i2_cache_layout(sl_i64 gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2, int64_t gs_export_arg3, int64_t gs_export_arg4, int64_t gs_export_arg5, int64_t gs_export_arg6, sl_i64 gs_export_arg7);
#define i2_cache_layout inkling_row_major_i2_cache_layout
int64_t inkling_row_major_i2_cache_row(int64_t gs_export_arg0, int64_t gs_export_arg1, int64_t gs_export_arg2, uint8_t gs_export_arg3);
#define i2_cache_row inkling_row_major_i2_cache_row
void inkling_row_major_i2_reset_cache_positions(sl_i64 gs_export_arg0);
#define i2_reset_cache_positions inkling_row_major_i2_reset_cache_positions
int64_t inkling_row_major_i2_store_kv(int64_t gs_export_arg0, int64_t gs_export_arg1, uint8_t gs_export_arg2, sl_i64 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5, sl_f32 gs_export_arg6, sl_f32 gs_export_arg7);
#define i2_store_kv inkling_row_major_i2_store_kv
int64_t inkling_row_major_i2_depth_mix(sl_f32 gs_export_arg0, int64_t gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5);
int64_t inkling_row_major_i2_loop_blend(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4);
int64_t inkling_row_major_i2_attention_token(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3, sl_f32 gs_export_arg4, sl_f32 gs_export_arg5, int64_t gs_export_arg6, int64_t gs_export_arg7, uint8_t gs_export_arg8, sl_i64 gs_export_arg9, sl_f32 gs_export_arg10, sl_f32 gs_export_arg11, sl_f32 gs_export_arg12, sl_f32 gs_export_arg13, sl_f32 gs_export_arg14, sl_f32 gs_export_arg15);
int64_t inkling_row_major_i2_rms_gain(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, float gs_export_arg2, sl_f32 gs_export_arg3);
int64_t inkling_row_major_i2_postconv_token(sl_f32 gs_export_arg0, sl_f32 gs_export_arg1, sl_f32 gs_export_arg2, sl_f32 gs_export_arg3);
int64_t inkling_row_major_i2_text_logits(sl_f32 gs_export_arg0, int64_t gs_export_arg1, float gs_export_arg2);
#define I2_CALL(name) inkling_row_major_ ## name
#else
#define I2_CALL(name) name
#endif

#include "graph-execute.h"
#include "ggml.h"

extern int64_t fg143_graph_nodes(uint64_t graph);
#include "native-kv.h"
#include "moe-epilogue.h"
#include "situ.h"
#include "bridge.h"
#include "inkling-backend.h"

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void check_state(const char *name, int token, const float *values, size_t length) {
    if (!getenv("I2_CHECK_STATE")) return;
    const unsigned char *bytes = (const unsigned char *)values;
    uint64_t value = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < length * sizeof(*values); ++i) {
        value = (value ^ bytes[i]) * UINT64_C(1099511628211);
    }
    fprintf(stderr, "I2 state %s token %d %016llx\n", name, token, (unsigned long long)value);
}

static double now(void);
static _Thread_local double call_seconds[9];
static _Thread_local unsigned call_count[9];

#define MEASURE(index, expression) ({ \
    const double start = now(); \
    __auto_type result = (expression); \
    call_seconds[index] += now() - start; \
    ++call_count[index]; \
    result; \
})
#define i2_attention_token(...) MEASURE(0, I2_CALL(i2_attention_token)(__VA_ARGS__))
#define i2_depth_mix(...) MEASURE(1, I2_CALL(i2_depth_mix)(__VA_ARGS__))
#define i2_rms_gain(...) MEASURE(2, I2_CALL(i2_rms_gain)(__VA_ARGS__))
#define i2_loop_blend(...) MEASURE(3, I2_CALL(i2_loop_blend)(__VA_ARGS__))
#define i2_postconv_token(...) MEASURE(4, I2_CALL(i2_postconv_token)(__VA_ARGS__))
#define i2_text_logits(...) MEASURE(5, I2_CALL(i2_text_logits)(__VA_ARGS__))
#define i2_attention_projections(...) MEASURE(6, i2_attention_projections(__VA_ARGS__))
#define i2_moe_ffn(...) MEASURE(7, i2_moe_ffn(__VA_ARGS__))
#define fg_standalone_graph_compute(...) MEASURE(8, fg_standalone_graph_compute(__VA_ARGS__))

extern void fg_cuda_enable_inkling_quants(bool enabled);
extern void fg_cuda_inkling_expert_phase(bool prefill, int position, int count);
extern void i2_cuda_attention_stats(void);
extern int64_t fg_cuda_free_bytes(void);

enum { model_width=2304, ff=1152, experts=128, active=8 };
typedef struct {
    const uint8_t *data;
    int64_t kind;
    int64_t shape[4];
} weight;

static weight bind(const uint8_t *file, const int64_t *tensors, int64_t index) {
    assert(index >= 0 && index < 704);
    const int64_t *record=tensors+12*index;
    weight w={ .data=file+record[10], .kind=record[2] };
    memcpy(w.shape,record+4,sizeof w.shape);
    return w;
}

static struct ggml_tensor * load(struct ggml_context * ctx, weight w) {
    struct ggml_tensor *t=ggml_new_tensor(ctx,(enum ggml_type)w.kind,4,w.shape);
    assert(t);
    t->data=(void *)(uintptr_t) w.data;
    return t;
}

static sl_f32 float_weights(weight w, int64_t count) {
    assert(w.kind == GGML_TYPE_F32);
    assert(w.shape[0]*w.shape[1]*w.shape[2]*w.shape[3]==count);
    return (sl_f32){(float *)(uintptr_t)w.data,count};
}

static bool uses_resident_plan(void) {
    static int resident_plan = -1;
    if (resident_plan < 0) {
        const char *config = getenv("I2_RESIDENT_PLAN");
        resident_plan = config && strcmp(config, "1") == 0;
    }
    return resident_plan != 0;
}

static _Thread_local float *prefill_direct_output;
static _Thread_local size_t prefill_direct_output_size;
extern void *fg_cuda_inkling_prefill_readback(size_t bytes);
extern void fg_cuda_i2_prefill_conv(const float *weights, float *history);
extern void inkling_arena_window(bool active, int begin);
extern bool inkling_arena_depthmix(const float *past, const float *next, const float *query, float *output, int rows, bool save_history, uint64_t full_outputs);
extern unsigned inkling_arena_depthmix_close(void);
extern unsigned inkling_arena_depthmix_reads(void);
extern void inkling_arena_residual_open(float *host);
extern void inkling_moe_retain_output(float *host, uint64_t device);
extern uint64_t inkling_prefill_full_scratch(size_t bytes);
extern bool inkling_arena_boundary_read(float *output, int index, bool last_only);
extern uint64_t inkling_loop_storage(void);
extern bool inkling_loop_save(uint64_t storage, const float *previous);
extern void inkling_loop_bind(const float *queries, const float *q, const float *k,
                              const float *v, const float *ffn);
extern unsigned inkling_loop_generated(void);
extern void inkling_loop_release(void);
extern void inkling_prelude_clear(void);
extern bool inkling_prelude_use(bool capture);
extern bool inkling_prefill_final_rows(float *current, float *prelude);

static void evaluate(struct ggml_context *ctx, struct ggml_tensor **nodes, int count,
                     float *output, int size) {
    if (output == prefill_direct_output && count == 1 && nodes[0] &&
        !nodes[0]->data && !nodes[0]->view_src && nodes[0]->type == GGML_TYPE_F32 &&
        (size_t)size == prefill_direct_output_size && ggml_nelements(nodes[0]) == size) {
        nodes[0]->data = output;
    }
    struct ggml_cgraph *graph=ggml_new_graph_custom(ctx,256,false);
    assert(graph);
    for(int i=0;i<count;i++)ggml_build_forward_expand(graph,nodes[i]);
    void *storage = NULL;
    struct fg_standalone_graph_plan *plan = NULL;
    if (uses_resident_plan() && count == 1 && fg143_graph_nodes((uint64_t)(uintptr_t)graph) > 2) {
        plan = fg_standalone_graph_plan_alloc(graph, nodes[0]);
        assert(plan && MEASURE(8, fg_standalone_graph_plan_compute(plan)) == GGML_STATUS_SUCCESS);
    } else {
        storage = fg_standalone_graph_alloc(graph);
        assert(storage && fg_standalone_graph_compute(graph) == GGML_STATUS_SUCCESS);
    }
    int at=0;
    for(int i=0;i<count;i++) {
        assert(nodes[i]);
        int64_t n=ggml_nelements(nodes[i]);
        assert(at+n<=size);
        if (output + at != nodes[i]->data)
            memcpy(output + at, nodes[i]->data, (size_t)n * sizeof(float));
        at+=(int)n;
    }
    assert(at==size);
    fg_standalone_graph_plan_free(plan);
    free(storage);
}


// Test driver for the shared Goose graph executor. Each component's
// floating-point operations and expert decisions are in the runtime.
enum { physical=24, logical=INKLING_VISITS, key_width=1024, attn_width=3072,
       vocabulary=INKLING_TEXT_VOCAB, out_width=201024 };

struct inkling_decode_graph {
    struct ggml_context *context;
    struct fg_standalone_graph_plan *plan;
    struct ggml_tensor *output, *inputs[3], *messages[3], *residual;
    struct inkling_attention_graph_call attention;
    struct inkling_bridge bridge;
    bool bridge_started;
};

struct i2_prefill_team;

enum { i2_draft_max = 16 };

struct i2_draft_state {
    int32_t *history;
    float *scores, *saved_history;
    int32_t tokens[i2_draft_max];
    int begin, count, used;
    unsigned groups, accepted, rejects;
};

struct decoder {
    const uint8_t *data;
    int64_t tensor[768*12], globals[5], core[24*10], aux[24*30], visits[48*4];
    int capacity, position;
    enum inkling_kv_format format;
    float logits[out_width];
    size_t flash_floats;
    float *flash_workspace, *kh, *vh, *ha, *hf;
    bool retain_decode_graphs;
    struct i2_draft_state draft;
    struct i2_prefill_team *logits_team;
    struct inkling_decode_graph decode_attention[logical], decode_moe[logical], decode_joined[logical];
};

static sl_f32 vec(float *p,int64_t n) { return (sl_f32){p,n}; }
static sl_i64 ints(int64_t *p,int64_t n) { return (sl_i64){p,n}; }
static weight named(struct decoder *d,int64_t id) { return bind(d->data,d->tensor,id); }
static sl_f32 coefficients(struct decoder *d,int64_t id,int64_t n) {
    return float_weights(named(d,id),n);
}

#include "native-host.h"

static struct ggml_tensor *input_tensor(struct ggml_context *ctx,float *values,int n) {
    struct ggml_tensor *t=ggml_new_tensor_2d(ctx,GGML_TYPE_F32,n,1);
    assert(t);
    t->data=values;
    return t;
}

static void execute_decode_graph(struct inkling_decode_graph *entry, float *output) {
    if (!entry->plan) {
        struct ggml_cgraph *graph = ggml_new_graph_custom(entry->context, 256, false);
        assert(graph);
        ggml_build_forward_expand(graph, entry->output);
        entry->plan = fg_standalone_graph_plan_alloc(graph, entry->output);
        assert(entry->plan);
    }
    assert(MEASURE(8, fg_standalone_graph_plan_compute(entry->plan)) == GGML_STATUS_SUCCESS);
    memcpy(output, entry->output->data, model_width * sizeof(*output));
}

static struct ggml_context *context(void) {
    struct ggml_context *ctx=ggml_init((struct ggml_init_params){2*1024*1024,NULL,true});
    assert(ctx);
    return ctx;
}

static void project(struct decoder *d,int64_t id,float *input,int n,float *output,int rows) {
    struct ggml_context *ctx=context();
    struct ggml_tensor *w=load(ctx,named(d,id)), *x=input_tensor(ctx,input,n);
    struct ggml_tensor *result=ggml_mul_mat(ctx,w,x);
    assert(result);
    struct ggml_tensor *nodes[]={result};
    evaluate(ctx,nodes,1,output,rows);
    ggml_free(ctx);
}

extern bool fg_i2_host_cached_history(void);
extern void fg_i2_host_save_history(float *row, float *current);
extern void fg_i2_host_mix(float *history, int count, float *current, sl_f32 queries, int queries_count, float *out);
extern void fg_i2_host_control_group(struct decoder *, const int64_t *, int, float *, int, float *);
extern struct ggml_tensor *fg_i2_host_rows_input(struct ggml_context *, float *, int, int);
extern struct ggml_tensor *fg_i2_host_controller_node(struct ggml_context *, struct decoder *,
    const int64_t *, int, struct ggml_tensor *, struct ggml_tensor *, struct ggml_tensor *);
extern void fg_i2_host_control_resident(struct decoder *, const int64_t *, int, float *, float *, int);
extern void fg_i2_host_control(struct decoder *, const int64_t *, int, float *, float *);

#define use_cached_history fg_i2_host_cached_history
#define save_history_row fg_i2_host_save_history
#define mix fg_i2_host_mix
#define project_control_group fg_i2_host_control_group
#define rows_input fg_i2_host_rows_input
#define controller_node fg_i2_host_controller_node
#define control_resident fg_i2_host_control_resident
#define control fg_i2_host_control

static bool use_resident_controller(void) {
    const char *setting = getenv("I2_RESIDENT_CONTROL");
    return uses_resident_plan() && (!setting || strcmp(setting, "0") != 0);
}

bool fg_i2_host_resident_plan(void) { return uses_resident_plan(); }
bool fg_i2_host_resident_control(void) { return use_resident_controller(); }
struct ggml_context *fg_i2_host_context(void) { return context(); }
struct ggml_tensor *fg_i2_host_weight(struct ggml_context *ctx, struct decoder *d, int64_t id) {
    return load(ctx, named(d, id));
}
void fg_i2_host_evaluate_one(struct ggml_context *ctx, struct ggml_tensor *tensor, float *out, int count) {
    evaluate(ctx, &tensor, 1, out, count);
}
void fg_i2_host_project(struct decoder *d, int64_t id, float *input, int n, float *out, int rows) {
    project(d, id, input, n, out, rows);
}
int64_t fg_i2_host_measured_rms(sl_f32 input, sl_f32 weight, float eps, sl_f32 out) {
    return i2_rms_gain(input, weight, eps, out);
}
int64_t fg_i2_host_measured_depth(sl_f32 states, int64_t width, sl_f32 query, sl_f32 norm, sl_f32 prob, sl_f32 out) {
    return i2_depth_mix(states, width, query, norm, prob, out);
}
int64_t fg_i2_host_measured_blend(sl_f32 current, sl_f32 message, sl_f32 gain, sl_f32 forget, sl_f32 out) {
    return i2_loop_blend(current, message, gain, forget, out);
}

_Static_assert(offsetof(struct ggml_tensor, data) == 248 && GGML_TYPE_F32 == 0 &&
    GGML_UNARY_OP_TANH == 4, "Goose Inkling control graph ABI");

static bool use_resident_moe(void) {
    const char *setting = getenv("I2_RESIDENT_MOE");
    return uses_resident_plan() && (!setting || strcmp(setting, "0") != 0);
}

extern struct ggml_tensor *fg_i2_host_moe_node(struct ggml_context *, struct decoder *,
    const int64_t *, const int64_t *, int, int, int, struct ggml_tensor *, float *,
    struct ggml_tensor *, struct inkling_decode_graph *);
extern void fg_i2_host_moe_resident(struct decoder *, const int64_t *, const int64_t *,
    int, int, int, float *, float *, float *, float *);
extern bool fg_i2_host_share_attention(bool, const int64_t *);
extern struct ggml_tensor *fg_i2_host_attention_node(struct ggml_context *, struct decoder *,
    const int64_t *, int, bool, float **, float **, const struct inkling_attention_graph_call *,
    struct inkling_decode_graph *);
extern void fg_i2_host_attention_graph(struct decoder *, const int64_t *, const int64_t *,
    int, int, int, bool, float **, float **, float *);

#define moe_node fg_i2_host_moe_node
#define moe_resident fg_i2_host_moe_resident
#define share_attention_inputs fg_i2_host_share_attention
#define attention_node fg_i2_host_attention_node
#define attention_graph fg_i2_host_attention_graph

int64_t fg_i2_host_measured_moe(int64_t ctx, int64_t input, sl_i64 weights, sl_i64 work, sl_i64 nodes) {
    return i2_moe_ffn(ctx, input, weights, work, nodes);
}

void fg_i2_host_execute_entry(struct inkling_decode_graph *entry, float *out) {
    execute_decode_graph(entry, out);
}

void fg_i2_measure_native_attention(const struct inkling_attention_batch *batch,
    const struct inkling_attention_input_projections *inputs, const struct inkling_attention_projection *output) {
    const double start = now();
    if (inputs) {
        assert(inkling_attention_execute_resident(batch, inputs, output) == 0);
    } else if (output) {
        assert(inkling_attention_execute_project(batch, output) == 0);
    } else {
        assert(inkling_attention_execute(batch) == 0);
    }
    call_seconds[0] += now() - start;
    ++call_count[0];
}

_Static_assert(offsetof(struct decoder, decode_attention) == 887280 &&
    offsetof(struct decoder, decode_moe) == 904944 && sizeof(struct inkling_decode_graph) == 368 &&
    offsetof(struct inkling_decode_graph, plan) == 8 && offsetof(struct inkling_decode_graph, output) == 16 &&
    offsetof(struct inkling_decode_graph, inputs) == 24 && offsetof(struct inkling_decode_graph, messages) == 48 &&
    offsetof(struct inkling_decode_graph, residual) == 72 && offsetof(struct inkling_decode_graph, attention) == 80 &&
    sizeof(struct inkling_attention_graph_call) == 232 && offsetof(struct inkling_attention_graph_call, batch.first) == 8 &&
    offsetof(struct ggml_tensor, op) == 80 && offsetof(struct ggml_tensor, op_params) == 84 &&
    offsetof(struct ggml_tensor, src) == 152 && GGML_MAX_OP_PARAMS == 64 && GGML_MAX_SRC == 10 &&
    GGML_OP_CUSTOM == 95 && INKLING_MOE_SITU == 0x49325354 && INKLING_MOE_EPILOGUE == 0x49324d45 &&
    INKLING_ATTENTION_GRAPH == 0x49324147, "Goose Inkling graph node ABI");

static bool use_attention_graph(void) {
    const char *setting = getenv("I2_ATTENTION_GRAPH");
    return use_device_attention_input() && uses_resident_plan() &&
        (!setting || strcmp(setting, "0") != 0);
}

static bool use_joined_bridge(void) {
    const char *setting = getenv("I2_JOIN_ATTENTION_MOE");
    return setting && strcmp(setting, "1") == 0;
}

_Static_assert(offsetof(struct inkling_decode_graph, bridge) == 312 && sizeof(struct inkling_bridge) == 48 &&
    offsetof(struct inkling_bridge, postconv) == 0 && offsetof(struct inkling_bridge, query) == 8 &&
    offsetof(struct inkling_bridge, initial_history) == 16 && offsetof(struct inkling_bridge, input) == 24 &&
    offsetof(struct inkling_bridge, slot) == 32 && offsetof(struct inkling_bridge, rows) == 36 &&
    offsetof(struct inkling_bridge, position) == 40 && offsetof(struct inkling_bridge, restart) == 44 &&
    offsetof(struct inkling_bridge, add_residual) == 45 && INKLING_BRIDGE_GRAPH == 0x49324252 &&
    INKLING_BRIDGE_ROWS == 11, "Goose Inkling joined bridge ABI");

int64_t fg_i2_measured_postconv(sl_f32 input, sl_f32 weight, sl_f32 history, sl_f32 output) {
    return i2_postconv_token(input, weight, history, output);
}

_Static_assert(offsetof(struct decoder, hf) == 887144, "Goose serial MoE history ABI");
bool fg_i2_serial_graph_enabled(void) { return use_attention_graph(); }

extern int fg_i2_project_attention_inputs(struct decoder *d, const int64_t *core, const int64_t *aux,
    bool sliding, float **rows, int count, float *projected, int64_t *work);

int64_t fg_i2_measured_attention_projections(int64_t ctx, sl_i64 input, sl_i64 weights, bool sliding,
    sl_i64 work, sl_i64 output) {
    return i2_attention_projections(ctx, input, weights, sliding, work, output);
}

void fg_i2_evaluate_attention_inputs(struct ggml_context *ctx, int64_t *output, int matrices, float *projected, int count) {
    struct ggml_tensor *roots[8];
    for (int index = 0; index < matrices; ++index) roots[index] = (struct ggml_tensor *)(intptr_t)output[index];
    evaluate(ctx, roots, matrices, projected, count);
}

void fg_i2_serial_native_attention(struct decoder *d, const int64_t *core, const int64_t *aux,
    int slot, int absolute, bool sliding, float (*inputs)[model_width], float *proj, int64_t *work) {
    if (use_device_attention_input()) {
        native_attention_resident(d, core, aux, slot, absolute, 1, sliding, inputs[0], inputs[1], inputs[2], proj);
    } else {
        float *rows[] = {inputs[0], inputs[1], inputs[2]};
        float projected[8664], attn[attn_width];
        int packed = fg_i2_project_attention_inputs(d, core, aux, sliding, rows, 1, projected, work);

        struct native_attention_inputs args;
        fg_i2_native_workspace(d, 1, &args);
        fg_i2_prepare_native_row(d, &args, aux, slot, absolute, 0, sliding, projected, packed);
        if (use_resident_attention_projection()) {
            fg_i2_native_project(d, &args, aux, slot, absolute, 1, sliding, core[3], proj);
        } else {
            fg_i2_native_submit(d, &args, aux, slot, absolute, 1, sliding, attn);
            project(d, core[3], attn, attn_width, proj, model_width);
        }
    }
}

static bool parallel_decode_logits(struct decoder *d, float *logits);

extern void fg_i2_read_token_logits(struct decoder *d, int absolute, float *current, float *history, int count, bool decode);

bool fg_i2_parallel_logits(struct decoder *d, float *logits) { return parallel_decode_logits(d, logits); }

void fg_i2_softcap_logits(sl_f32 logits) {
    assert(i2_text_logits(logits, vocabulary, 30.0f) == vocabulary);
}

void fg_i2_validate_logits(int absolute, const float *logits, bool handled) {
    check_state("logits", absolute, logits, out_width);
    if (!handled) for (int i = 0; i < vocabulary; ++i) assert(isfinite(logits[i]));
}

void i2_serial_trace(int32_t visit, sl_f32 current) {
    check_state("draft-serial-visit", visit, current.data, (size_t)current.len);
}

_Static_assert(offsetof(struct decoder, visits) == 81456 && logical == 48 && model_width == 2304,
    "Goose serial token ABI");

_Static_assert(GGML_TYPE_Q8_0 == 8 && offsetof(struct decoder, data) == 0, "Goose embedding ABI");
extern bool fg_i2_embed_prefill(struct decoder *d, const int32_t *ids, int count, void *output, size_t stride);

void fg_i2_serial_finish(struct decoder *d, int absolute, float *current, float *history, int count, bool logits) {
    check_state("residual", absolute, current, model_width);
    if (logits) fg_i2_read_token_logits(d, absolute, current, history, count, true);
}

extern void fg_i2_prepare_prefill_rows(struct decoder *d, const struct native_attention_inputs *args,
    const int64_t *aux, int slot, int begin, int count, bool sliding, const float *projections, int packed);

#include "prefill.h"

extern int64_t fg_standalone_cpu_decode_threads(void);

static bool parallel_decode_logits(struct decoder *d, float *logits) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *setting = getenv("I2_PARALLEL_LOGITS");
        enabled = setting && strcmp(setting, "1") == 0;
    }
    if (!enabled) return false;
    if (!d->logits_team) {
        const int64_t configured = fg_standalone_cpu_decode_threads();
        const int tasks = configured > 4 ? 4 : configured;
        if (tasks < 2) return false;
        struct i2_prefill_team *team = calloc(1, sizeof(*team));
        if (!team) return false;
        if (!prefill_team_open(team, tasks, false)) {
            free(team);
            return false;
        }
        d->logits_team = team;
    }
    const struct i2_prefill_work work = {.logits = logits};
    prefill_team_run(d->logits_team, &work, vocabulary);
    return true;
}


static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec+(double)ts.tv_nsec*1e-9;
}


_Static_assert(sizeof(struct decoder) == 940272 && offsetof(struct decoder, data) == 0 &&
    offsetof(struct decoder, tensor) == 8 && offsetof(struct decoder, globals) == 73736 &&
    offsetof(struct decoder, core) == 73776 && offsetof(struct decoder, aux) == 75696 &&
    offsetof(struct decoder, visits) == 81456 && offsetof(struct decoder, flash_workspace) == 887112 &&
    offsetof(struct decoder, kh) == 887120 && offsetof(struct decoder, vh) == 887128 &&
    offsetof(struct decoder, hf) == 887144 && INKLING_CONTEXT_LIMIT == 1048576 &&
    INKLING_WINDOW == 1024, "Goose Inkling open ABI");

_Static_assert(offsetof(struct decoder, logits_team) == 887272, "Goose Inkling cleanup ABI");

void fg_i2_close_logits_team(struct i2_prefill_team *team) {
    prefill_team_close(team);
}

_Static_assert(offsetof(struct decoder, capacity) == 82992 &&
    offsetof(struct decoder, position) == 82996 &&
    offsetof(struct decoder, format) == 83000 &&
    offsetof(struct decoder, retain_decode_graphs) == 887152 &&
    offsetof(struct decoder, ha) == 887136 &&
    offsetof(struct decoder, decode_joined) == 922608 &&
    offsetof(struct decoder, logits) == 83004 &&
    offsetof(struct decoder, draft) == 887160 &&
    offsetof(struct i2_draft_state, history) == 0 &&
    offsetof(struct i2_draft_state, scores) == 8 &&
    offsetof(struct i2_draft_state, saved_history) == 16 &&
    offsetof(struct i2_draft_state, tokens) == 24 &&
    offsetof(struct i2_draft_state, begin) == 88 &&
    offsetof(struct i2_draft_state, count) == 92 &&
    offsetof(struct i2_draft_state, used) == 96 &&
    offsetof(struct i2_draft_state, groups) == 100 &&
    offsetof(struct i2_draft_state, accepted) == 104 &&
    offsetof(struct i2_draft_state, rejects) == 108 &&
    offsetof(struct inkling_decode_graph, bridge_started) == 360 &&
    sizeof(struct inkling_decode_graph) == 368 &&
    logical == 48 && model_width == 2304 && max_prefill_batch == 4096 &&
    i2_draft_max == 16 && vocabulary == 200058 && INKLING_KV_Q4_0 == 1, "Goose Inkling draft ABI");

bool fg_i2_backend_draft_features(void) {
    return use_attention_graph() && use_cached_history() && use_resident_moe() && use_joined_bridge();
}

void fg_i2_backend_prefill(uint64_t address, uint64_t tokens, int position, int count, bool logits) {
    prefill((struct decoder *)(uintptr_t)address, (const int32_t *)(uintptr_t)tokens, position, count, logits);
}
