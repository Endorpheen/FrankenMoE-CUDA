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

### Measured: n_kv_max does not change the result

A probe builds three identical flash-attention nodes over the same Q4_0 K/V and
F16 mask in the permuted layout llama.cpp uses, with `n_kv_max` unset, set to the
row count, and set to 256. All three produce bitwise-identical output on CPU, so
`ggml_flash_attn_ext_set_n_kv_max` is not the cause and that hypothesis is
closed.

With the gate zeroed the difference also persists, and with V zeroed it
disappears, so the disagreement is the V values the attention node reads - not
the gate, not the kernel's algorithm selection, and not the bytes in the store
(which are canonical). What is left is the *geometry* the module gives that node:
the permuted view of a quantised tensor, and the strides it derives for the K/V
cells.

### The view geometry reads correctly

`goose/model/qwen35-kv.goose` (q35_attn_cached) builds the K/V operands as
`view4(cache, head, n_head_kv, read_rows, 1, head_bytes, row_bytes, stream_bytes, 0)`
with `head_bytes = row_size(Q4_0, head) = 144` and `row_bytes = row_size(Q4_0,
width) = 288` for this model. For a Q4_0 store of `[512, capacity]` that is the
correct geometry: head 0 at offset 0, head 1 at 144, the next cell at 288. The
strides are therefore not obviously wrong, and `ne[3] = 1` makes `stream_bytes`
inert.

So the remaining suspects inside the attention output are narrower than "the
geometry": either the flash-attention kernel's quantised V handling on CPU, or
the exact operand the module hands it. Both need the node's own output to
compare, not its inputs.

### The kernel is exact too

A direct probe of `ggml_flash_attn_ext` with Q4_0 K/V, F32 Q, an F16 mask with a
single valid cell and the permuted layout: the output equals the dequantised V
cell for both heads with **0 mismatches out of 512 values**. So on CPU the op
reads quantised V exactly as a manual dequantise would.

That closes every single piece of the V path in isolation: the projection, the
canonical store bytes, the view geometry, and the kernel. The chain still
disagrees end to end, which means the difference is introduced between those
pieces - most plausibly in what the graph *passes* to the op rather than in the
pieces themselves.

### The manual branch hit the GQA ratio

Rebuilding the attention branch by hand in the fixture with
`out = V * sigmoid(gate)` aborted on `ggml_can_repeat`: the gate has
`n_head * head_dim = 16 * 256 = 4096` elements while V has
`n_head_kv * head_dim = 2 * 256 = 512`. This model is GQA 8:1, so a
single-token attention output has one head per *query* head, with each query head
taking the V of its KV group - the hand-built branch must expand V accordingly,
which is exactly the step the flash-attention node performs internally.

That is a useful confirmation in itself, and it sharpens the remaining question:
the difference must be in how the 16 query heads map onto the 2 V heads, either
inside the FA node (its K/V operand views) or in the gate's head order, because
every input to that mapping is already proven exact.

### The hand-built branch is bitwise the Goose path, and native is the outlier

With the GQA expansion in place (query head `h` takes V head `h/8`), the
hand-built branch - dequantised store -> gate view -> expansion -> `wo` ->
residual -> post norm -> final norm -> head - gives:

    manual vs Goose : max diff 0,        0 of 248320 differ
    manual vs native: max diff 1.3537,   248320 of 248320 differ

So the Goose attention branch is bitwise what an independent construction from
the same bytes produces, and it is native that disagrees. That flips the framing
of this whole experiment: it is no longer "the Goose trunk diverges" but "the
Goose branch and a straightforward construction agree, while llama.cpp's graph
differs from both".

Therefore the next question is what llama.cpp does that this construction does
not. Candidates worth checking first, because they are the only places where the
two differ by construction:

- the V it attends to may not be the cell this construction dequantises (a
  different cell index or a transposed write would still be self-consistent
  inside llama.cpp);
- its gate may be taken from a different half or head order than the view this
  construction reproduces;
- its attention may dequantise V through F16 rather than F32.

### The difference is in the branch, not the head

Comparing the post-`output_norm` hidden state (`llama_get_embeddings_ith`, 2048
values) between the hand-built branch and native:

    mean |d| 0.2211   max |d| 0.9208   bitwise 0 of 2048

So the disagreement is already present before the LM head, i.e. in the
attention/residual branch itself, and the head is exonerated.

The first attempt to explain it as native dequantising V through F16 (a variant
that casts the dequantised V to F16 and back before the gate multiply) produced
no output in this session and is left recorded as not yet measured rather than
guessed.

### F16-V and gate order are both closed

    F16-V variant   : manual vs native max 1.3507  (gate kept)
    gate-zeroed model: manual vs native max 1.3665 (gate = 0.5 on both sides)

Neither explains it. A kernel that dequantises V through F16 would have moved
the hand-built branch towards native; it does not. With the gate half zeroed the
gate order cannot matter, and the disagreement is unchanged.

So of the three candidates only one is left: **which V native attends to**. Every
other input to that branch has been checked one by one and matches, and the
hand-built branch is bitwise the Goose path.

### The quantisation round trip carries almost all of it

Removing the Q4_0 round trip from the hand-built branch (V straight from the
projection) and comparing against native with `type_v = F32`:

    exact-V branch vs native F32-KV : max 0.0746   (248318 of 248320 differ)
    exact-V branch vs Goose Q4-V    : max 0.8240
    quantised-V branch vs native Q4 : max 1.3537   (from before)

So the Q4 round trip is the dominant term: with it removed the disagreement falls
by more than an order of magnitude. What remains (0.075) is the residue of native
still using its own F32 cache path rather than a hand computation.

That is a sharper statement than "the V path differs": both sides quantise the
same 512 values with the same block size and the same writer, yet their results
diverge as if the quantised values differed. The next artefact must therefore be
native's own cell, not goose's.

### The scan needs a faster tool

Searching native's 2196156-byte state blob for a 288-byte Q4_0 window matching
the exact V (512 values) is too slow in Python at a two-byte stride; the attempt
timed out twice and is recorded as not yet done rather than as a negative result.
The planned artefact is a C scanner, or a targeted search for the first block's
16-bit scale, which is a two-byte pattern.

The earlier exact-byte search is still valid for what it covered: goose's stored
K and V cells and every 64-byte chunk of them do not appear anywhere in native's
state blob for the same model, so the two sides do not store identical bytes.

### Scan result: native's state does not contain goose's V cell

A C scanner (two-byte stride over the 2196156-byte state of the *matching*
diagnostic model) looked for a Q4_0 window matching the exact V (512 values,
absmax 1.767, mean abs 0.424), both as one 288-byte window and as two head-major
144-byte windows:

    best 288-byte window : mean abs err 0.267
    best head-major (h0) : mean abs err 0.401
    best head-major (h1) : mean abs err 0.441

Nothing even close, so native's serialized state does not contain goose's V in
either layout. That does not yet prove the values differ - a serialisation may
re-pack cells - but combined with the earlier exact-byte search it means the
state blob cannot be used as the comparison artefact.

### The quantitative clue: native's quantisation barely perturbs anything

Reading the measured numbers as perturbation sizes on the same model and prompt:

| what changed | logits max |d| |
| --- | ---: |
| exact V vs Goose's Q4_0 V (my round trip) | 0.8240 |
| native F32-KV vs native Q4_0-KV (its round trip) | 0.1462 |
| exact V vs native F32-KV (residual difference) | 0.0746 |
| exact V vs native Q4_0-KV | 1.3507 |

Native's own Q4_0 cache perturbs its logits about **5.6x less** than my Q4_0 store
perturbs mine, even though both use the same quantiser on the same values. That
is what "the two tables disagree" looks like when the cache types are not
actually the same in effect: native's V reaches attention at a precision closer
to F16/F32 than to Q4_0, while mine is genuinely Q4_0.

This explains the earlier anomaly (Goose looked *closer* to native's F32 run than
to its Q4_0 run) and it is the first hypothesis that fits every measurement: the
Goose cache is quantised as declared, and native's is not, in the way the
attention reads it.

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
5. compare the attention node's *output* rather than its inputs: have the
   fixture hand the graph's flash-attention result back (the module can be asked
   to return that node through the existing `nodes` array, or the fixture can
   recompute the same node over the same operands), and compare it against a
   manual dequantise-V-and-copy for the single-token case, where attention is
   exactly V;
6. compare the *final hidden state* rather than the logits: llama.cpp exposes
   `llama_get_embeddings_ith`, which is the post-`output_norm` tensor, so the
   same construction can be compared one step earlier (before the head) and the
   difference can be attributed to the branch or to the head;
7. check what precision native actually feeds to attention when `type_v` is
   Q4_0: read `llama-kv-cache`/`build_attn` for a dequantise-to-F16 step before
   `ggml_flash_attn_ext` (the fork casts only *F32* V to F16 explicitly), and
   confirm by building a model whose V weights make coarse quantisation obvious:
   if native's result tracks the exact V within F16 error, its attention never
   sees Q4_0 V.
