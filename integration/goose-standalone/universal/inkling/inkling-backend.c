#include "main.h"
#include "graph-execute.h"
#include "ggml.h"

#include "native-kv.h"
#include "moe-epilogue.h"
#include "situ.h"
#include "bridge.h"
#include "inkling-backend.h"

#include <pthread.h>
#include <stdint.h>

static _Thread_local uint64_t prefill_readback[2];

uint64_t fg_i2_readback_state(void) { return (uintptr_t)prefill_readback; }

_Static_assert(offsetof(struct ggml_tensor, type) == 0 && offsetof(struct ggml_tensor, view_src) == 232,
    "Goose graph readback ABI");

enum { logical=INKLING_VISITS, model_width=2304, attn_width=3072, vocabulary=INKLING_TEXT_VOCAB, out_width=201024 };

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

#include "native-host.h"

_Static_assert(offsetof(struct ggml_tensor, data) == 248 && GGML_TYPE_F32 == 0 &&
    GGML_UNARY_OP_TANH == 4, "Goose Inkling control graph ABI");

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

_Static_assert(offsetof(struct inkling_decode_graph, bridge) == 312 && sizeof(struct inkling_bridge) == 48 &&
    offsetof(struct inkling_bridge, postconv) == 0 && offsetof(struct inkling_bridge, query) == 8 &&
    offsetof(struct inkling_bridge, initial_history) == 16 && offsetof(struct inkling_bridge, input) == 24 &&
    offsetof(struct inkling_bridge, slot) == 32 && offsetof(struct inkling_bridge, rows) == 36 &&
    offsetof(struct inkling_bridge, position) == 40 && offsetof(struct inkling_bridge, restart) == 44 &&
    offsetof(struct inkling_bridge, add_residual) == 45 && INKLING_BRIDGE_GRAPH == 0x49324252 &&
    INKLING_BRIDGE_ROWS == 11, "Goose Inkling joined bridge ABI");

_Static_assert(GGML_TYPE_Q8_0 == 8 && offsetof(struct decoder, data) == 0, "Goose embedding ABI");
#include "prefill.h"

_Static_assert(sizeof(struct decoder) == 940272 && offsetof(struct decoder, data) == 0 &&
    offsetof(struct decoder, tensor) == 8 && offsetof(struct decoder, globals) == 73736 &&
    offsetof(struct decoder, core) == 73776 && offsetof(struct decoder, aux) == 75696 &&
    offsetof(struct decoder, visits) == 81456 && offsetof(struct decoder, flash_workspace) == 887112 &&
    offsetof(struct decoder, kh) == 887120 && offsetof(struct decoder, vh) == 887128 &&
    offsetof(struct decoder, hf) == 887144 && INKLING_CONTEXT_LIMIT == 1048576 &&
    INKLING_WINDOW == 1024, "Goose Inkling open ABI");

_Static_assert(offsetof(struct decoder, logits_team) == 887272, "Goose Inkling cleanup ABI");

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
