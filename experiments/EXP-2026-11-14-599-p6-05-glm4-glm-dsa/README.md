# EXP-2026-11-14-597 — glm4 and glm-dsa builders (P6-05 architectures 14 and 15)

Native counterparts: `src/models/glm4.cpp` and `src/models/glm-dsa.cpp` in the
pinned llama.cpp tree. Base commit: `a640bac9`. This is the P6-05 row 14
(glm4, S1 dense + nextn) and row 15 (glm-dsa, S3 MLA + MoE + indexer) of
`docs/goose-migration/P6-05-COVERAGE.md`.

## Hypothesis

The two families share exactly one structural decision — where the decoder
trunk ends and the NextN block begins — and differ in everything the plan and
the shape table have to say afterwards. One shared skeleton
(`source/base/trunk.goose`) plus one file per architecture therefore covers
both loaders without inventing a common "dense MLA" that neither family has,
and without duplicating the block split, the `n_layer_nextn < n_layer_all`
assert or the `2*n_embd` NextN projection.

The shared part, taken from the two loaders:

- `n_layer_all = n_layer + n_layer_nextn`; blocks below `n_layer` are the
  trunk, blocks at and past it are the NextN block;
- a file that declares as many NextN blocks as it has blocks is rejected;
- the NextN projection reads `2*n_embd` rows: the block concatenates the
  normalised token embedding and the normalised previous hidden state;
- `output` is optional and duplicates `token_embd` when absent.

The architecture-specific part:

| decision | glm4 | glm-dsa |
|---|---|---|
| attention | plain GQA through `create_tensor_qkv` | MLA: `wq_a`/`wq_b`, `wkv_a_mqa`, `wk_b`/`wv_b` |
| fused QKV row | yes, `q + k + v` | no such tensor |
| attention output | `wo` rows `n_embd_head_k * n_head` | `wo` rows `n_head * n_embd_head_v_mla` |
| extra attention rows | none | `attn_q_a_norm`, `attn_kv_a_norm`, 5 indexer rows |
| post-norms | `attn_post_norm`, `ffn_post_norm` on every layer | none |
| FFN | one fused SWIGLU row `{n_embd, 2*n_ff}`, no gate | dense SILU-PAR gate/up/down for the first `n_layer_dense_lead` layers, then routed experts + one shared expert |
| expert width | none | `n_ff_exp`; shared expert `n_ff_exp * n_expert_shared` |
| indexer | none | DSA lightning indexer, declared on every layer, full/shared per layer |
| NextN block | trunk rows kept with `TENSOR_SKIP`, never executed | full decoder block with its own MTP graph |

## What this slice covers

`source/glm4.goose`:

- the per-block plan (`g4_plan`, `g4_kind`) and the NextN assert
  (`g4_nextn_ok`);
- token embedding, output norm, optional output with the duplicate fallback;
- attention: query, key and value widths, the fused QKV row, fused and legacy
  bias sets, `attn_out` rows, `attn_post_norm`;
- FFN: the fused `ffn_up` row of width `2*n_ff`, `ffn_down`, `ffn_post_norm`,
  and the absence of any gate row;
- the NextN rows (`eh_proj`, `enorm`, `hnorm`, optional `embed_tokens`,
  `shared_head_head`, `shared_head_norm`), present only on a NextN block;
- the head-split assert marker (`n_embd_head_k == n_embd_head_v`).

`source/glm-dsa.goose`:

- the per-block plan and the NextN constraints (below `n_layer_all`, exactly
  one draft block);
- the FFN branch as an index decision: dense lead layers versus MoE layers,
  including the NextN block, plus the loader/graph disagreement check
  (`dsa_mtp_ffn_ok`);
- MLA: `wq_a`, `wq_b`, `wkv_a_mqa` (latent plus rope), `wk_b`, `wv_b`, `wo`,
  the q/kv norms and the nope width;
- the MoE branch: router, optional score bias, routed gate/up/down and the
  shared expert at `n_ff_exp * n_expert_shared`;
- the dense branch: gate, up, down at `n_ff`;
- the DSA indexer: the five tensor shapes, the 5.2 full/shared per-layer
  pattern, the pre-5.2 back-compat rule, the "shared layer must follow a full
  one" assert, the top-k clamp and the rope-inside-indexer-head assert;
- MLA presence and the half-declared pair assert, the gating default and the
  MoE dimension checks;
- the NextN rows.

`source/base/trunk.goose` is new shared code. `source/base/model_graph.goose`
is the accepted model-graph slice copied from the base commit (identical bytes
to `goose/model/base/model_graph.goose`, SHA-256
`8c83ca66a50322a20c76a13b55038b87646785d7fb7973531bdac15f70bd5fcc`); only
`n_ff_exp` is used from it.

## Gate

`tests/build.sh` builds two independent programs at `-O1` (the modules export
the same base helpers and are never linked together) and runs two structure
gates:

```
FG-ARCH14 cases=50 values=96 failures=0
FG-ARCH15 cases=94 values=170 failures=0
```

The expected values are a transcription of the loader declarations, with line
anchors in `tests/cases.md`. The gate proves the decisions and shapes; it does
not link the native library and does not execute a graph.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves: six in
`source/glm4.goose`, six in `source/glm-dsa.goose`. Each runs in its own
temporary tree at `-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Half 1 covers the fused SWIGLU factor, the fused QKV bias width, the `ffn_down`
direction, a plan that never reaches the NextN block, trunk blocks claiming the
NextN projection, and the post-norm width. Half 2 covers the dense `ffn_up`
width, the shared-expert multiplier, the rope block inside `wkv_a_mqa`, the
`wk_b` nope rows, the indexer projection width, and an off-by-one dense/MoE
boundary.

## Recorded limits

- The gate is structural. No native library is linked and no graph is
  executed: the oracle is a transcription of `glm4.cpp` and `glm-dsa.cpp`, so
  this proves builder decisions and shapes, not numeric execution.
- glm4's released geometries have `n_embd_head_k * n_head == n_embd` (GLM-4-9B:
  32 heads of 128 over 4096). The fixture therefore cannot tell the native
  `attn_out` row expression from `n_embd`, and no mutation relies on that
  difference; the code keeps the native expression, as the qwen2 slice does.
- For the same reason the key and value widths are numerically equal on every
  glm4 fixture (the graph asserts `n_embd_head_k == n_embd_head_v`); they are
  still computed from their own head counts and passed separately to the fused
  row, but the gate cannot separate them.
- The glm-dsa fixture declares `n_expert_shared = 2` so that the multiplier in
  the shared-expert width is observable. Released GLM-5.2 files carry one
  shared expert, where `n_ff_exp * n_expert_shared` and `n_ff_exp` are the same
  number and the gate would be blind to the difference.
- The `GLM_5_2_DEFAULT_INDEXER_TYPES` table is transcribed as a rule
  (`il < 2` or `il % 4 == 2` is full). That rule matches the 26 listed entries;
  if upstream changes the table the rule has to be re-checked. A per-file
  custom array is exercised only through `dsa_indexer_kind` /
  `dsa_indexer_ok` with the fixture's own array.
- `dsa_mtp_ffn_ok` records a disagreement between the loader and the MTP
  graph: the loader picks dense/MoE by block index, while the MTP graph always
  builds a MoE FFN and asserts `ffn_gate_inp`. Only the agreeing combination is
  planned; the failing combination is gated as a decision, not executed.
- RoPE parameters, `rope_sections` (glm4's mrope path), the attention scale,
  the model-type switches by `n_layer`, the indexer's Hadamard transform and
  the graph bodies (`build_attn`, `build_ffn`, `build_moe_ffn`) are outside
  this slice.
- The bias direction is covered as shapes and presence; which direction a real
  GGUF ships is a file property, not a builder decision.
- Only two parameter sets are exercised (one per family); other sizes differ
  only by the same arithmetic, but they are not gated.
