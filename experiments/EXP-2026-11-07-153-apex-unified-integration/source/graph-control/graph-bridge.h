#pragma once
// Graph bridge contract for worker-1 Goose modules.
// ABI source: inputs/ggml-impl.h:238-359, inputs/ggml.h:685-717.
//
// Typed access: graph and tensor fields have separate externs. No registry,
// no type guessing, no fixed-capacity handle table.
//
// Graph layout/control is owned by graph-storage.goose. The adapter included
// in ggml.c provides only raw primitives: ggml_new_object call, arena pointer,
// addr arithmetic, typed field assign, memzero, and sizeof/alignof constants.
// Graph allocations are owned by the explicit ggml_context passed by callers.
// Views are POD field stores into caller-owned struct ggml_cgraph storage;
// bounds and offset policy stay in Goose.
//
// Generated Goose C must export exactly:
//   int64_t gt_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t)
//   int64_t gs_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t)
//   int64_t hp_entry(int64_t,int64_t,int64_t,int64_t,int64_t,int64_t,int64_t)
// and strong fg153_hp_find/insert/find_or_insert/contains definitions.
// Numeric emitter IDs are not part of this contract.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// Constants.
// gt: 60=LEFT_TO_RIGHT, 61=RIGHT_TO_LEFT, 62=HASHSET_FULL.
// gs: 60,61,62 plus 70=OP_OPT_STEP_ADAMW, 71=TYPE_F32,
//     80..89 are sizeof/alignof values read from the ggml.c TU.
// ---------------------------------------------------------------------------
int64_t fg153_gt_const(int64_t code);
int64_t fg153_gs_const(int64_t code);

// ---------------------------------------------------------------------------
// Tensor fields (typed; tensors only).
// gt tensor: 0=op, 1=flags, 2=src[i], 4=name_present.
// gs tensor: 0=op, 1=flags, 2=type, 3=src[i].
// ---------------------------------------------------------------------------
int64_t fg153_gt_tensor_get(int64_t t, int64_t k, int64_t i);
void    fg153_gt_tensor_put(int64_t t, int64_t k, int64_t i, int64_t v);
int64_t fg153_gs_tensor_get(int64_t t, int64_t k, int64_t i);
void    fg153_gs_tensor_put(int64_t t, int64_t k, int64_t i, int64_t v);

// ---------------------------------------------------------------------------
// Graph fields (typed; graphs only).
// gt graph: 0=size, 1=n_nodes, 2=n_leafs, 3=nodes[i], 4=leafs[i],
//           5=use_counts[i], 6=order, 7=visited_used[i],
//           8=visited_keys[i], 9=hash_size.
// gs graph: same plus 10=grads[i], 11=grad_accs[i], 12=uid,
//           14=nodes_offset, 15=grads_base, 16=grad_accs_base,
//           17=nodes_base, 18=leafs_base,
//           21=use_counts_base, 22=visited_keys_base, 23=visited_used_base.
// Array reads return 0 for null arrays and out-of-range indices.
// ---------------------------------------------------------------------------
int64_t fg153_gt_graph_get(int64_t g, int64_t k, int64_t i);
void    fg153_gt_graph_put(int64_t g, int64_t k, int64_t i, int64_t v);
int64_t fg153_gs_graph_get(int64_t g, int64_t k, int64_t i);
void    fg153_gs_graph_put(int64_t g, int64_t k, int64_t i, int64_t v);

// ---------------------------------------------------------------------------
// Traversal helpers.
// ---------------------------------------------------------------------------
int64_t fg153_gt_hash_find(int64_t g, int64_t t);
void    fg153_gt_ensure_name(int64_t t, int64_t is_leaf, int64_t idx);
int64_t fg153_gt_array_get(int64_t array, int64_t i); // tensor handle array

// ---------------------------------------------------------------------------
// Raw arena primitives. Implemented by graph-arena-adapter.inc, included in
// ggml.c after ggml_new_object and struct ggml_context/ggml_object are
// visible. These primitives contain no graph layout or initialization policy.
// ---------------------------------------------------------------------------
int64_t fg153_gs_arena_new_object(int64_t ctx, int64_t bytes);
int64_t fg153_gs_arena_graph_ptr(int64_t ctx, int64_t obj);
int64_t fg153_gs_addr_add(int64_t base, int64_t off);
void    fg153_gs_raw_graph_set(int64_t base, int64_t code, int64_t i, int64_t v);
void    fg153_gs_raw_zero(int64_t addr, int64_t bytes);

// Layout constants read from the ggml.c TU (sizeof/alignof, no mutable init).
int64_t fg153_gs_sizeof_cgraph(void);
int64_t fg153_gs_sizeof_ptr(void);
int64_t fg153_gs_alignof_ptr(void);
int64_t fg153_gs_sizeof_i32(void);
int64_t fg153_gs_alignof_i32(void);
int64_t fg153_gs_alignof_bitset(void);
int64_t fg153_gs_sizeof_object(void);
int64_t fg153_gs_mem_align(void);

// ---------------------------------------------------------------------------
// Storage services.
// ---------------------------------------------------------------------------
int64_t fg153_gs_hash_prime_count(void);
int64_t fg153_gs_hash_prime(int64_t index);
int64_t fg153_gs_bitset_bytes(int64_t hash_size);
int64_t fg153_gs_hash_insert(int64_t g, int64_t t);
int64_t fg153_gs_hash_find(int64_t g, int64_t t);
int64_t fg153_gs_hash_set(int64_t g); // address of g->visited_hash_set
void    fg153_gs_zero_field(int64_t g, int64_t field, int64_t count);

// Tensor storage helpers used by graph_reset.
int64_t fg153_gs_tensor_empty(int64_t t);
int64_t fg153_gs_tensor_buffer(int64_t t);
int64_t fg153_gs_tensor_data(int64_t t);
void    fg153_gs_zero_data(int64_t t);
void    fg153_gs_backend_zero(int64_t t);
int64_t fg153_gs_tensor_is_scalar(int64_t t);
void    fg153_gs_write_f32_one(int64_t t);

// ---------------------------------------------------------------------------
// Hash raw primitives (hash-primitives.goose imports these).
// ---------------------------------------------------------------------------
int64_t fg153_hp_size(int64_t hs);
int64_t fg153_hp_used_get(int64_t hs, int64_t i);
void    fg153_hp_used_set(int64_t hs, int64_t i, int64_t v);
int64_t fg153_hp_key_get(int64_t hs, int64_t i);
void    fg153_hp_key_set(int64_t hs, int64_t i, int64_t v);
void    fg153_hp_abort(int64_t code);
int64_t fg153_hp_full(void);
int64_t fg153_hp_already(void);

// Strong imports from generated hash-primitives.goose. No weak fallback.
int64_t fg153_hp_find(int64_t hs, int64_t t);
int64_t fg153_hp_insert(int64_t hs, int64_t t);
int64_t fg153_hp_find_or_insert(int64_t hs, int64_t t);
int64_t fg153_hp_contains(int64_t hs, int64_t t);

void fg153_gs_fail(int64_t code);

#ifdef __cplusplus
}
#endif
