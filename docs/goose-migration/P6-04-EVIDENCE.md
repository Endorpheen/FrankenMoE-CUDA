# P6-04: Apex graph construction, prefill/decode and state lifecycle

Criterion: "Apex graph construction and real prefill/decode/state lifecycle execute Goose using general shared interfaces; no
model-specific native graph callback."

The model: `qwen35moe`, 35B-A3B, 40 repeating layers (block_count 41 including the nextn block), `n_embd` 2048, 256 experts with 8 used,
`full_attention_interval` 4, and a `recurrent_layers` bit array, so it is a hybrid model with both recurrent and full-attention blocks.

## What executes Goose

- **Model-graph decisions.** `FG200G_MODEL_GRAPH is_recr=800 recurrent=600 native=0` on the accepted run: every layer's recurrent
  decision is answered by the module, with zero native fallbacks counted. `FG200G_MOE group_select=680 scale_check=680` covers expert
  grouping and the scale requirement, and `FG200G_RECR geometry=0` confirms the recurrent geometry path was not taken.
- **Graph input plan.** `FG408S_GRAPH_INPUT_PLAN` is live on the same run.
- **Recurrent payload and IO.** `FG382S_RECURRENT_IO_PLAN`, `FG396S_RECURRENT_PAYLOAD_SPAN` and `FG232R_RECR_STATE_RANGES` (six calls)
  are live, and the loader reports the recurrent prefix checkpoint path as enabled and actually writing checkpoints during the run.

## Real prefill, decode and state

The accepted profile is a 32K-token prefill followed by 128 decoded tokens, and it returns `e68df05b` with all probabilities equal,
including after this session relinked `libllama` with a trace hook. The state lifecycle is witnessed separately: a 131 975 288-byte
state is written, restored by a fresh process and continued identically.

## No model-specific native graph callback

`qwen35moe`'s builder is part of the shared graph code (`llama-graph.cpp` in the rebuilt set); there is no separate native plugin or
per-model callback supplying its graph. The request trace from EXP340 confirms the composition: 40 layers, exactly two MoE gate nodes
per layer, and the conv/recurrent path on 30 of 40 layers -- the hybrid pattern the `recurrent_layers` array describes.

## The precise claim, and what it is not

What holds: the graph is built through the shared builders, every model-graph decision on this model comes from Goose modules with zero
native fallback, and the lifecycle runs with Goose-owned recurrent planning. What does not hold: the builder *bodies* are the shared C++
of this line, not Goose-emitted code. P6-03 measures the builders themselves; this item measures that Apex runs through them with Goosed
decisions and no per-model native callback.
