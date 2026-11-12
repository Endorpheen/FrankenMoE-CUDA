# EXP343: regression sweep after this session's changes

The session produced roughly thirty commits, including one that changed a source file in the rebuilt set (`llama-context.cpp`, the
env-gated graph trace) and the relink of `libllama` that followed. This sweep re-runs the gates that were green before, to confirm nothing
drifted.

## Results

| gate | result |
|---|---|
| accepted profile (Apex 32K/128) | `e68df05b`, 128 tokens |
| P4-03 constructor/view probe (block-aligned) | identical to flint, 15 lines |
| P4-03 ops/graph probe | identical except one line: `ggml_graph_dump_dot` prints the temp path it was given, which differs per run |
| MXFP4 encoder | 3944 checks, 0 failures |
| NVFP4 encoder | 4176, 0 |
| TQ2_0 encoder | 1914, 0 |
| TQ1_0 encoder | 1566, 0 |
| IQ4_NL encoder | 4176, 0 |
| IQ4_XS encoder | 3944, 126 -- the known red gate, unchanged |
| tiny hybrid fixture gate | 5 checks, 0 failures (run earlier in the session) |

So the trace hook and the relink changed nothing in behaviour, and every gate that was green is still green. The single red gate is the
IQ4_XS encoder, which was red before this sweep and is recorded as such in its own experiment.

## Not part of this sweep

The state round trip (EXP293) and the quantized K/V A/B (EXP299) are heavier and were last verified when they were built; they are not
repeated here, and nothing in this sweep is claimed to replace them.
