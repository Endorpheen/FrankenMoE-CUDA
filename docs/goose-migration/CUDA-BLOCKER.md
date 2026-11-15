# CUDA backend in the standalone — the exact blocker, measured

Base: `bbeea4ca`. This card records a finding, not a fix. Nothing here is a passing gate.

## 1. What was believed

The ledger said two CUDA modules could not enter the build because they need 25 `fg162_*`
host primitives "that do not exist yet; an ABI surface, not a wiring gap"
(`docs/goose-migration/BUILD-SET.md:141`).

That is wrong in its first half. The 25 primitives exist, all of them, in
`experiments/EXP-2026-11-08-162-goose-backend-buffers/source/buffers-bridge.h` - 53 lines,
complete, accepted as `ACCEPTED_SHARED_BACKEND_BUFFER_CONTROL_NOT_FULL_RUNTIME`.

## 2. What the actual obstacle is

The bridge does not compile against this repository's ggml. It uses three types that the
vendored ggml does not declare at all:

| type | where the bridge uses it | present in `upstream/llama.cpp/ggml/include/ggml-backend.h` |
|---|---|---|
| `ggml_backend_buft_alloc_buffer_n_plan_item` | `fg162_constant`, `fg162_heap`, `fg162_get/put` | **no** |
| `ggml_backend_multi_buffer_context` | `fg162_heap`, `fg162_get/put` | **no** |
| `ggml_backend_buft_interface.alloc_buffer_n` | `fg162_callback`, `fg162_buft_call` | **no** |

`grep -c` over the vendored header returns 0 for each. They are not optional decorations:
`fg162_heap` allocates an array of `alloc_buffer_n_plan_item` and `fg162_get` reads its
`size`/`first`/`last` fields, so the struct layout is part of the ABI the module was
compiled against.

The fixture's manifest names the library it targets:

```
runtime/exp162/run/libggml-base-buffers-6273a37c65ee.so.0.27.0
sha256 21aa28e9...c8cc01c
```

That file is **not present** on this machine any more. So the accepted fixture's ABI cannot
even be checked against a live library here, and the vendored ggml in this repository is a
different, newer or older revision that never had `alloc_buffer_n`.

## 3. Why this is the honest blocker and not an excuse

The three options, and why each is what it is:

1. **Port the bridge to the vendored ggml.** This means writing the multi-buffer and
   `alloc_buffer_n` functionality that the vendored ggml lacks - new implementation, not
   translation. Rule 17 forbids exactly this: a host adapter is acceptable only while the
   substrate has no Goose-callable surface, it must not grow into a parallel implementation.
   Reimplementing ggml's buffer planning in the host would be that parallel implementation.
2. **Vendor the fixture's ggml.** The `.so` is gone and there is no matching header set in
   the tree. Reconstructing it from a generated C file is guesswork over an ABI.
3. **Leave the two modules out and say so.** This is what is being done.

## 4. What this means for the 100% claim

The CUDA backend is not migrated, and cannot be by wiring. Two of the modules that carry it
were built against a ggml API that this repository does not have. Until the vendored ggml
grows `alloc_buffer_n`/multi-buffer, or the upstream revision that has them is adopted, those
two modules stay outside the build set - and the honest statement is:

- **CPU path:** layers, MoE, state, prefill/decode all proven (EXP-688b..706).
- **CUDA path:** 5 modules in the build set (dispatch, arch policy, target flags, install
  layout, mmq dispatch); the buffer-control modules are blocked on a substrate gap, with the
  reason named and measured.

## 5. The concrete next step, if this is to be closed

Not a re-attempt at the bridge. Adopt or align to the ggml revision that has
`ggml_backend_buft_alloc_buffer_n`, then the 53-line bridge compiles unchanged and the two
modules enter the build set without a single new line of host logic. Anything else is
writing the substrate inside the adapter.
