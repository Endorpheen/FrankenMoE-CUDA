# C1-02: Qwen4Exp graph and metadata - the slice exists, most of it is not in the unit

Checked 2026-11-19 on `main`, text level, no build.

## What the tree contains

| module | exports | in the composed unit |
|---|---|---|
| `goose/model/q4m-metadata.goose` | 19 | **no** |
| `goose/model/q4t-tensor-map.goose` | 10 | yes (`build-main-runtime.sh:71`) |
| `goose/model/q4e-expert-layout.goose` | 15 | **no** |
| `goose/model/qwen4exp.goose` (the accepted P6-05 plan/shape builder) | 4 (`q4_kind`, `q4_is_ple`, `q4_plan`, `q4_shape`) | **no** |

**No host calls any of them**: sampling `q4_kind`, `q4_plan`, `q4m_recurrent`, `q4t_flags` and
`q4e_experts` across `vendor/flint`, `runtime/` and `integration/` returns nothing.

## What the experiment proved, and where

EXP-2026-11-15-660-c1-02 built the three slices and gated them:

```
C1_02_Q4M  cases=15 values=222 failures=0
C1_02_Q4T  cases=13 values=528 failures=0
C1_02_Q4E  cases=12 values=28  failures=0
C1_02_JOIN cases=3  values=156 failures=0
43 cases / 934 values, 0 failures, at -O1 and again at -O0
36/36 semantic mutations caught
```

That is real evidence, and it lives in the experiment (`tests/build.sh`, `tests/mutate.py`,
`results/gate-O1.txt`, `results/gate-O0.txt`). It is not in `tests/` as a permanent gate, and the
gate builds the modules from the experiment's `source/` copies, not from `goose/`.

## The three open questions in the requirement

1. **`T_NEXTN_HC_HEAD` (ids 49..51)**: the experiment's own fixture records that these ids have no
   entry in the pinned `llama-arch.cpp`; the tensors exist in the integration tree but not in the
   pinned upstream. They are therefore **not required by the pinned tree**, and the ported map must
   not invent names for them. That is the answer the requirement asks for, and it is recorded rather
   than guessed.
2. **skip-PLE tensor branch**: it belongs to the metadata/tensor-map split the slice made; the
   experiment's `q4t` gate covers presence per kind for all 51 canon ids, including the PLE ids.
3. **HC gamma rank (`{n_embd, hc}` vs `{hc_dim}`)**: this is a shape question, and shapes are exactly
   what the `q4e`/`q4m` gates compare against their transcriptions. It cannot be settled here without
   either the fixture that carries the native lines or a real GGUF.

## What closing C1-02 needs

1. put `qwen4exp.goose`, `q4m` and `q4e` into the composed unit (only `q4t` is there today);
2. move the experiment's gate into `tests/` as a permanent test, building from `goose/` rather than
   from the experiment's copies - then the 43 cases become a regression check instead of a record;
3. a Qwen GGUF to check the shapes against a real file, which is a model-side run.

None of it is claimed as done, and no Qwen model has been loaded at any point.

## Why the append failed: per-family constants share one name

Adding `qwen4exp.goose`, `q4m-metadata.goose` and `q4e-expert-layout.goose` to the unit list was
tried and reverted, because the composer reports a real conflict:

```
conflicts:
  const P_HAS_OUTPUT: goose/model/qwen4exp.goose differs from an earlier module
```

`P_HAS_OUTPUT` is a parameter-vector index, and each architecture numbers its own vector:

| module | value |
|---|---|
| `goose/model/qwen35moe.goose` (the only one in the unit today) | its own index |
| `goose/model/qwen4exp.goose` | 26 |
| `goose/model/gemma3.goose` | 8 |
| `goose/model/deepseek32.goose` | 21 |

One flat unit carries exactly one definition of that name, so **the architecture builders cannot
share one namespace as module-level constants**. That is not a naming accident; it is the
per-family difference the requirement itself points at ("architecture adapters only where the
semantics really differ"). The same shape appears in the `fail` binding: the unit already carries
eight modules declaring `fn fail(code: i64)` with eight different C symbols.

The fix is a decision, not a patch: either the parameter-vector indices move into a per-architecture
table (a struct or an array passed to the builder), or the unit composes one family's builder at a
time. Until one is chosen, `qwen4exp`, `q4m` and `q4e` stay out of the list, and the append is not
retried blindly.
