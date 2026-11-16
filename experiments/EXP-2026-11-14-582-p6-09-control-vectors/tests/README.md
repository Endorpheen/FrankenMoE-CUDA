# P6-09 S3 gate — control vectors (EXP578)

Differential gate for the Goose port of `llama_adapter_cvec` in
`source/cvec.goose` against the real adapter methods in
`llama.cpp-flint-cuda` (`src/llama-adapter.cpp:16-138`).

## Run

```sh
bash tests/build.sh            # build + run, 10 cases
bash tests/build.sh --no-run   # build only
python3 tests/mutate.py --half A
python3 tests/mutate.py --half B
```

Expected: `FG578_CV cases=10 values=2447 failures=0` and
`FG578_MUT half=all caught=16/16`.

## Oracle

The gate links the real `libllama`/`libggml` and calls the native
`llama_adapter_cvec::apply`, `apply` (disable path), `apply_to` and
`tensor_for`. There are no weights, no GGUF and no graph computation.

`llama_model` is **abstract** in the current flint tree
(`src/llama-model.h:708-920`): `load_stats`, `load_hparams`, `load_vocab`,
`load_tensors`, `load_arch_hparams`, `load_arch_tensors` and
`build_arch_graph` are pure virtual. Therefore `llama_model model;` cannot be
compiled, and `#include "llama-model.h"` alone does not help.

`tests/fixture.cpp` defines `TestModel : llama_model`, which implements those
seven virtuals as unused stubs. The native methods under test only read
`model.hparams` (`n_embd`, `n_layer()`); none of the stubs is ever called. The
model object is an hparams carrier, just as in the earlier version that wrote
the same fields into a concrete `llama_model`.

The direction table is built by the fixture, exactly as `init()` would build
it: one zeroed F32 tensor per layer 1..n-1, layer 0 absent, CPU backend buffer,
`layer_start = layer_end = -1`. It is installed into the adapter through a
test-only `#define private public` around `llama-adapter.h`. Because the table
is non-empty, native `apply` skips `init()` and never calls
`model.select_buft` (which needs `pimpl->dev_layer` from a real load).

The pool is deliberately seeded with garbage before `cv_init`, so a missing
clear in the port is visible (mutation A2).

## What is compared per layer

- presence: native `tensor_for(il) != nullptr` vs port `cv_direction_off(il) >= 0`;
- direction bytes: `ggml_backend_tensor_get` vs the port pool slice;
- `apply_to`: same pointer when absent; a new `GGML_OP_ADD` node with
  `ne[0]=n_embd`, `ne[1]=rows` when present, plus the exact broadcasted numeric
  result of the port.

## What remains unverified

- native `init()` allocation path (context per buffer type, `select_buft`,
  buffer clear) — the table is installed by the fixture;
- the graph is not computed: the add is checked by node shape/pointer and the
  port's arithmetic, not by executing `ggml_add`;
- weight loading, real model hparams, GGUF, ALoRA (other slices).
