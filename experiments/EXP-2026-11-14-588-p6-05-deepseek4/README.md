# deepseek4 builder (P6-05 architecture 9)

Native counterpart: `src/models/deepseek4.cpp` (DeepSeek-V4 family: MLA with a
grouped low-rank output, compressor-based attention variants, lightning
indexer, hash routing, hyper-connections, shared experts, MTP head). Base
commit: `5ab757e0f6e7a63d5bc9832286d75bcc44a1f934`.

## What this slice covers

`source/deepseek4.goose` is the plan/shape half of the builder, not the graph
emitter. It owns, family by family:

- **plan** — `ds4_layer_kind` / `ds4_plan`: one kind per layer from the model
  file's compress ratio (`0` raw, `4` CSA, `128` HCA), the MTP block at and
  past `n_layer`, the "compress ratios shorter than block count" check, and the
  rejection of any other ratio. `ds4_ffn_kind` records that this architecture
  has no dense FFN layer at all: routed experts plus one shared expert exist on
  every layer, trunk and MTP.
- **mla** — low-rank query (`wq_a` → `attn_q_a_norm` → `wq_b`), the single KV
  head, its norm, the grouped low-rank output (`wo_a` reshaped to
  `[n_head*n_embd_head/o_groups, o_lora_rank, o_groups]`, `wo_b`), the
  `n_head % n_groups` check and the rotary/nope head split.
- **moe** — router width, three routed-expert matrices with the declared
  `n_ff_exp`, and the routing kind: layers below `hash_layer_count` get a
  `[n_expert_used, n_vocab]` token-id table instead of a `[n_expert]` score
  bias. `ds4_gating_ok` keeps the loader's sqrtsoftplus-only rule.
- **shexp** — one shared expert whose width is `n_ff_exp * n_expert_shared`,
  added to the routed output on every layer (`deepseek4.cpp:1404-1416`).
- **nextn** — the MTP probe (`blk.<n_layer_main>.nextn.eh_proj.weight` must
  exist or the block is dropped), the doubled `eh_proj` input because the
  hidden state is `hc*n_embd` wide, `enorm`/`hnorm`, the optional own embedding
  and the optional shared head; the MTP layer also carries the SWA marker and
  refuses hash routing.
- **indexer** — present only on ratio-4 layers: `indexer_proj`,
  `indexer_attn_q_b`, the four compressor tensors, the top-k clamp
  `min(score ne0, indexer_top_k)`.
- **compressor** — the overlap decision `coff = 2` for CSA and `1` for HCA, the
  gated `wkv`/`wgate` projections, the absolute positional table and the norm;
  the family is absent on raw and MTP layers.
- **sinks** — one attention sink per query head, carried by all four kinds
  (raw, CSA+LID, HCA, MTP).
- **hc** — hyper-connection multiplicity, `hc_mix_dim = (2+hc)*hc`, the head
  mixer `[hc*n_embd, hc]`, the effective output width (and its fallback), the
  `hc == 4` restriction and the `n_embd_out() == n_embd*hc` MTP check. Both
  per-layer modules (attn and ffn) are exposed with the same widths.

`source/base/model_graph.goose` is the accepted shared slice without its
`fn main` (canonical file `goose/model/goose-model-graph.goose`); the builder
imports it for `n_ff_exp`.

## Gate

`tests/build.sh` builds both modules with `--standalone` at `-O1` and runs 100
structure cases:

```
FG-ARCH9 cases=100 values=188 failures=0
```

Cases and parameters are listed in `tests/cases.md`. The gate checks the plan
and shapes only. It does not link a native library: the expectations are a
transcription of `deepseek4.cpp`, so it proves the decisions and shapes, not
native numeric execution.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves; each runs
in its own temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Mutations cover the MTP plan boundary, the ratio-4 kind, the CSA overlap
coefficient (decision and width), the hash-routing boundary, the HC mixer
width, the MLA query width, the `wo_a` group split and group dimension, the MTP
input width, the sink width and the MTP probe direction.

## Recorded limits

- **Structural only.** The gate compares the plan and the shape table against
  a hand transcription of the native loader. No native library is linked, no
  GGUF is loaded, and the graph is never executed, so no numeric claim is made.
- **Synthetic fixture.** The parameter vector and the compress-ratio array are
  a plausible DeepSeek-V4-sized configuration, not a real checkpoint header.
  The gate is sensitive to the rules, not to that particular file.
- **Abort paths are not exercised.** A short `compress_ratios` array, an
  unknown ratio inside `ds4_plan`, and the MTP hash-routing assert all call
  `fail`, which the fixture records but does not provoke: only the decision
  functions (`ds4_ratio_kind`, `ds4_mtp_route_ok`, `ds4_mtp_ratio_ok`) have
  negative cases.
- **Not covered by cases.** `ffn_exp_probs_b_vl` (optional vision routing
  bias) has no case; `nextn.embed_tokens` and `nextn.shared_head_head` shapes
  are checked but their "absent means fall back to the model tensors" decision
  is not; `n_layer_nextn == 1` (the native restriction) is not asserted here.
- **KV/compressor state is out of scope.** The `llama_dsv4_comp_state` tensors,
  the CSA state restore/snapshot indices and the KV cache planes belong to the
  KV subsystem, not to this builder slice.
