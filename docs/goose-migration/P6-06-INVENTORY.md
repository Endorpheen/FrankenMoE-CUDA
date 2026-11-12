# P6-06 inventory: recurrent and hybrid text builders

Criterion: "Every remaining recurrent/hybrid text builder in the kept set mapped with a tiny fixture. Multimodal encoders, projectors
and audio builders are out of scope by owner directive."

## Builders in the rebuilt graph file

Recurrent/hybrid-specific entry points present in `llama-graph.cpp`:

- `build_inp_mem_hybrid`
- `build_inp_mem_hybrid_iswa`
- `build_inp_mem_hybrid_k`
- `build_rwkv_token_shift_load` / `build_rwkv_token_shift_store`

The per-architecture text builders that use them (`llm_build_qwen35moe` and the other kept architectures) are part of the same shared
file.

## What actually executes on Apex, from the live trace

The request trace from EXP340 (`FG_TRACE_GRAPH`, 3819 nodes per graph, two graphs) contains the hybrid path explicitly:

| node family | op | per request |
|---|---|---|
| `cache_r_l<N>` views | 37 (VIEW) | 60 |
| `cache_r_l<N>` reshapes | 36 (RESHAPE) | 30 |
| `cache_r_l<N>` scales | 32 (SCALE) | 30 |
| `cache_r_l<N>` copies | 34 (CPY) | 30 |
| `conv_output_silu` | 91 | 30 |

Thirty occurrences per request is exactly the number of recurrent layers the model's `recurrent_layers` array declares (`conv_output_silu`
appeared on 30 of 40 layers in the EXP340 analysis), so the mapping from builder to executed graph is established for the hybrid path:
each recurrent layer contributes one conv node and its cache views.

## Why the "tiny fixture" half is not produced here

A fixture for these builders needs a hybrid model small enough to assert on node by node. None exists in the tree, and fabricating a
minimal GGUF with a `recurrent_layers` array is a build task of its own -- not something to claim as done by pointing at Apex, which is
a 35B model and not a fixture.

What is established: the builders are present, the hybrid path they produce is visible in a real request with per-layer counts matching
the model's declared recurrent layers, and the model-graph decisions feeding them are Goosed with zero native fallback (P6-04).

What is missing: a small hybrid model to serve as the tiny fixture. That is a configuration gap, not a code gap, and it is recorded as
such rather than papered over with the Apex run.
