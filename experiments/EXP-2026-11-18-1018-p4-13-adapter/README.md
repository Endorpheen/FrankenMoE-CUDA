# EXP-2026-11-18-1018 — P4-13 adapter, Goose side (compiled skeleton)

`src/adapter.goose` declares the five unified operations on top of the existing
allocator primitives (`fg161_alloc_buffer`, `fg161_free_buffer`,
`fg161_reset_buffer`, `fg161_usage`, `fg161_tensor_alloc`) and carries the
generation guard from `P4-13-CONTRACT.md` §8.

State: it compiles with the qualified compiler `8b7a56bb…`
(`build_adapter.c`, 45 884 bytes). The generation itself still has to live in
the host record: `gen_of` is a placeholder and `place` therefore accepts
generation 0 for any buffer. The host unit and the real generation store are
the next step, followed by the differential gate against
`experiments/EXP-2026-11-18-1014-p4-13-alloc-gate/reference-native*.txt`.

## Host unit (EXP-2026-11-18-1020)

`host/fg161_host.c` implements the five primitives the Goose adapter imports —
`fg161_alloc_buffer`, `fg161_free_buffer`, `fg161_reset_buffer`,
`fg161_set_usage`, `fg161_tensor_alloc` — over the real ggml buffer API, with a
per-buffer generation and a live table. It compiles clean with `-Wall -Wextra`
and its functional test (`host/host_test.c`, output `host/host-reference.txt`)
gives: alloc handle 1 gen 0 live 1; reset bumps gen to 1; free drops live to 0
and the handle's generation query returns 0 (dead); re-allocation reuses the
handle. Next: bind the compiled Goose skeleton to this unit and run the
differential gate against `evidence-.../reference-native*.txt`.

## Goose adapter bound to the host unit (EXP-2026-11-18-1021)

`src/adapter.goose` gained a callable `dispatch(op,a,b,c)` so the compiler emits
the operations; `host/adapter_wrap.c` maps `allocate`/`release`/`reset` to it and
`host/adapter_test.c` drives the pair. Output
(`host/adapter-reference.txt`): `goose_alloc=1 live=1 gen=0`,
`goose_reset_ret=1 gen=1`, `goose_release_ret=0 live=0`. `place` (op 3) and the
real generation argument are still to be wired.

## Generation guard works end to end (EXP-2026-11-18-1022)

`gen_of` now reads `fg161_generation` from the host and `dispatch` takes the
generation as its fifth argument. The test raises the contract's item 6 through
the Goose path:

```
alloc=1 live=1 gen=0
place_fresh=0 reset=1 place_stale=-1 release=0 live=0
```

A fresh placement is accepted, `reset` returns the new generation, a placement
carrying the old generation is rejected, and `release` clears the live entry.
`reset` was fixed to return the new generation rather than the generation after
the bump.

## Differential gate passes through the adapter (EXP-2026-11-18-1023)

`host/adapter_gate.c` drives the Goose adapter and compares against the native
reference recorded in `reference-native*.txt`:

```
alloc sizes: 12288 3168 (expect 12288 3168)
distinct handles: 1 live=2
reuse handle: 1 size=12288
reset gen=1 size_kept=1
threads: 200,200,200,200 (expect 200 each)
GATE PASS
```

Covered: allocation sizes, distinct handles, reuse after free, reset keeping the
size and bumping the generation, four-thread allocate/free, and no leak
(`live=0` after release). Output kept in `host/adapter-gate.txt`.

## Bulk placement matches native (EXP-2026-11-18-1024)

`fg161_bulk` places every tensor of a context into one buffer with the buft's
alloc size and alignment. Compared against
`ggml_backend_alloc_ctx_tensors_from_buft_size` on identical contexts:

```
200x3+64x3  native_need=3168  goose_size=3168  a_off=0 b_off=2400
256x8+128x8 native_need=12288 goose_size=12288 a_off=0 b_off=8192
```

Sizes and offsets are identical to native, which is the same shape the accepted
Goose allocator module uses (`fg161_bulk` is a host primitive there too).
