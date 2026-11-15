# EXP-2026-11-15-684 — the Apex architecture builder plans the real Apex model

Base: `5a556e20`. Reads metadata only: no model was started, no GPU was touched.

## 1. The blocker this clears

The Apex layer-structure module (`goose/model/qwen35moe.goose`) had three exports
but could not join the common unit: it imports `base/model_graph.goose`, whose
`ensure` and per-layer helpers collide with the canonical modules. That is the
same collision that excluded `q4e-expert-layout` in EXP677.

The resolution is the one the build script already implied: an architecture gets a
unit of its own. `tools/build-apex-unit.sh` derives the module list from
`build-main-runtime.sh` - never typed twice - and appends the architecture module.
`base/model_graph.goose` is deliberately not composed: it is
`goose-model-graph.goose` minus its `main()`, which the common unit already
carries, so the composer aliases the import to the canonical module instead of
duplicating it.

Unit: 120 modules, 3991 specializations, 0 duplicate globals. `q35_kind`,
`q35_plan`, `q35_shape` are present in the object alongside `gb_common` and
`rh_is_moe_node`.

## 2. The plan for the real model

```
FG_APEX_PLAN arch=qwen35moe layers=42 attn=10 recr=31 mtp=1 other=0
             embd=2048 head=16 head_kv=2 experts=256 used=8 interval=4
```

`Qwen3.6-35B-A3B-UD-IQ4_XS.gguf`, 55 metadata keys. The plan is 10 full-attention
layers, 31 recurrent (GDN) layers and 1 MTP block: a hybrid architecture, which
is what `full_attention_interval=4` describes for 41 trunk layers.

## 3. A defect found by running it on the real file

The first run reported `attn=41 recr=0 interval=0` - a plan that was wrong. The
gate was reading `qwen35moe.attention.recurrent_layers_interval`, which this model
does not carry; the loader's key is `qwen35moe.full_attention_interval`. With the
correct key the plan becomes 10/31/1.

This is the value of running against the real file: the synthetic parameter sets
EXP-577 accepted exercise the interval formula, but only a real model shows which
key name carries it.

## 4. What this does and does not prove

**Proves**: the architecture builder answers for the actual Apex weights file -
layer count, per-layer kind distribution, embedding width, head counts, expert
count - and does so from Goose, through the composed unit, with no C++ layer
logic involved in the plan.

**Does not prove**: that a graph can be built from this plan, or that a token can
be generated. The plan is the input to the graph builder; the graph assembly, the
KV buffers, the CUDA backend and a model run are still open.

## 5. Cost

One unit build (120 modules, required because the architecture exports did not
exist in any object) and one gate compile. No repeated builds.

## 6. Reproduce

```
bash tools/build-apex-unit.sh
gcc-13 -O1 -std=c11 -I build/goose-apex -I upstream/llama.cpp/ggml/include \
    -c integration/goose-standalone/tests/apex_plan.c -o /tmp/apex_plan.o
g++-13 -O1 -fopenmp build/goose-apex/main.o builds/goose-standalone/host.o \
    builds/goose-standalone/graph-bindings.o /tmp/apex_plan.o \
    -L<off ggml>/ -lggml-base -lggml -lggml-cpu -lm -pthread -lstdc++ -o /tmp/apex_plan
/tmp/apex_plan <apex-model.gguf>
```
