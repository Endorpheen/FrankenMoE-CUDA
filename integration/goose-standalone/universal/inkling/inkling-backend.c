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

static void drop_decode_graph(struct inkling_decode_graph *entry) {
    fg_standalone_graph_plan_free(entry->plan);
    if (entry->context) ggml_free(entry->context);
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

extern int64_t i2_depth_mix_cached(sl_f32 history, sl_f32 normalized_current, sl_f32 queries,
                                   sl_f32 probabilities, sl_f32 output);

static bool use_cached_history(void) {
    static int enabled = -1;
    if (enabled < 0) {
        const char *setting = getenv("I2_DEPTH_HISTORY_CACHE");
        enabled = !setting || strcmp(setting, "0") != 0;
    }
    return enabled != 0;
}

static void save_history_row(float *row, float *current) {
    if (use_cached_history()) {
        assert(i2_rms_gain(vec(current, model_width), vec(NULL, 0), 1e-6f,
                           vec(row, model_width)) == model_width);
    } else {
        memcpy(row, current, model_width * sizeof(float));
    }
}

static void mix(float *blocks,int count,float *cur,sl_f32 query,int queries,float *out) {
    assert(count>=0 && count<12);
    if (use_cached_history()) {
        float normalized[model_width], probabilities[12];
        assert(i2_rms_gain(vec(cur, model_width), vec(NULL, 0), 1e-6f,
                           vec(normalized, model_width)) == model_width);
        assert(i2_depth_mix_cached(vec(blocks, count * model_width), vec(normalized, model_width),
            query, vec(probabilities, count + 1), vec(out, queries * model_width)) == queries);
        return;
    }
    float states[12*model_width], norm[12*model_width], p[12];
    for(int i=0;i<count;i++) memcpy(states+i*model_width,blocks+(count-1-i)*model_width,model_width*sizeof(float));
    memcpy(states+count*model_width,cur,model_width*sizeof(float));
    int n=(count+1)*model_width;
    assert(i2_depth_mix(vec(states,n),model_width,query,vec(norm,n),vec(p,count+1),vec(out,queries*model_width))==queries);
}

static void project_control_group(struct decoder *d, const int64_t *aux, int at, float *norm,
                                  int count, float *combined) {
    struct ggml_context *ctx = context();
    struct ggml_tensor *input = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, 2 * model_width, count);
    assert(input);
    input->data = norm;
    struct ggml_tensor *h = ggml_mul_mat(ctx, load(ctx, named(d, aux[at])), input);
    struct ggml_tensor *forget = ggml_mul_mat(ctx, load(ctx, named(d, aux[at + 1])), h);
    struct ggml_tensor *gain = ggml_mul_mat(ctx, load(ctx, named(d, aux[at + 2])), h);
    struct ggml_tensor *results[] = {ggml_concat(ctx, gain, forget, 0)};
    evaluate(ctx, results, 1, combined, 2 * count * model_width);
    ggml_free(ctx);
}

static bool use_resident_controller(void) {
    const char *setting = getenv("I2_RESIDENT_CONTROL");
    return uses_resident_plan() && (!setting || strcmp(setting, "0") != 0);
}

static struct ggml_tensor *controller_node(struct ggml_context *ctx, struct decoder *d,
    const int64_t *aux, int at, struct ggml_tensor *signal, struct ggml_tensor *memory,
    struct ggml_tensor *normalized_signal) {
    const float eps = 1e-6f;
    struct ggml_tensor *x = normalized_signal ? normalized_signal : ggml_rms_norm(ctx, signal, eps);
    struct ggml_tensor *y = ggml_rms_norm(ctx, memory, eps);
    struct ggml_tensor *xy = ggml_concat(ctx, x, y, 0);
    struct ggml_tensor *hidden = ggml_mul_mat(ctx, load(ctx, named(d, aux[at])), xy);
    struct ggml_tensor *forget = ggml_mul_mat(ctx, load(ctx, named(d, aux[at + 1])), hidden);
    struct ggml_tensor *gain = ggml_mul_mat(ctx, load(ctx, named(d, aux[at + 2])), hidden);
    struct ggml_tensor *keep = ggml_scale_bias(ctx, ggml_unary(ctx, forget, GGML_UNARY_OP_TANH), -1.0f, 1.0f);
    struct ggml_tensor *add = ggml_unary(ctx, gain, GGML_UNARY_OP_TANH);
    return ggml_add(ctx, ggml_mul(ctx, signal, keep), ggml_mul(ctx, memory, add));
}

static struct ggml_tensor *rows_input(struct ggml_context *ctx, float *values, int width, int count) {
    struct ggml_tensor *tensor = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, width, count);
    assert(tensor);
    tensor->data = values;
    return tensor;
}

static void control_resident(struct decoder *d, const int64_t *aux, int at,
                             float *current, float *message, int count) {
    struct ggml_context *ctx = context();
    struct ggml_tensor *signal = rows_input(ctx, current, model_width, count);
    struct ggml_tensor *memory = rows_input(ctx, message, model_width, count);
    struct ggml_tensor *result[] = {controller_node(ctx, d, aux, at, signal, memory, NULL)};
    evaluate(ctx, result, 1, current, count * model_width);
    ggml_free(ctx);
}

static bool use_resident_moe(void) {
    const char *setting = getenv("I2_RESIDENT_MOE");
    return uses_resident_plan() && (!setting || strcmp(setting, "0") != 0);
}

static struct ggml_tensor *moe_node(struct ggml_context *ctx, struct decoder *d,
        const int64_t *core, const int64_t *aux, int slot, int position, int count,
        struct ggml_tensor *input, float *message, struct ggml_tensor *residual,
        struct inkling_decode_graph *entry) {
    if (message) {
        struct ggml_tensor *memory = rows_input(ctx, message, model_width, count);
        if (entry) entry->messages[0] = memory;
        input = controller_node(ctx, d, aux, 18 + 3 * 3, input, memory, NULL);
    }
    struct ggml_tensor *norm = ggml_rms_norm(ctx, input, 1e-6f);
    struct ggml_tensor *scaled = ggml_mul(ctx, norm, load(ctx, named(d, core[9])));
    const int64_t names[] = {core[5], core[8], core[6], core[7]};
    int64_t weights[4], work[16] = {0}, nodes[8] = {0};
    for (int i = 0; i < 4; ++i) weights[i] = (intptr_t)load(ctx, named(d, names[i]));
    struct ggml_tensor *moe = (struct ggml_tensor *)(intptr_t)i2_moe_ffn((intptr_t)ctx,
        (intptr_t)scaled, ints(weights, 4), ints(work, 16), ints(nodes, 8));
    assert(moe);
    const char *situ = getenv("I2_FUSED_MOE_SITU");
    if (!situ || strcmp(situ, "0") != 0) {
        struct ggml_tensor *hidden = (struct ggml_tensor *)(intptr_t)nodes[6];
        struct ggml_tensor *gate_up = (struct ggml_tensor *)(intptr_t)nodes[5];
        assert(hidden && gate_up);
        hidden->op = GGML_OP_CUSTOM;
        memset(hidden->op_params, 0, sizeof hidden->op_params);
        memset(hidden->src, 0, sizeof hidden->src);
        hidden->op_params[0] = INKLING_MOE_SITU;
        hidden->src[0] = gate_up;
    }
    struct ggml_tensor *epilogue = ggml_dup_tensor(ctx, moe);
    assert(epilogue);
    epilogue->op = GGML_OP_CUSTOM;
    epilogue->op_params[0] = INKLING_MOE_EPILOGUE;
    epilogue->op_params[1] = slot;
    epilogue->op_params[2] = position;
    epilogue->src[0] = moe;
    epilogue->src[1] = residual;
    epilogue->src[2] = load(ctx, named(d, aux[3]));
    return epilogue;
}

static void moe_resident(struct decoder *d, const int64_t *core, const int64_t *aux,
                         int slot, int position, int count, float *ffn, float *message,
                         float *residual, float *out) {
    struct inkling_decode_graph *entry = count == 1 && d->retain_decode_graphs ? &d->decode_moe[slot] : NULL;
    if (entry && entry->plan) {
        assert((entry->messages[0] != NULL) == (message != NULL));
        entry->inputs[0]->data = ffn;
        if (message) entry->messages[0]->data = message;
        entry->residual->data = residual;
        entry->output->op_params[2] = position;
        execute_decode_graph(entry, out);
        return;
    }
    struct ggml_context *ctx = context();
    struct ggml_tensor *input = rows_input(ctx, ffn, model_width, count);
    if (entry) entry->inputs[0] = input;
    struct ggml_tensor *epilogue = moe_node(ctx, d, core, aux, slot, position, count,
        input, message, rows_input(ctx, residual, model_width, count), entry);
    if (entry) {
        entry->context = ctx;
        entry->output = epilogue;
        entry->residual = epilogue->src[1];
        execute_decode_graph(entry, out);
        return;
    }
    struct ggml_tensor *result[] = {epilogue};
    evaluate(ctx, result, 1, out, count * model_width);
    ggml_free(ctx);
}

static void control(struct decoder *d,const int64_t *aux,int which,float *current,float *message) {
    if (use_resident_controller()) {
        control_resident(d, aux, 18 + which*3, current, message, 1);
        return;
    }
    float norm[2*model_width], h[64], gain[model_width], forget[model_width];
    assert(i2_rms_gain(vec(current,model_width),vec(NULL,0),1e-6f,vec(norm,model_width))==model_width);
    assert(i2_rms_gain(vec(message,model_width),vec(NULL,0),1e-6f,vec(norm+model_width,model_width))==model_width);
    const int at=18+which*3;
    if (uses_resident_plan()) {
        float combined[2 * model_width];
        project_control_group(d, aux, at, norm, 1, combined);
        memcpy(gain, combined, sizeof gain);
        memcpy(forget, combined + model_width, sizeof forget);
    } else {
        project(d,aux[at],norm,2*model_width,h,64);
        project(d,aux[at+1],h,64,forget,model_width);
        project(d,aux[at+2],h,64,gain,model_width);
    }
    assert(i2_loop_blend(vec(current,model_width),vec(message,model_width),vec(gain,model_width),
                         vec(forget,model_width),vec(current,model_width))==model_width);
}

static bool use_attention_graph(void) {
    const char *setting = getenv("I2_ATTENTION_GRAPH");
    return use_device_attention_input() && uses_resident_plan() &&
        (!setting || strcmp(setting, "0") != 0);
}

static bool share_attention_inputs(bool sliding, const int64_t *aux) {
    const char *sharing = getenv("I2_SHARED_ATTENTION_INPUTS");
    return (sliding || aux[10] < 0) && (!sharing || strcmp(sharing, "0") != 0);
}

static struct ggml_tensor *attention_node(struct ggml_context *ctx, struct decoder *d,
        const int64_t *aux, int count, bool sliding, float *input[3], float *message[3],
        const struct inkling_attention_graph_call *metadata, struct inkling_decode_graph *entry) {
    struct ggml_tensor *node = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, model_width, count);
    assert(node);
    node->op = GGML_OP_CUSTOM;
    node->op_params[0] = INKLING_ATTENTION_GRAPH;
    memcpy(node->op_params + 2, &metadata, sizeof(metadata));
    const bool shared = share_attention_inputs(sliding, aux);
    struct ggml_tensor *signal = shared ? rows_input(ctx, input[0], model_width, count) : NULL;
    struct ggml_tensor *normalized_signal = signal ? ggml_rms_norm(ctx, signal, 1e-6f) : NULL;
    if (entry && signal) entry->inputs[0] = signal;
    for (int i = 0; i < 3; ++i) {
        if (i > 0 && shared && sliding && !message) {
            node->src[i] = node->src[0];
            continue;
        }
        struct ggml_tensor *normalized = normalized_signal;
        if (message || !normalized) {
            struct ggml_tensor *x = signal ? signal : rows_input(ctx, input[i], model_width, count);
            if (entry && !signal) entry->inputs[i] = x;
            if (message) {
                struct ggml_tensor *memory = rows_input(ctx, message[i], model_width, count);
                if (entry) entry->messages[i] = memory;
                x = controller_node(ctx, d, aux, 18 + 3 * i, x, memory, normalized_signal);
            }
            normalized = ggml_rms_norm(ctx, x, 1e-6f);
        }
        struct ggml_tensor *gain = load(ctx, named(d, aux[sliding ? 11 : 12 + i]));
        node->src[i] = ggml_mul(ctx, normalized, gain);
    }
    return node;
}

static void attention_graph(struct decoder *d, const int64_t *core, const int64_t *aux,
                            int slot, int begin, int count, bool sliding,
                            float *input[3], float *message[3], float *out) {
    const struct inkling_attention_graph_call call = bind_attention_graph(d, core, aux, slot, begin, count, sliding);
    struct inkling_decode_graph *entry = count == 1 && d->retain_decode_graphs ? &d->decode_attention[slot] : NULL;
    if (entry && entry->plan) {
        assert((entry->messages[0] != NULL) == (message != NULL));
        entry->attention.batch.first = begin;
        for (int i = 0; i < 3; ++i) {
            if (entry->inputs[i]) entry->inputs[i]->data = input[i];
            if (message) entry->messages[i]->data = message[i];
        }
        execute_decode_graph(entry, out);
        return;
    }
    const struct inkling_attention_graph_call *metadata = &call;
    if (entry) {
        entry->attention = call;
        metadata = &entry->attention;
    }
    struct ggml_context *ctx = context();
    struct ggml_tensor *node = attention_node(ctx, d, aux, count, sliding, input, message, metadata, entry);
    if (entry) {
        entry->context = ctx;
        entry->output = node;
        execute_decode_graph(entry, out);
        return;
    }
    struct ggml_tensor *result[] = {node};
    evaluate(ctx, result, 1, out, count * model_width);
    ggml_free(ctx);
}

static bool use_joined_bridge(void) {
    const char *setting = getenv("I2_JOIN_ATTENTION_MOE");
    return setting && strcmp(setting, "1") == 0;
}

static void joined_layer(struct decoder *d, const int64_t *core, const int64_t *aux,
        int slot, int position, bool sliding, bool add_residual, float *input[3],
        float (*messages)[model_width], float *cur, float *history, int rows) {
    struct inkling_decode_graph *entry = &d->decode_joined[slot];
    float bridge_input[(INKLING_BRIDGE_ROWS + 1) * model_width];
    assert(rows >= 0 && rows <= INKLING_BRIDGE_ROWS);
    memcpy(bridge_input, cur, model_width * sizeof(float));
    memcpy(bridge_input + model_width, history, rows * model_width * sizeof(float));
    entry->bridge = (struct inkling_bridge) {
        .postconv = coefficients(d, aux[2], 4 * model_width).data,
        .query = coefficients(d, aux[9], model_width).data,
        .initial_history = d->ha + slot * 3 * model_width,
        .input = bridge_input, .slot = slot, .rows = rows, .position = position,
        .restart = !entry->bridge_started, .add_residual = add_residual,
    };
    if (entry->plan) {
        entry->attention.batch.first = position;
        for (int i = 0; i < 3; ++i) {
            if (entry->inputs[i]) entry->inputs[i]->data = input[i];
            if (messages) entry->messages[i]->data = messages[i];
        }
        if (messages) entry->residual->data = messages[3];
        entry->output->op_params[2] = position;
    } else {
        struct ggml_context *ctx = context();
        entry->context = ctx;
        entry->attention = bind_attention_graph(d, core, aux, slot, position, 1, sliding);
        float *attn_messages[3];
        for (int i = 0; i < 3; ++i) attn_messages[i] = messages ? messages[i] : NULL;
        struct ggml_tensor *attention = attention_node(ctx, d, aux, 1, sliding,
            input, messages ? attn_messages : NULL, &entry->attention, entry);
        struct ggml_tensor *bridge = ggml_new_tensor_2d(ctx, GGML_TYPE_F32, model_width, 2);
        assert(bridge);
        bridge->op = GGML_OP_CUSTOM;
        bridge->op_params[0] = INKLING_BRIDGE_GRAPH;
        const struct inkling_bridge *metadata = &entry->bridge;
        memcpy(bridge->op_params + 2, &metadata, sizeof metadata);
        bridge->src[0] = attention;
        struct ggml_tensor *ffn = ggml_view_1d(ctx, bridge, model_width, 0);
        struct ggml_tensor *next = ggml_view_1d(ctx, bridge, model_width, model_width * sizeof(float));
        struct inkling_decode_graph moe = {0};
        entry->output = moe_node(ctx, d, core, aux, slot, position, 1,
            ffn, messages ? messages[3] : NULL, next, &moe);
        entry->residual = moe.messages[0];
    }
    execute_decode_graph(entry, cur);
    entry->bridge_started = true;
}

static void layer(struct decoder *d,int physical_layer,int slot,int absolute,int sliding,
                  float *cur,float *blocks,int *count,float (*messages)[model_width]) {
    const int64_t *core=d->core+10*physical_layer, *aux=d->aux+30*physical_layer;
    float inputs[3][model_width];
    if(aux[10]>=0) {
        const int n=sliding?1:3;
        mix(blocks,*count,cur,coefficients(d,aux[10],n*model_width),n,inputs[0]);
        if(sliding){memcpy(inputs[1],inputs[0],sizeof inputs[0]);memcpy(inputs[2],inputs[0],sizeof inputs[0]);}
    } else {
        for(int i=0;i<3;i++)memcpy(inputs[i],cur,model_width*sizeof(float));
    }
    float proj[model_width], next[model_width], ffn[model_width];
    int64_t work[16] = {0};
    struct ggml_context *ctx;
    if (use_attention_graph()) {
        float *input[] = {inputs[0], inputs[1], inputs[2]};
        float *message[] = {messages ? messages[0] : NULL, messages ? messages[1] : NULL,
                            messages ? messages[2] : NULL};
        if (d->retain_decode_graphs && use_joined_bridge() && use_cached_history() && use_resident_moe()) {
            const bool add_residual = physical_layer % 6 != 0;
            if (!add_residual) {
                assert(*count < INKLING_BRIDGE_ROWS);
                save_history_row(blocks + (*count) * model_width, cur);
                ++*count;
            }
            joined_layer(d, core, aux, slot, absolute, sliding, add_residual, input, messages, cur, blocks, *count);
            return;
        }
        attention_graph(d, core, aux, slot, absolute, 1, sliding, input, messages ? message : NULL, proj);
    } else {
        for(int i=0;i<3;i++){
            if(messages)control(d,aux,i,inputs[i],messages[i]);
            assert(i2_rms_gain(vec(inputs[i],model_width),coefficients(d,aux[sliding?11:12+i],model_width),
                               1e-6f,vec(inputs[i],model_width))==model_width);
        }

        if (use_device_attention_input()) {
            native_attention_resident(d, core, aux, slot, absolute, 1, sliding, inputs[0], inputs[1], inputs[2], proj);
        } else {
            ctx = context();
            int64_t in[3], projection[8]={0}, wp[8]={0};
            for(int i=0;i<3;i++)in[i]=(intptr_t)input_tensor(ctx,inputs[i],model_width);
            int64_t ids[]={core[0],core[1],core[2],aux[5],aux[8],core[4],aux[15],aux[16]};
            const int count_proj=sliding?6:8;
            for (int i=0;i<count_proj;i++)wp[i]=(intptr_t)load(ctx,named(d,ids[i]));
            assert(i2_attention_projections((intptr_t)ctx,ints(in,3),ints(wp,8),sliding,
                                            ints(work,16),ints(projection,8))==count_proj);
            struct ggml_tensor *roots[8];
            for(int i=0;i<count_proj;i++)roots[i]=(struct ggml_tensor *)(intptr_t)projection[i];
            float projected[8664], attn[attn_width];
            int packed=sliding?8600:8664;
            evaluate(ctx,roots,count_proj,projected,packed);
            ggml_free(ctx);

            struct native_attention_inputs args = native_attention_inputs(d, 1);
            native_attention_prepare(d, args, aux, slot, absolute, 0, sliding, projected, packed);
            if (use_resident_attention_projection()) {
                native_attention_project(d, args, aux, slot, absolute, 1, sliding, named(d, core[3]), proj);
            } else {
                native_attention_submit(d, args, aux, slot, absolute, 1, sliding, attn);
                project(d, core[3], attn, attn_width, proj, model_width);
            }
        }
    }
    assert(i2_postconv_token(vec(proj,model_width),coefficients(d,aux[2],4*model_width),
                            vec(d->ha+slot*3*model_width,3*model_width),vec(next,model_width))==model_width);
    if(physical_layer%6==0){
        assert(*count < 11);
        save_history_row(blocks + (*count) * model_width, cur);
        (*count)++;
    }else{
        for(int i=0;i<model_width;i++)next[i]+=cur[i];
    }
    mix(blocks,*count,next,coefficients(d,aux[9],model_width),1,ffn);
    if (use_resident_moe()) {
        moe_resident(d, core, aux, slot, absolute, 1, ffn, messages ? messages[3] : NULL, next, cur);
        return;
    }
    if(messages)control(d,aux,3,ffn,messages[3]);
    assert(i2_rms_gain(vec(ffn,model_width),coefficients(d,core[9],model_width),1e-6f,vec(ffn,model_width))==model_width);
    ctx=context();
    int64_t moe_weights[4];
    const int64_t mid[]={core[5],core[8],core[6],core[7]};
    for(int i=0;i<4;i++)moe_weights[i]=(intptr_t)load(ctx,named(d,mid[i]));
    int64_t nodes[8]={0};
    struct ggml_tensor *y=(struct ggml_tensor *)(intptr_t)i2_moe_ffn((intptr_t)ctx,
       (intptr_t)input_tensor(ctx,ffn,model_width),ints(moe_weights,4),ints(work,16),ints(nodes,8));
    assert(y);
    struct ggml_tensor *moe_root[]={y};
    evaluate(ctx,moe_root,1,proj,model_width);
    ggml_free(ctx);
    assert(i2_postconv_token(vec(proj,model_width),coefficients(d,aux[3],4*model_width),
                            vec(d->hf+slot*3*model_width,3*model_width),vec(cur,model_width))==model_width);
    for(int i=0;i<model_width;i++){
        cur[i]+=next[i];
        assert(isfinite(cur[i]));
    }
}

static bool parallel_decode_logits(struct decoder *d, float *logits);

static void read_token_logits(struct decoder *d, int absolute, float *cur, float *blocks, int count, bool decode) {
    float normalized[model_width];
    mix(blocks,count,cur,coefficients(d,d->globals[4],model_width),1,normalized);
    assert(i2_rms_gain(vec(normalized,model_width),coefficients(d,d->globals[3],model_width),
                      1e-6f,vec(normalized,model_width))==model_width);
    float *logits = d->logits;
    project(d,d->globals[2],normalized,model_width,logits,out_width);
    const bool handled = decode && parallel_decode_logits(d, logits);
    if (!handled) assert(i2_text_logits(vec(logits,out_width),vocabulary,30.0f)==vocabulary);
    check_state("logits", absolute, logits, out_width);
    if (!handled) for (int i = 0; i < vocabulary; ++i) assert(isfinite(logits[i]));

}

extern int32_t i2_walk_serial_token(uint64_t decoder, sl_i64 visits, int32_t absolute,
    sl_f32 current, sl_f32 history, sl_f32 prelude, sl_f32 bank_output, sl_f32 bank,
    sl_f32 messages, uint8_t trace);

int32_t i2_serial_layer(uint64_t decoder, int32_t physical, int32_t slot, int32_t absolute,
    int32_t sliding, sl_f32 current, sl_f32 history, int32_t count, sl_f32 messages) {
    int history_count = count;
    layer((void *)(uintptr_t)decoder, physical, slot, absolute, sliding,
          current.data, history.data, &history_count, messages.len ? (void *)messages.data : NULL);
    return history_count;
}

void i2_serial_mix(uint64_t decoder, int32_t physical, sl_f32 history, int32_t count,
    sl_f32 output, sl_f32 messages) {
    struct decoder *d = (void *)(uintptr_t)decoder;
    mix(history.data, count, output.data, coefficients(d, d->aux[physical * 30 + 17], 4 * model_width),
        4, messages.data);
}

void i2_serial_trace(int32_t visit, sl_f32 current) {
    check_state("draft-serial-visit", visit, current.data, (size_t)current.len);
}

static void token(struct decoder *d, int32_t input, int absolute, bool need_logits) {
    float cur[model_width], prelude[model_width], bank_output[model_width], blocks[12*model_width];
    float bank[12*model_width], messages[4][model_width];
    weight emb=named(d,d->globals[0]);
    int64_t row=ggml_row_size(GGML_TYPE_Q8_0,model_width);
    assert(i2_embed_row((sl_u8){(uint8_t *)(uintptr_t)(emb.data+row*input),row},
                        coefficients(d,d->globals[1],model_width),1e-6f,vec(cur,model_width))==model_width);
    const int count = i2_walk_serial_token((uintptr_t)d, ints(d->visits, logical * 4), absolute,
        vec(cur, model_width), vec(blocks, 12 * model_width), vec(prelude, model_width),
        vec(bank_output, model_width), vec(bank, 12 * model_width), vec(messages[0], 4 * model_width),
        absolute == 4097 && getenv("I2_DRAFT_PARITY"));
    fprintf(stderr,"Pure Goose visited 48 layers for token %d at position %d\n",input,absolute);
    check_state("residual", absolute, cur, model_width);
    if (need_logits) read_token_logits(d, absolute, cur, blocks, count, true);
}

extern int64_t i2_place_expert_bank(sl_i64 tensors, sl_i64 core, int64_t data, int64_t budget);
extern int64_t i2_place_weights(sl_i64 tensors, sl_i64 core, int64_t data, int64_t excluded,
    int64_t budget, uint8_t adaptive, uint8_t keep_routers, sl_i64 ranges, sl_i64 totals);
extern void fg_cuda_expert_stats(sl_i64 stats);

static int64_t load_inkling_expert_bank(struct decoder *d, int64_t budget) {
    return i2_place_expert_bank(ints(d->tensor, sizeof(d->tensor) / sizeof(*d->tensor)),
        ints(d->core, physical * 10), (intptr_t)d->data, budget);
}

static int64_t place_inkling_weights(struct decoder *d, int64_t tensor_count, int64_t budget, bool adaptive) {
    const int64_t capacity = (tensor_count + physical * experts * 2) * 3;
    int64_t *ranges = calloc((size_t)capacity, sizeof(*ranges));
    assert(ranges);
    const char *config = getenv("I2_RESIDENT_ROUTERS");
    int64_t totals[2];
    const int64_t used = i2_place_weights(ints(d->tensor, tensor_count * 12), ints(d->core, physical * 10),
        (intptr_t)d->data, d->globals[0], budget, adaptive, adaptive && (!config || strcmp(config, "0") != 0),
        ints(ranges, capacity), ints(totals, 2));
    free(ranges);
    printf("Resident weights: %.2f MiB, dense %.2f MiB\n", used / 1048576.0, totals[0] / 1048576.0);
    printf("Resident router weights: %" PRId64 " bytes\n", totals[1]);
    return used;
}

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


static void free_decoder(struct decoder *d) {
    if (d->logits_team) {
        prefill_team_close(d->logits_team);
        free(d->logits_team);
    }
    for (int i = 0; i < logical; ++i) {
        drop_decode_graph(&d->decode_attention[i]);
        drop_decode_graph(&d->decode_moe[i]);
        drop_decode_graph(&d->decode_joined[i]);
    }
    free(d->draft.history);
    free(d->draft.scores);
    free(d->draft.saved_history);
    free(d->hf);
    free(d->ha);
    free(d->vh);
    free(d->kh);
    free(d->flash_workspace);
    free(d);
}

struct decoder *inkling_backend_open(const uint8_t *image, size_t bytes, int64_t capacity,
                                     int64_t expert_limit, enum inkling_kv_format format) {
    if (!image || bytes > INT64_MAX || capacity <= 0 || capacity > INKLING_CONTEXT_LIMIT ||
        expert_limit < 0) return NULL;

    struct decoder *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->data = image;
    d->capacity = capacity;
    d->format = format;
    int64_t st[8] = {0}, kv[256*8] = {0}, key_slots[512] = {0},
        tensor_slots[2048] = {0}, layout[9] = {0};
    const int64_t header_bytes = bytes < 64 * 1024 * 1024 ? (int64_t)bytes : 64 * 1024 * 1024;
    sl_u8 header = {(uint8_t *)(uintptr_t)image, header_bytes};
    if (gr_parse(header, bytes, ints(st, 8), ints(kv, 256*8), ints(d->tensor, 768*12),
                 ints(key_slots, 512), ints(tensor_slots, 2048)) != 0 ||
        i2_read_layout(header, ints(st, 8), ints(kv, 256*8), ints(key_slots, 512),
                       ints(layout, 9), ints(d->visits, logical*4)) != logical ||
        i2_bind_text_weights(header, ints(st, 8), ints(kv, 256*8), ints(key_slots, 512),
                            ints(d->tensor, 768*12), ints(tensor_slots, 2048), ints(layout, 9),
                            ints(d->globals, 5), ints(d->core, physical*10)) != physical ||
        i2_bind_text_aux(header, ints(st, 8), ints(kv, 256*8), ints(key_slots, 512),
                        ints(d->tensor, 768*12), ints(tensor_slots, 2048), ints(layout, 9),
                        ints(d->aux, physical*30)) != physical) {
        free_decoder(d);
        return NULL;
    }

    d->kh = calloc(logical * 3 * key_width, sizeof(float));
    d->vh = calloc(logical * 3 * key_width, sizeof(float));
    d->ha = calloc(logical * 3 * model_width, sizeof(float));
    d->hf = calloc(logical * 3 * model_width, sizeof(float));
    if (!d->kh || !d->vh || !d->ha || !d->hf) {
        free_decoder(d);
        return NULL;
    }

#ifdef I2_NUMERIC_MODULE
    i2_candidate_init(0, NULL);
#endif
    const int64_t free_bytes = fg_cuda_free_bytes();
    const int64_t local_rows = capacity < INKLING_WINDOW ? capacity : INKLING_WINDOW;
    const int64_t kv_rows = 8 * capacity + 40 * local_rows;
    const int64_t row_bytes = format == INKLING_KV_Q4_0 ? key_width / 32 * 18 : key_width * sizeof(uint16_t);
    const int64_t reserve = 1024LL*1024*1024 + kv_rows*(2*row_bytes + sizeof(int64_t));
    if (free_bytes <= reserve || (expert_limit > 0 && expert_limit > free_bytes - reserve)) {
        free_decoder(d);
        return NULL;
    }
    const int64_t cap = 7168LL*1024*1024;
    const int64_t available = free_bytes - reserve;
    const int64_t budget = expert_limit > 0 ? available : (available < cap ? available : cap);
    fg_cuda_enable_inkling_quants(true);
    const int64_t resident = place_inkling_weights(d, st[3], budget, true);
    const int64_t expert_budget = expert_limit > 0 ? expert_limit : budget - resident;
    if (resident <= 0 || expert_budget > available - resident ||
        load_inkling_expert_bank(d, expert_budget) <= 0) {
        free_decoder(d);
        return NULL;
    }
    return d;
}

int inkling_backend_reset(struct decoder *d) {
    if (!d || reset_inkling_attention(d) != 0) return -1;
    d->position = 0;
    if (d->draft.groups) {
        fprintf(stderr, "Inkling speculation groups=%u cached=%u rejects=%u\n",
                d->draft.groups, d->draft.accepted, d->draft.rejects);
    }
    d->draft.count = d->draft.used = 0;
    d->draft.groups = d->draft.accepted = d->draft.rejects = 0;
    for (int i = 0; i < logical; ++i) d->decode_joined[i].bridge_started = false;
    return 0;
}

extern void inkling_draft_verification(bool active);
extern bool inkling_draft_native_snapshot(bool restore, int position);
extern bool inkling_draft_moe_snapshot(bool restore, int position);
extern bool inkling_draft_bridge_history(float *destination, int position);

static bool draft_init(struct decoder *d) {
    struct i2_draft_state *draft = &d->draft;
    if (draft->history) return true;
    const char *enabled = getenv("I2_NGRAM_VERIFY");
    if (!enabled || strcmp(enabled, "1") != 0 || d->position != 0 ||
        d->format != INKLING_KV_Q4_0 || !use_attention_graph() ||
        !use_cached_history() || !use_resident_moe() || !use_joined_bridge()) return false;
    draft->history = malloc((size_t)d->capacity * sizeof(*draft->history));
    draft->scores = malloc((size_t)i2_draft_max * vocabulary * sizeof(float));
    draft->saved_history = malloc((size_t)logical * 3 * model_width * sizeof(float));
    if (draft->history && draft->scores && draft->saved_history) return true;
    free(draft->history);
    free(draft->scores);
    free(draft->saved_history);
    draft->history = NULL;
    draft->scores = draft->saved_history = NULL;
    return false;
}

static bool draft_start(struct decoder *d, int count) {
    struct i2_draft_state *draft = &d->draft;
    const size_t bytes = (size_t)logical * 3 * model_width * sizeof(float);
    const int position = d->position;
    memcpy(draft->saved_history, d->ha, bytes);
    if ((d->decode_joined[0].bridge_started && !inkling_draft_bridge_history(d->ha, position)) ||
        !inkling_draft_native_snapshot(false, position) ||
        !inkling_draft_moe_snapshot(false, position)) {
        memcpy(d->ha, draft->saved_history, bytes);
        return false;
    }
    const char *lfu = getenv("I2_VERIFY_LFU");
    const bool preserve_lfu = !lfu || strcmp(lfu, "0") != 0;
    int64_t stats_before[6] = {0}, stats_after[6] = {0};
    fg_cuda_expert_stats(ints(stats_before, 6));
    fg_cuda_inkling_expert_phase(!preserve_lfu, position, count);
    d->draft.count = count;
    inkling_draft_verification(true);
    prefill(d, draft->tokens, position, count, true);
    inkling_draft_verification(false);
    draft->begin = position;
    draft->used = 0;
    ++draft->groups;
    fg_cuda_expert_stats(ints(stats_after, 6));
    fprintf(stderr, "Inkling speculation start position=%d count=%d lfu=%d fills=%" PRId64 " h2d=%" PRId64 "\n",
            position, count, preserve_lfu, stats_after[1] - stats_before[1], stats_after[3] - stats_before[3]);
    return true;
}

static bool draft_reject(struct decoder *d) {
    struct i2_draft_state *draft = &d->draft;
    const int position = draft->begin;
    if (!inkling_draft_native_snapshot(true, position) || !inkling_draft_moe_snapshot(true, position)) return false;
    memcpy(d->ha, draft->saved_history, (size_t)logical * 3 * model_width * sizeof(float));
    const bool retain = d->retain_decode_graphs;
    d->retain_decode_graphs = true;
    for (int i = 0; i < draft->used; ++i) {
        fg_cuda_inkling_expert_phase(false, position + i, 1);
        token(d, draft->tokens[i], position + i, false);
    }
    d->retain_decode_graphs = retain;
    fprintf(stderr, "Inkling speculation rejected position=%d used=%d count=%d\n", position, draft->used, draft->count);
    ++draft->rejects;
    draft->count = draft->used = 0;
    return true;
}

static void draft_take(struct decoder *d, float *logits, int64_t count) {
    struct i2_draft_state *draft = &d->draft;
    if (count) memcpy(logits, draft->scores + (size_t)draft->used * vocabulary, (size_t)count * sizeof(float));
    if (draft->used) ++draft->accepted;
    ++draft->used;
    ++d->position;
    if (draft->used == draft->count) {
        fprintf(stderr, "Inkling speculation committed position=%d count=%d\n", draft->begin, draft->count);
        for (int i = 0; i < logical; ++i) d->decode_joined[i].bridge_started = false;
        draft->count = draft->used = 0;
    }
}

extern int64_t i2_eval_prepared(uint64_t decoder, sl_i32 tokens, uint8_t prompt, sl_f32 logits,
    sl_i32 history, sl_i32 proposals, int64_t position, uint8_t pending, int32_t used, uint8_t retain);

uint8_t i2_adapter_draft_start(uint64_t decoder, int32_t count) {
    return draft_start((void *)(uintptr_t)decoder, count);
}

uint8_t i2_adapter_draft_reject(uint64_t decoder) {
    return draft_reject((void *)(uintptr_t)decoder);
}

void i2_adapter_draft_take(uint64_t decoder, sl_f32 logits) {
    draft_take((void *)(uintptr_t)decoder, logits.data, logits.len);
}

void i2_adapter_run_step(uint64_t decoder, sl_i32 tokens, uint8_t prompt, sl_f32 logits) {
    struct decoder *d = (void *)(uintptr_t)decoder;
    fg_cuda_inkling_expert_phase(prompt, d->position, tokens.len);
    if (prompt) prefill(d, tokens.data, d->position, tokens.len, logits.len != 0);
    else token(d, tokens.data[0], d->position, logits.len != 0);
    d->position += tokens.len;
    if (logits.len) memcpy(logits.data, d->logits, (size_t)logits.len * sizeof(float));
}

int64_t inkling_backend_eval(struct decoder *d, const int32_t *tokens, int64_t count,
                             bool prompt, float *logits, int64_t logits_count) {
    if (!d || !tokens || count <= 0 || count > max_prefill_batch ||
        (!prompt && count != 1) || count > d->capacity - d->position ||
        (logits_count != 0 && (logits_count != vocabulary || !logits))) return -1;
    for (int64_t i = 0; i < count; ++i) {
        if (tokens[i] < 0 || tokens[i] >= vocabulary) return -1;
    }

    const char *retain = getenv("I2_RETAIN_DECODE_GRAPHS");
    d->retain_decode_graphs = !prompt && (!retain || strcmp(retain, "0") != 0);
    const bool draft_ready = draft_init(d);
    struct i2_draft_state *draft = &d->draft;
    return i2_eval_prepared((uintptr_t)d, (sl_i32){(int32_t *)tokens, count}, prompt,
        vec(logits, logits_count), (sl_i32){draft->history, draft_ready ? d->capacity : 0},
        (sl_i32){draft->tokens, i2_draft_max}, d->position, draft->count != 0,
        draft->used, d->retain_decode_graphs);
}

void inkling_backend_report(void) {
    int64_t stats[6] = {0};
    fg_cuda_expert_stats(ints(stats, 6));
    printf("Expert statistics: hits=%" PRId64 " fills=%" PRId64 " evictions=%" PRId64
           " bytes=%" PRId64 " pinned=%" PRId64 " ranges=%" PRId64 "\n",
           stats[0], stats[1], stats[2], stats[3], stats[4], stats[5]);
    inkling_attention_report();
}
