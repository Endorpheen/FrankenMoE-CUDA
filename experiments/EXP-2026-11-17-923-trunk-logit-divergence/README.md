# EXP-2026-11-17-923: why the Goose trunk logits differ from llama.cpp

Status: in progress.
Base: EXP922, 2e68fa94.

## Finding this experiment starts from

EXP922 compared a composed Goose unit against llama.cpp's own tokenizer, model
graph and greedy choice. The token ids and the text agreed on every tested run,
including a stop on `<|im_end|>`, but the logits do not agree:

| comparison (one-token prompt "Hi") | mean abs diff | max abs diff | bitwise equal |
| --- | ---: | ---: | ---: |
| Goose vs native, Q4_0 KV | 0.1068 | 0.7492 | 0 / 248320 |
| Goose vs native, F32 KV | 0.0989 | 0.6374 | 0 / 248320 |
| native Q4_0 KV vs native F32 KV | 0.0769 | 0.5541 | 0 / 248320 |

A multi-token prompt gives mean 0.2010, max 1.3137.

The third row rules out "it is only the KV quantisation": native's own KV effect
is smaller than the Goose-vs-native gap, and Goose is not closer to native's
Q4_0 run than to native's F32 run.

## Hypothesis

The Goose trunk graph is a faithful transcription of what EXP918's fixture
believed the model computes, but it differs from llama.cpp's actual
Qwen3.5-MoE graph in at least one structural decision. Candidates, in the order
they can be tested cheaply:

1. how K/V are written into the Q4_0 cache (which op quantises, and on which
   layout) and how attention reads them back;
2. the attention branch: rope variant and sections, kq scale, mask semantics,
   flash-attention accumulation precision;
3. the GDN (recurrent) layers: convolution state update, gate ordering,
   normalisation placement;
4. the MoE block: router normalisation/scale, shared-expert placement, fused
   versus separate RMS-norm/SwiGLU.

## Method fixed before changes

Isolate by building the smallest comparison that separates one decision at a
time, using llama.cpp as the reference and keeping every measurement on the real
Apex weights:

1. single token, goose vs native, both F32 KV and both Q4_0 KV: already
   measured; the residual is ~0.10 with either, so KV is not the only cause;
2. per-layer isolation: compute the Goose frames for a one-token prompt and
   compare a layer's output against a native reference built from the same
   weights with llama.cpp's construction, one layer at a time, starting with
   layer 0 (a GDN layer);
3. once a layer matches, move to the next; the first divergent layer names the
   subsystem.

Acceptance: bitwise equality of the logits vector for a fixed prompt (or, if a
fused op makes bitwise impossible, equality of the tokens the profile produces
plus a stated, measured tolerance that the chain-level comparison can use).
No tolerance is chosen before the cause is known.

## Boundaries

CPU only, batch 1, mmap, no GPU, no server. The protected Apex bundle, the
baseline and the shipping runtime are untouched. Nothing here is a speed
result.

## Results

### A direct F32-KV comparison is not possible through the accepted module

The first isolation attempt switched the fixture's KV tensors to F32
(`GOOSE_EXP923_F32_KV=1`, a diagnostic switch added to the EXP922 fixture) to
separate the KV write/read path from the rest of the graph. The Goose trunk
refuses to build with F32 KV stores: `q35_trunk` expects the Q4_0 cache layout,
so the accepted module cannot answer this comparison without changing an
accepted module. Recorded rather than forced.

That leaves two ways forward, and they need different work:

1. read llama.cpp's `src/models/qwen35moe.cpp` and `src/models/delta-net-base.cpp`
   against `goose/model/qwen35-trunk.goose`, `qwen35-attention.goose`,
   `qwen35-gdn.goose` line by line and list every decision where the port
   could differ from the shipped graph;
2. build a one-layer GGUF from the real weights (metadata `block_count=1`, the
   layer-0 tensors plus embedding and head) so both implementations can be
   compared layer by layer on the real data without touching the accepted
   modules.

Work continues with (1) first, because it needs no new artifact and can name
the candidates before any more compute is spent.

### Source read: what was eliminated

Reading `src/models/qwen35moe.cpp`, `src/models/delta-net-base.cpp`,
`src/models/models.h` and `src/llama-graph.cpp` against
`goose/model/qwen35-gdn.goose`, `qwen35-attention.goose`,
`qwen35-trunk.goose` and `cpu/goose-graph-builders.goose` eliminated several
candidates:

| decision | native | Goose | verdict |
| --- | --- | --- | --- |
| GDN q/k norm | `rms_norm(x, eps/n)` then `scale(1/sqrt(n))` | `cfg[5]=eps/n`, `cfg[6]=1/sqrt(n)`, rms then scale | same |
| GDN delta rule | `ggml_gated_delta_net` (fused, default on) | `gated_delta_net` fused op | same |
| GDN beta/alpha/gate | sigmoid, softplus(+dt.bias), `* ssm_a` | same order | same |
| GDN output norm | `rms_norm(out, ssm_norm) * silu(z)` | same | same |
| MoE weights | softmax, top-k, `sum_rows`, clamp `6.103515625e-5`, div, optional scale | same | same |
| MoE accumulation | per-expert view + sequential add | same | same |
| expert order | `ggml_argsort_top_k` | `argsort` + view | probe: identical indices on a tie-heavy vector |
| weight before FFN | LLaMA4 only | not used | same |
| attention scale | `1/sqrt(256) = 1/16` (no attention.scale key in the GGUF) | 1/16 | same |
| rope packing | p1=n_dims, p2=mode, p4=n_ctx_orig, p5..p10 freqs, p11..14 sections | same layout | same |

Still open, and not separable by reading:

- the KV write and read path: the fixture stores K/V as `[hd*hk, capacity]` rows,
  llama.cpp uses a head-major `[hd, hk, capacity]` cache, and the quantisation
  path (which op converts to Q4_0) is not proven identical;
- the attention mask construction (`build_inp_mem_hybrid` versus the fixture's
  F16 0/-inf mask);
- CPU-backend fused variants llama.cpp may select at graph build time
  (e.g. rms_norm fused with the weight multiply) whose accumulation differs.

### Next step

Build a one-layer GGUF from the real weights (`block_count = 1`, layer-0
tensors, embedding, output norm and head) so both implementations can be
compared at the layer boundary on real data, without touching the accepted
modules and without guessing which of the open items matters.
