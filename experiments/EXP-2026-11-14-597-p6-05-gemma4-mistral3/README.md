# EXP-2026-11-14-596 — Gemma 4, Gemma 4 assistant and Mistral 3 builders (P6-05 architectures 18, 19, 20)

Native counterparts: `src/models/gemma4.cpp`, `src/models/gemma4-assistant.cpp`
and `src/models/mistral3.cpp` of the pinned llama.cpp tree, with the per-layer
head helpers in `src/llama-hparams.cpp`. Base commit: `a640bac9`.

This one experiment carries three independent slices. Each has its own Goose
source file, its own fixture binary and its own gate line; nothing is shared
between them except the accepted common module and the parameter layout.

## What these slices cover

All three files are the plan/shape half of a builder, not the graph emitter.
They import the accepted `source/base/model_graph.goose` (the canonical
`goose/model/base/model_graph.goose` without its `fn main`) and reuse
`key_dim`, `value_dim` and `kq_scale_auto`.

- `source/gemma4.goose` — architecture 18. Two orthogonal per-layer axes are
  planned together: sliding-window vs full attention, and dense FFN vs MoE. The
  head width changes with the attention kind (`n_embd_head_k_swa` on SWA
  layers), so the query, GQA key/value and `wo` widths, the q/k norm width and
  the `rope_freqs` width all follow it. The dense FFN tensors exist on every
  layer, because on a MoE layer they are the shared expert; the expert group
  exists only where the loader found a router. The slice also covers the
  shared-KV boundary (`n_layer_all - n_kv_shared_layers`), the doubled merged
  expert `gate_up`, the declared-only expert width, the optional duplicated
  output and the per-layer embeddings (PLE) globals and per-layer tensors.
- `source/gemma4-assistant.goose` — architecture 19. One layer kind: every
  effective layer is a nextn block, and `n_layer_nextn` must equal
  `n_layer_all`. The draft hidden state is `n_embd` while its input is the
  target width `n_embd_backbone`, so `nextn_proj_pre` is
  `[2 * n_embd_backbone, n_embd]` (target embedding concatenated with target
  hidden state) and `nextn_proj_post` is `[n_embd, n_embd_backbone]`. There are
  no K/V projections at all: attention reuses the target cache. `out_scale` is
  required here, not optional, and `rope_freqs` again follows the SWA pattern.
- `source/mistral3.goose` — architecture 20. One attention shape with GQA, and
  two per-file FFN kinds selected by `n_expert`: the dense gate/up/down triplet
  with optional biases, or the router plus expert triplet with an optional
  shared expert of its own declared width. The expert width is plain `n_ff`,
  with no `n_ff_exp` and no `n_ff / n_expert_used` fallback. The fused
  `attn_qkv` tensor is optional (`q + k + v` wide) and the loader falls back to
  separate q/k/v; the output is optional and duplicated; rope factors are a
  single optional `rope_freqs` or a long/short pair for longrope files; the
  attention-temperature path adds a Q scaling node but no tensor shape.

## Gate

`tests/build.sh` builds each module with `--standalone` at `-O1`, links it with
its own fixture and runs the structure cases:

```
tests/build.sh                     # all three, one after another
tests/build.sh gemma4              # architecture 18
tests/build.sh gemma4-assistant    # architecture 19
tests/build.sh mistral3            # architecture 20
```

```
FG-ARCH18 cases=36 values=143 failures=0
FG-ARCH19 cases=24 values=80  failures=0
FG-ARCH20 cases=36 values=183 failures=0
```

Recorded run: `results/gate.log`. The gate checks the plan and the declared
shapes only; it does not link a native library, so it proves the decisions and
shapes, not native numeric execution. Cases, parameters and anchors are listed
in `tests/cases.md`.

## Mutations

`tests/mutate.py` holds 12 semantic mutations in two halves, four per family.
Each runs in its own temporary tree and builds only the affected family at
`-O0`, while the gate stays at `-O1`.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

Recorded run: `results/mutations.log`, one line per mutation in
`results/mutations.tsv`.

- gemma4: SWA head width, expert width taken from `n_ff / n_expert_used`,
  shared-KV boundary off by one, `rope_freqs` on the wrong layer set.
- gemma4-assistant: `nextn_proj_pre` loses the concatenated width, `out_scale`
  reported optional, draft SWA head width, `nextn_proj_pre` on every layer.
- mistral3: routed expert width divided by `n_expert_used`, fused QKV drops the
  value width, shared-expert width collapses to `n_ff`, longrope files still
  declaring the single `rope_freqs` tensor.

## Layout

```
source/gemma4.goose                     architecture 18
source/gemma4-assistant.goose           architecture 19
source/mistral3.goose                   architecture 20
source/base/model_graph.goose           imported common module (canonical bytes)
tests/build.sh                          three-family structure gate
tests/gate_common.h                     shared gate harness
tests/fixture_gemma4.cpp                FG-ARCH18 fixture
tests/fixture_g4assistant.cpp           FG-ARCH19 fixture
tests/fixture_mistral3.cpp              FG-ARCH20 fixture
tests/cases.md                          parameters, cases, anchors, limits
tests/mutate.py                         12 mutations, two halves
results/gate.log                        recorded gate output
results/mutations.log                   recorded mutation output
results/mutations.tsv                   mutation table, one line each
```

`build/` and `build-mut/` hold reproducible artefacts (generated C, objects,
temporary mutation trees) and are not part of the evidence.

## Recorded limits

- Structural gate only: no native library is linked and no graph is executed.
  No numeric output is compared.
- The gemma4 router flags are the loader's tensor-presence scan. The fixture
  models a dense lead of four layers followed by MoE layers to exercise both
  kinds; native code never derives the pattern from hparams, so the fixture says
  nothing about a released checkpoint's layer split.
- Two 0-dim/1-element tensors are reported through the three-slot gate contract
  as `{1,1,1}`: the assistant's masked-embedding metadata (native `{}`) and the
  `layer_out_scale` scalars (native `{1}`).
- `f_attention_scale` enters as float bits and the softcapping flag as a
  boolean; the gate checks the resulting decision, not the float value.
- Control vectors (`build_cvec`), the PLE graph projection
  (`build_inp_per_layer` / `project_per_layer_inputs`) and every graph-level node
  order are outside this slice.
