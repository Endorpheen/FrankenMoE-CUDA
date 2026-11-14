# P6-09 coverage — adapters, LoRA, control vectors and model saving

Updated: 2026-11-14. Native sources: `src/llama-adapter.cpp`,
`src/llama-model-saver.cpp`, the LoRA manifest/application blocks and the saver
writer path.

The requirement owns five slices. S3 is accepted; the rest is open.

| slice | native scope | state |
|---|---|---|
| S1. LoRA manifest and pairing | adapter manifest parsing, tensor pairing | **done (EXP600)** |
| S2. LoRA application and scale | `apply` for LoRA, scaling, cleanup | in acceptance |
| S3. Control vectors | `llama_adapter_cvec` state, `apply`, `apply_to`, `tensor_for` | **done (EXP582)** |
| S4. Saver writer | GGUF writer path for saved models | **done (EXP606)** |
| S5. Saver from a model | `llama_model_saver` metadata/tensor enumeration | open |

## S3 — accepted

Source: `experiments/EXP-2026-11-14-582-p6-09-control-vectors/`; canonical file
`goose/model/control-vectors.goose`.

The port owns the per-layer direction table (layer 0 absent), `tensor_for`
bounds against `cv_direction_off`, the `apply` copy rule
`off = n_embd*(il-1)` with the `off+n_embd <= len` guard, short vectors,
repeated application and disable, the `n_embd` refusal before any state change,
and `apply_to` (same tensor without a direction, a `GGML_OP_ADD` node of shape
`[n_embd, rows]` with exact per-row arithmetic with one).

Gate: a white-box `TestModel` (the seven pure virtuals stubbed, because
`llama_model` is abstract in this tree) drives the real adapter over 10 cases /
2447 values, 0 failures; mutations 16/16 in two halves. The pool is seeded so a
skipped zeroing is visible, and a negative control produces 284 failures.

## S4/S5 plan (model saving)

Native anchors in `src/llama-model-saver.cpp`: `llama_model_saver_supports_arch`
(16), the two constructors (31, 36), `add_kv` overloads (45-130), `add_tensor`
(133), `add_kv_from_model` (147), `add_tensors_from_model` (473) and `save`
(521), over the accepted GGUF writer (P7-02 S2/S3).

S4 is the writer path: the saver accepts an arch name and a GGUF context and
appends the metadata/tensor records for a given set of values; the gate runs it
against a synthetic context and compares the resulting bytes with the native
saver on the same values. No model is needed.

S5 is the model path: `add_kv_from_model` reads hparams and vocabulary, and
`add_tensors_from_model` scans layer pointers with architecture-specific
aliasing. That cannot be proven offline; acceptance must be an integration
fixture with a tiny synthetic model/hparams/vocab, and the report must say so
instead of claiming a byte gate.

## Open boundaries

- The native `init()` allocation path is not executed; the fixture installs the
  direction table and the buffer zeroing is checked through the seeded pool.
- The `ggml_add` graph is not evaluated; node kind, shape, pointer and the
  port's row arithmetic are compared.
- LoRA (S1, S2) and the model saver (S4, S5) remain native.
