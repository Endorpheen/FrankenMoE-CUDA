# EXP-2026-11-14-582 — control vectors (P6-09 S3)

Native counterpart: `llama_adapter_cvec` in `src/llama-adapter.cpp` (state,
`apply`, `apply_to`, `tensor_for`). Base commit: `3e9947ed`.

## What this slice covers

`source/cvec.goose` ports the control-vector adapter:

- per-layer direction table state with layer 0 absent;
- `tensor_for` bounds against `cv_direction_off`;
- `apply`: the copy `off = n_embd*(il-1)` with the `off+n_embd <= len` guard,
  short vectors, repeated application, disable, and the `n_embd` refusal before
  any state change;
- `apply_to`: the same tensor when no direction exists, and a new `GGML_OP_ADD`
  node of shape `[n_embd, rows]` with the exact per-row translation when a
  direction exists.

## Gate

The native oracle is white-box but real: the fixture defines
`TestModel : llama_model` with the seven pure virtuals stubbed (the current tree
makes `llama_model` abstract), installs a 31-tensor F32 direction table in a CPU
buffer (layer 0 absent) and drives the real `apply` / `apply_to` / `tensor_for`.
The stub virtuals are never called by those paths; only `model.hparams` is read.
This executes the adapter code rather than a rewritten copy.

```
FG578_CV cases=10 values=2447 failures=0
```

The test pool is seeded with `(i % 13) + 0.25f` before `cv_init`, so a skipped
zeroing is visible in the byte comparison. Negative control:
`hparams.n_embd = N_EMBD - 1` yields `failures=284`, proving the oracle is not
empty.

## Mutations

`tests/mutate.py --half A|B` holds 8 + 8 mutations over state/validation and
copy/arithmetic; each is checked with `goose check`, rebuilt and required to
fail the gate.

```
python3 tests/mutate.py --half A   # 8/8 caught
python3 tests/mutate.py --half B   # 8/8 caught
```

## Recorded limits

- Native `init()` is not executed: buffer-type contexts, `select_buft` and the
  buffer zeroing are out of the gate (the fixture installs the table).
- The `ggml_add` graph is not evaluated; the gate checks the node kind, shape,
  pointer and the port's row arithmetic.
- Weight/GGUF loading and the model saver are other slices of P6-09.
