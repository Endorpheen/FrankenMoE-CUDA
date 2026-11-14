# P6-05 coverage — modern text model architecture builders

Updated: 2026-11-14. Native tree: `src/models/*.cpp` of the pinned llama.cpp
plus the shared graph helpers (`src/llama-graph.cpp`), with the accepted
`goose/model/goose-model-graph.goose` slice as the common base.

Each architecture is one independent slice: the plan (per-layer kind), the
tensor shapes and the builder decisions. A slice is accepted only with a
structure gate and mutations.

| # | architecture | native | families / slices | state |
|---:|---|---|---|---|
| 1 | qwen35moe | `qwen35moe.cpp` | S2 MoE + S4 hybrid | **done (EXP577)** |
| 2 | qwen4exp | `qwen4exp.cpp` | S2 + S3 indexer + S4 hybrid + PLE | **done (EXP580)** |
| 3 | qwen3 | `qwen3.cpp` | S1 dense | **done (EXP583)** |
| 4 | qwen3moe | `qwen3moe.cpp` | S2 | **done (EXP584)** |
| 5 | qwen3next | `qwen3next.cpp` | S4 | **done (EXP589)** |
| 6 | qwen2 | `qwen2.cpp` | S1 | **done (EXP583)** |
| 7 | qwen2moe | `qwen2moe.cpp` | S2 | **done (EXP584)** |
| 8 | deepseek32 | `deepseek32.cpp` | S3 MLA + indexer | **done (EXP587)** |
| 9 | deepseek4 | `deepseek4.cpp` | S3 + compressor | **done (EXP588)** |
| 10 | kimi-k3 | `kimi-k3.cpp` | S3 + S4 | **done (EXP592)** |
| 11 | kimi-linear | `kimi-linear.cpp` | S3 + S4 | open |
| 12 | glm5-next | `glm5-next.cpp` | S3 + S4 + HC | **done (EXP591)** |
| 13 | glm4-moe | `glm4-moe.cpp` | S2 | **done (EXP584)** |
| 14 | glm4 | `glm4.cpp` | S1 + nextn | open |
| 15 | glm-dsa | `glm-dsa.cpp` | S3 | open |
| 16 | gemma3 | `gemma3.cpp` | S5 SWA | open |
| 17 | gemma3n | `gemma3n.cpp` | S5 PLE | open |
| 18 | gemma4 | `gemma4.cpp` | S5 + MoE | open |
| 19 | gemma4-assistant | `gemma4-assistant.cpp` | S5 + nextn | open |
| 20 | mistral3 | `mistral3.cpp` | S5 + MoE | open |

## qwen4exp — accepted

Source: `experiments/EXP-2026-11-14-580-p6-05-qwen4exp/`; canonical files
`goose/model/qwen4exp.goose` and the shared
`goose/model/base/model_graph.goose`.

The slice owns the per-layer kind decision (attention vs gated delta net on the
hybrid interval, the recurrent flag), fused QKV and GDN `wqkv` widths, indexer
projections, hyper-connection shapes, routed-expert width with `n_expert_used`,
the MTP doubled hidden state and the PLE marker. Gate: 10 structure cases /
26 values, 0 failures; mutations 12/12 in two halves.

## Open boundaries

- The gate is structural: it checks plan and shapes against a transcription of
  the native builder. It does not link the native library and does not execute
  the graph.
- `q4_plan`, `q4_is_ple`, PLE tensors, `indexer.k_proj`, shared experts and
  `nextn.*norm` have no cases yet; the recurrent branch has no positive case
  because the fixture passes zero flags.
- The shared base module is carried as `goose/model/base/model_graph.goose`
  (the canonical file without its `fn main()`), so both builders import the
  same bytes.
