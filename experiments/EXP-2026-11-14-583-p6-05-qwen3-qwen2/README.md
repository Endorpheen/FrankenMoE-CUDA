# EXP-2026-11-14-586 — qwen3 and qwen2 dense builders (P6-05 architectures 3 and 6)

Native counterparts: `src/models/qwen3.cpp` and `src/models/qwen2.cpp` in the
pinned llama.cpp tree. Base commit: `4dac50f2`. This is the P6-05 row 3 (qwen3,
S1 dense) and row 6 (qwen2, S1 dense) of `docs/goose-migration/P6-05-COVERAGE.md`.

## Hypothesis

Both families are the same dense transformer and differ only in four local
decisions. One shared skeleton (`source/base/dense.goose`) plus one thin file
per architecture therefore reproduces the loader plan and every tensor shape
that the builders declare, without duplicating the plan and QKV geometry.

The shared part, taken from the two native loaders and `create_tensor_qkv`:

- the plan is uniform: every trunk layer is a full-attention layer, with no
  hybrid interval, no recurrent branch and no MTP layer;
- the fused QKV row is `q_width + k_width + v_width`;
- the GQA key/value width is `n_embd_head_v * n_head_kv`;
- the FFN is gated SILU-PAR with `n_ff` as the inner width;
- `output` is optional and duplicates `token_embd` when absent.

The architecture-specific part:

| decision | qwen3 | qwen2 |
|---|---|---|
| query block | `n_embd_head_k * n_head` | `n_embd` |
| qk-norm | per-head RMS, width `n_embd_head_k` | none |
| `attn_wo` rows | query block | `n_embd` |
| attention bias | normally none, fused/legacy machinery present | fused `attn_qkv.bias` or legacy q/k/v |
| `attn_wo` bias | never declared | never declared by the loader |
| output bias | absent | optional `output.bias` |
| extra heads | optional `cls_out`, `dense_2_out` | none |

## What this slice covers

`source/qwen3.goose` and `source/qwen2.goose` are the plan/shape half of the
builders:

- the per-layer plan (`q3_plan`, `q2_plan`) and its uniform kind;
- token embedding, output norm, optional output/lm_head with the duplicate
  fallback, and the qwen3 optional reranker/embedding heads;
- attention: fused QKV weight and biases, output projection, and for qwen3 the
  per-head q/k norm widths;
- FFN: gate, up and down shapes;
- the qwen2 bias set, including the legacy split q/k/v bias direction and the
  absence of any `attn_out` bias;
- the qk-norm presence marker: `1` for qwen3, `0` for qwen2.

`source/base/dense.goose` is new shared code: the plan, the GQA width, the
fused-QKV width and the output-duplicate decision. `source/base/model_graph.goose`
is the accepted model-graph slice copied from the base commit (identical bytes
to `goose/model/base/model_graph.goose`).

## Gate

`tests/build.sh` builds two independent programs at `-O1` (the modules export
the same helper names and are never linked together) and runs two structure
gates:

```
FG-ARCH3 cases=30 values=96 failures=0
FG-ARCH6 cases=26 values=84 failures=0
```

The expected values are a transcription of the loader declarations, with line
anchors in `tests/cases.md`. The gate proves the decisions and shapes; it does
not link the native library and does not execute a graph.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves: six in
`source/qwen3.goose`, six in `source/qwen2.goose`. Each runs in its own
temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Half 1 covers the query-block width, the GQA key/value width, `attn_wo` rows,
the qk-norm width, transposed `ffn_down`, and a truncated plan. Half 2 covers
the qwen2 key/value width, the fused QKV bias width, the output bias width,
a claimed qk-norm tensor, transposed `ffn_up`, and a truncated plan.

## Recorded limits

- The gate is structural. No native library is linked, no graph is executed:
  the oracle is a transcription of `qwen3.cpp` / `qwen2.cpp`, so this proves
  builder decisions and shapes, not numeric execution.
- qwen2's query width is `n_embd`, while qwen3's is `n_embd_head_k * n_head`.
  For every released qwen2 size the two expressions are numerically equal, so
  the fixture cannot distinguish them and no mutation relies on that
  difference; the code keeps the native expression.
- The type switches (`LLM_TYPE_*`, `qwen3.cpp:5-16`, `qwen2.cpp:5-20`) are
  model-type labelling, not plan or shapes, and are not covered.
- RoPE parameters, attention scale (`1/sqrt(n_embd_head)`) and the graph body
  (norm placement, `ggml_rope_ext`, `build_attn`, `build_ffn`) are outside this
  slice.
- The bias direction is covered as shapes and presence; which direction a real
  GGUF ships is a file property, not a builder decision.
- Only two parameter sets are exercised (one per family); other sizes differ
  only by the same arithmetic, but they are not gated.
