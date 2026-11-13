# P4-14 closure: failure injection, invalid inputs, abort/reset/reuse and concurrent contexts

Criterion: "Allocation failure injection, invalid-size/view/copy, abort/reset/reuse and concurrent independent contexts..."

| clause | evidence | result |
|---|---|---|
| allocation failure injection | EXP348: `LD_PRELOAD` shim fails the N-th malloc/calloc, same probe on both lines | identical exit codes and messages at every point that fires (2, 30, 60, 120, 300) |
| invalid size / view / copy | P4-03: six of seven error-path decisions identical, the seventh being the known block-alignment divergence | as recorded there |
| abort | the same injection runs, where the failure takes the abort path | both lines exit 134 with byte-identical messages (`libgomp: Out of memory allocating 8 bytes`; `memory allocation of 416 bytes failed`) |
| reset / reuse ordering | EXP348 probe2: create 20 tensors, `ggml_reset`, create 20 again, then three more reset-and-refill cycles, in two threads | `used=27840` after reset in both contexts on both lines, stable across cycles, `ok=1` |
| concurrent independent contexts | same probe2: two threads, one context each, created and freed independently | identical numbers, no interference; ours additionally emits its witnesses (`goose_modes=0x400100f`) |

## Scope notes

Abort parity is demonstrated through the failure path rather than by calling an abort entry point directly: the injected failures take that
path, and the messages match byte for byte. That is the observable part; the abort symbol itself is a declared primitive.

Allocation failure exercises context creation only -- tensors come from the arena, so injection past a few hundred allocations never fires.
That is stated in EXP348 as scope, not as coverage.

The probe keeps `_IONBF`, since block-buffered stdout silently swallowed the evidence twice in this session.
