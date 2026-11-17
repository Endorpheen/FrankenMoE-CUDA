# EXP-2026-11-17-936 — Maxwell shapes and layer plan in Goose

Status: ACCEPTED

## Hypothesis

The canonical Goose architecture module (`goose/model/qwen35moe.goose`) owns the
shape/decision half of the builder, while the graph emitter stays host-side.
Maxwell was missing that half: its tensor shapes and per-layer kind lived only
in C++. Moving them to Goose closes the canon gap without adding runtime C++.

## What Goose owns now

`source/runtime.goose`:

- `mx_n_tensors()` — 35 tensor ids per Maxwell layer (31 trunk + 4 MBSA indexer);
- `mx_layer_kind(il, lead)` — dense FFN on the leading blocks, routed MoE after;
- `mx_shape(tid, p, out4)` — ggml-order shapes for embeddings, output, attention
  (Q 32x128, K/V 4x128, O), QK norms, both mHC triplets (fn [hc*n_embd, 24],
  base [24], scale [3]), dense FFN, routed experts ([n_embd, n_ff, n_expert]
  and [n_ff, n_embd, n_expert]), shared expert, and the learned HC head;
- `mx_plan(p, kinds)` — the 45-layer plan (3 dense, 42 routed).

## Gate

`bash tests/build.sh` → `checks=113 failures=0`, `MAXWELL-SHAPES OK`.

Checked on the real Hyperparameters (hidden 2048, 32/4 heads x128, FF 6144,
expert FF 512, shared 1024, 256 experts, 45 layers, 3 dense, 4 streams): every
shape above, the plan, and the tensor count.

## Accounting

- C++ runtime: **+0 / -0** (only the test fixture is C++, it is not runtime);
- Goose: ~205 lines in `source/runtime.goose`;
- Moved under Goose control: Maxwell tensor shapes and layer-kind plan.
