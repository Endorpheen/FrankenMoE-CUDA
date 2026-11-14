# EXP-2026-11-14-596 — gemma3 and gemma3n builders (P6-05 architectures 16 and 17)

Native counterparts: `src/models/gemma3.cpp` and `src/models/gemma3n.cpp` in the
pinned llama.cpp tree. Base commit: `a640bac9`. This is the P6-05 row 16
(gemma3, S5 SWA) and row 17 (gemma3n, S5 PLE) of
`docs/goose-migration/P6-05-COVERAGE.md`.

## Hypothesis

Both families share one trunk skeleton and differ in local decisions: gemma3
has conditional SWA, a type switch that changes the attention scale rule,
optional dense heads and conditional softcapping; gemma3n makes SWA
unconditional, fixes the attention scale at 1.0, splits the KV cache into a
20-layer prefix, and adds altup, laurel and per-layer embeddings. One shared
skeleton (`source/base/gemma.goose`) plus one file per architecture therefore
reproduces the loader plan and every tensor shape both builders declare,
without duplicating the plan and the QKV geometry.

The shared part, taken from the two native loaders and `llama-hparams.cpp`:

- the plan is uniform: every trunk layer is a full-attention layer, with no
  hybrid interval, no recurrent branch and no MTP layer;
- the SWA mask is `period == 0 || il % period < period - 1` (dense-first is
  false for both families);
- the fused QKV row is `q_width + k_width + v_width`;
- the GQA key/value width is `n_embd_head_v * n_head_kv`;
- the FFN is gated `n_ff`, with gate/up `{n_embd, n_ff}` and down
  `{n_ff, n_embd}`;
- `output` is optional and duplicates `token_embd` when absent;
- token inputs are scaled by `sqrt(n_embd)`, raw embedding inputs are not.

The architecture-specific part:

| decision | gemma3 | gemma3n |
|---|---|---|
| SWA type | STANDARD only when `n_swa > 0` | always STANDARD |
| SWA period default | 6 | 5 |
| attention scale | 27B: `1/sqrt(n_embd/n_head)`, else `1/sqrt(n_embd_head_k)` | fixed `1.0f` |
| KV cache | every layer | first 20 layers; later layers reuse it |
| V norm | none in the graph | weightless RMS, no tensor |
| q/k norm | per head, `n_embd_head_k` | per head, `n_embd_head_k` |
| altup | none | `n_altup` states, router, predict/correct coefficients |
| laurel | none | low-rank `{n_embd, rank}` / `{rank, n_embd}` |
| per-layer embeddings | none | token table, model projection, proj norm, inp gate |
| activation sparsity | none | first 10 layers |
| dense heads | optional `dense_2_out`, `dense_3_out` | none |
| final softcap | only when the key is non-zero | unconditional |

## What this slice covers

`source/gemma3.goose` is the plan/shape/decision half of the gemma3 builder:

- the uniform plan (`g3_plan`) and the per-layer SWA mask (`g3_is_swa`);
- the SWA type decision, the period default, the graph template choice
  (`g3_graph_iswa`) and the per-layer rope base source;
- the type switch by `n_layer` and the 27B attention-scale rule;
- token embedding, output norm, optional output with the duplicate fallback,
  optional `dense_2_out` / `dense_3_out`;
- attention: fused QKV weight, `wo` rows from the query block, per-head qk-norm
  widths, attention and FFN post norms;
- FFN gate/up/down shapes, GELU-PAR marker, conditional softcap;
- the input-embedding scale decision and the boundary that the shared auto
  scale branch never fires.

`source/gemma3n.goose` covers the same trunk plus:

- unconditional SWA with period 5 and the SWA mask;
- the 20-layer KV prefix and the `has_kv` decision, including its `-1` branch;
- the fixed 1.0f attention scale and the shared auto-branch boundary;
- qk-norm and the weightless V norm marker;
- altup: `{n_embd, n_embd, n_altup-1}` projections, correct/predict
  coefficients, router, router norm, correct scale, merge divisor and active
  index;
- laurel: `l`, `r` and post norm shapes;
- per-layer embeddings: token table, model projection, projection norm,
  per-layer inp gate, projection and post norm;
- activation sparsity over the leading 10 layers.

`source/base/gemma.goose` is new shared code: the plan, the SWA mask, the GQA
width, the fused-QKV width, the output-duplicate decision and the embedding
scale decision. `source/base/model_graph.goose` is the accepted model-graph
slice copied from the base commit (identical bytes to
`goose/model/base/model_graph.goose`); its `kq_scale_auto` is used to prove
that neither builder falls into the automatic attention-scale branch.

## Gate

`tests/build.sh` builds two independent programs at `-O1` (the modules export
the same helper names and are never linked together) and runs two structure
gates:

```
FG-ARCH16 cases=50 values=116 failures=0
FG-ARCH17 cases=62 values=157 failures=0
```

The expected values are a transcription of the loader declarations, with line
anchors in `tests/cases.md`. The gate proves the decisions and shapes; it does
not link the native library and does not execute a graph.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves: six in
`source/gemma3.goose`, six in `source/gemma3n.goose`. Each runs in its own
temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Half 1 covers SWA detection, an off-by-one SWA mask, the query width, the
key/value width, the 27B scale rule and the conditional softcap. Half 2 covers
the KV prefix boundary, the altup projection depth, the per-layer table width,
the laurel rank, the fixed attention scale and the activation-sparsity
boundary.

## Recorded limits

- The gate is structural. No native library is linked, no graph is executed:
  the oracle is a transcription of `gemma3.cpp` / `gemma3n.cpp`, so this
  proves builder decisions and shapes, not numeric execution.
- The SWA mask is checked as a decision table. The graph itself (which KV
  cache view each layer consumes, `ggml_rope_ext` arguments) is outside this
  slice.
- RoPE parameters (bases, scales, rotation dims) and attention arithmetic are
  not covered; only the choice of which rope base an SWA layer uses is.
- Float constants (softcap 30.0, altup/laurel/embedding scales, sparsity
  multiplier `1.6448533535003662f`) are covered as presence/bit-pattern
  decisions, not as arithmetic on the values.
- altup and laurel are covered as tensor shapes and state-count decisions.
  The per-token math (`calc_magnitude`, router tanh, coefficient broadcast) is
  graph body, not plan.
- `n_altup`, `i_altup_act`, `laurel_rank`, `n_embd_altup` are hparams defaults
  in this tree (`llama-hparams.h:243-246`); the slice takes them as parameters
  and does not read the document keys.
- The type switches (`LLM_TYPE_*`) are model-type labelling; only the 27B
  scale rule consumes the type, and that rule is gated.
- One parameter set per family is exercised (gemma3 4B, gemma3n E4B); other
  sizes differ only by the same arithmetic, but they are not gated.
- Two optional tensors with non-zero features are exercised (gemma3
  `dense_2_out`, `dense_3_out`); sizes that ship them differently are a file
  property, not a builder decision.
- Both fixtures use equal key and value head widths (256/256), so a swap
  between `n_embd_head_k` and `n_embd_head_v` inside the width helpers would
  not be distinguishable; the code keeps the native expressions and no mutation
  relies on that difference.
