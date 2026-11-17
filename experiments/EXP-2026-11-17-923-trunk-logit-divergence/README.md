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

### Measured: the divergence is in the attention branch

A one/two-block GGUF was generated from the real weights (metadata copied, only
the chosen blocks' tensors kept, block indices renumbered; `tests/make_layer_gguf.py`).
A single-block model cannot be loaded by llama.cpp (its hybrid memory asserts
when there is no attention cache), so the diagnostic models keep one recurrent
and one full-attention block.

| model | mean abs diff | max abs diff | bitwise equal |
| --- | ---: | ---: | ---: |
| 2 blocks (GDN 0 + attention 3), real weights | 0.1649 | 1.0876 | 0 / 248320 |
| same, GDN block zeroed | 0.2921 (argmax differs) | - | 0 / 248320 |
| same, attention branch of block 1 zeroed (norms, MoE, residuals, head remain) | **0.0** | **0.0** | **248320 / 248320** |

The third row is the result: with the attention branch's own weights zeroed, the
two implementations agree bit for bit over the whole vocabulary. The MoE block,
both RMS norms, the residual chain, the final norm and the LM head are therefore
already exact, and the divergence lives entirely in the attention branch.

The prompt is a single token ("Hi"), so attention reduces to `wv` -> gated by
`sigmoid(qgate)` -> `wo` with softmax over one position; the difference appears
already there, which points at the V projection's write/read through the Q4_0
cache, the q-gate split, or the output projection - not at KV growth with
context.

### Measured: the difference is the V value path

Continuing the same ablation on the attention-only model (one token, so
attention is `wv -> sigmoid(qgate) -> wo` with a single-position softmax):

| model | goose vs native (mean abs) | bitwise equal |
| --- | ---: | ---: |
| attention-only (GDN and MoE zeroed) | 0.1723 | 0 / 248320 |
| same, but the gate half of `attn_q` zeroed (sigmoid(gate) = 0.5 for both) | 0.2064 | 2 / 248320 |
| same, but `attn_v` zeroed | **0.0** | **248320 / 248320** |

Zeroing the gate half does not help, so the q-gate split is not the cause.
Zeroing `attn_v` makes the two implementations identical again. Combined with
the earlier result (attention zeroed -> identical), the whole divergence is in
the path that carries V: the projection output, its write into the Q4_0 KV
cache, and the read back through attention. Everything else - q, k, both head
norms, rope, the gate, `wo`, the MoE, the residuals, the final norm and the head
- is already exact.

The F32 V values themselves cannot be the difference: the same ablation shows
`mul_mat` on Q4_K weights is exact, and zeroing only `attn_v` zeroes both sides.
That leaves the KV write/read path: which op writes V, in which layout, and with
which quantiser.

### Byte level: the stores hold one cell, and the bytes differ

Two new dumps on the two-block model, one token:

- the fixture writes every persistent store tensor after the step
  (`kvbytes` mode): layer 0 slots 0/1 are the GDN conv/state (F32), layer 1
  slots 2/3 are K and V (`GGML_TYPE_Q4_0`, 589824 bytes each), and only the
  first 288 bytes of each K/V store are non-zero, i.e. exactly one cell;
- llama.cpp's `llama_state_seq_get_data` for the same context writes a
  2196156-byte blob (`state` mode).

Searching the native blob for goose's first K cell (288 bytes) and its first V
cell: **not found**, and no 64-byte chunk of either appears either. So the two
sides do not store the same bytes, which rules out "the write is identical and
only the read differs".

What that leaves, precisely:

1. the V (and K) F32 values feeding the store differ, even though the projection
   matmul is exact on this model - the remaining way that can happen is a
   different *slice* of the projection output being stored, or a different
   reshape/permute before the write;
2. the write op quantises differently - e.g. a different block alignment or a
   cast through F16 before Q4_0.

The next test decides between them without guessing: on the attention-only model
the layer-1 input is the token embedding (block 0's weights are zero), so the
fixture can compute `v = wv @ (attn_norm(embedding))` itself, quantise it with
`ggml_quantize_chunk(Q4_0)` and compare those bytes directly with what the graph
wrote into the store.

### The V write is canonical; the difference is the attention call

Recomputing V inside the fixture on the attention-only model (`v = wv @
(attn_norm(embedding))`, layer 1's input is the embedding because block 0 is
zeroed) and quantising it with `ggml_quantize_chunk(Q4_0)`:

    prefix_equal = 288 of 288 bytes

The graph's stored V is byte-identical to canonical Q4_0 quantisation of the
straightforwardly recomputed V. A separate probe also shows `ggml_cpy` and
`ggml_set_rows` produce exactly the same bytes as `ggml_quantize_chunk` for the
same values, so all three writers agree.

An earlier run of this check reported a 3-ulp scale difference; that was a bug in
the check itself (it passed `sqrt(1+eps)-1` instead of `eps` to the norm). The
corrected run is the one above.

So the write path is right and the remaining difference is on the read side, in
how attention is invoked. Reading `src/llama-graph.cpp` against
`goose/model/qwen35-attention.goose`, the native call does two things the Goose
module does not:

- `ggml_flash_attn_ext_set_n_kv_max(cur, n_kv_max)` - the kernel is told how many
  KV cells the cache can hold, which selects its algorithm;
- `ggml_flash_attn_ext_add_sinks(cur, sinks)` - no sinks for this model, so this
  one is inert here.

`ggml_prec_set_acc(fa, GGML_PREC_F32)` is set by both.

### Next step

Continue inside the attention branch, using the same ablation method:

1. zero the MoE tensors of the attention block (keep its attention) - if the
   difference stays, the attention branch alone carries it;
2. zero only `attn_v` - the attention output becomes zero if the difference
   disappears, the V path is the cause;
3. zero only the q-gate half's consumer (`attn_output`) - separates the gate
   split from the projections;
4. dump the bytes the Goose fixture wrote into the Q4_0 V store and compare them
   with `ggml_quantize_chunk(Q4_0)` over the exact V values (both computed inside
   the fixture from the same weights), to decide whether Goose stores V
   differently or llama.cpp reads a different cache layout;
5. measure whether `ggml_flash_attn_ext_set_n_kv_max` changes the result: build
   two identical flash-attention nodes over the same Q4_0 K/V and F16 mask, one
   with `n_kv_max` set to the row count and one without, and compare the output
   bits. The first probe attempt used non-GQA shapes and tripped
   `ggml_can_mul_mat`, so it is being redone with `n_head` a multiple of
   `n_head_kv`.
