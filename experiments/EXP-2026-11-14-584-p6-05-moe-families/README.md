# EXP-2026-11-14-581 — MoE family builders (P6-05 architectures 4, 7, 13)

Native counterparts: `src/models/qwen3moe.cpp`, `src/models/qwen2moe.cpp` and
`src/models/glm4-moe.cpp` of the pinned llama.cpp tree. Base commit:
`4dac50f2`. This one experiment carries three independent MoE slices; each has
its own source file, its own fixture binary and its own gate line.

## What these slices cover

All three files are the plan/shape half of a builder, not the graph emitter.
They reuse the accepted common module
`source/base/model_graph.goose` (the canonical
`goose/model/goose-model-graph.goose` without its `fn main`) for `n_ff_exp`,
`n_ff_shexp`, `key_dim` and `value_dim`.

- `source/qwen3moe.goose` — architecture 4. One layer kind: dense attention
  with q/k RMS norm followed by routed experts. Covers the GQA key/value
  widths, the head-sized query block, the fused QKV width, `wo`, the router
  projection and the three expert matrices, plus the output policy (an absent
  output is duplicated from `token_embd`).
- `source/qwen2moe.goose` — architecture 7. One layer kind: attention with no
  q/k norm, routed experts and one shared expert on every layer. Covers the
  `n_embd`-wide query block, the square `wo`, the separately declared
  shared-expert width and the mandatory output.
- `source/glm4-moe.goose` — architecture 13. Three layer kinds: dense lead
  blocks, routed MoE blocks and one MTP block (`n_layer_all` loop bound). Covers
  the dense/MoE/MTP plan, `attn_post_norm` in place of `ffn_norm`, optional q/k
  norm, the expert-probability bias, the shared-expert width
  `n_ff_exp * n_expert_shared`, the doubled `nextn.eh_proj` rows and the mrope
  decision.

## Gate

`tests/build.sh` builds each module with `--standalone` at `-O1`, links it with
its own fixture and runs ten structure cases per family:

```
tests/build.sh            # all three, one after another
tests/build.sh qwen3moe   # one family only
```

```
FG-ARCH4  cases=10 values=78 failures=0
FG-ARCH7  cases=10 values=58 failures=0
FG-ARCH13 cases=10 values=78 failures=0
```

The gate checks the plan and shapes only. It does not link a native library:
the expectations are a transcription of the three native builders, so it
proves the decisions and shapes, not native numeric execution. Cases and
parameters are listed in `tests/cases.md`.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves, four per family;
each runs in its own temporary tree and builds only the affected family at
`-O0` while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

qwen3moe: fused QKV width, routed-expert width, `wo` width, `ffn_down_exps`
orientation. qwen2moe: fused QKV width, routed-expert width, shared-expert
width, `wo` width. glm4-moe: dense-lead boundary, shared-expert multiplicity,
`nextn.eh_proj` doubled width, routed-expert width fallback.

## Recorded limits

- Structural gate only: no native library is linked and no graph is executed.
- One shared 21-slot parameter layout serves all three fixtures; values not
  used by a family are zero and documented in `tests/cases.md`.
- The cases leave some tensors uncovered (see "Recorded limits" in
  `tests/cases.md`), and the shared-expert activation path is graph
  construction, outside this slice.
- `build/` and `build-mut/` hold reproducible artifacts (generated C, objects,
  temporary mutation trees) and are not part of the evidence.
