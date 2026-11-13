# P6-03 inventory: graph helpers and the trace that must prove them

Criterion: "All shared dense, MoE, attention, rope, norm, recurrent and output graph helpers Goose; node/shape/config traces
differential."

## Current state

`llama-graph.cpp` is rebuilt from the reconstructed tree (it is in the `changed` list, so relinking `libllama` is a normal operation and
does **not** require the blocked `libggml-base`). It carries seventeen hook references:

- `fg200g` (eleven): the model-graph decisions -- recurrent flag, gating kind, expert group selection, weight-before-FFN, scale
  requirement. Live in the accepted run as `FG200G_MODEL_GRAPH is_recr=800 recurrent=600 native=0`, `FG200G_MOE
  group_select=680 scale_check=680`, `FG200G_RECR geometry=0`.
- `fg408s` (six): the graph input plan (mixed-batch detection and the input layout).

The helper families, by call count in the file: `build_forward_expand` 41, `build_lora_mm` 19, `build_input_k_idxs` 14,
`build_attn_inp_kq_mask` 13, `build_input_k_rot` 11, `build_attn_mha` 8, `build_attn` 7, `build_input_v_idxs` 6, `build_lora_mm_id` 5,
`build_forward_select` 5, plus the rope/norm/output helpers.

## What "node/shape/config trace differential" has to mean here

The criterion asks for three traces, and they are different instruments:

1. **config** -- the hparams and model fields that feed the helpers. Already comparable through the metadata probe used in P6-02
   (identical key/value hash), so this one is cheap and should simply be recorded as covered by that evidence.
2. **shape** -- the `ne`/`nb` of every tensor the graph builds. Comparable by dumping the graph with `ggml_graph_dump_dot`, which is a
   public function available on both lines.
3. **node** -- the op sequence. Same dump, read as an ordered list of ops.

## The measurement problem, stated before trying

There is no built-in switch that prints a graph for a real request: `GGML_SCHED_DEBUG` is compiled out (verified in the P6-02 branch) and
no `n_nodes` line appears in verbose logs. So the dump has to come from a small harness that builds the graph itself, or from a
temporary instrumented build -- and this session has already produced three broken instruments (a preload that could not bind, a trace
hook on the wrong object, a debug export that was never emitted). The rule that came out of those failures applies here first: prove the
instrument is live (a counter or a marker) before trusting a single line it prints.

## First slice

Build a harness that constructs a small graph through the public API on both lines, dumps it with `ggml_graph_dump_dot`, and compares the
op sequence and shapes. That covers the *shared* helpers at the API level without touching the model, and it establishes the dump
mechanism. Only then move to a request-driven graph, which needs an instrumented build.
