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

## Context allocation goes through Goose (EXP-2026-11-18-1025)

Dispatch op 4 calls `allocate_ctx`, which uses the `fg161_bulk` primitive and
sets the weight usage. End to end through the adapter:

```
ctx: native=12288 goose=12288 a_off=0 b_off=8192 live=1
after_release live=0
```

The full operation set is now allocate, allocate_ctx, release, reset and place,
with the generation guard — every differential check against the native
reference passes.

## Call site switched in the shipped build + triple A/B (EXP-2026-11-18-1026)

`ggml_backend_alloc_ctx_tensors_from_buft` now allocates through the Goose
adapter (`fg161_bulk`), with `GGML_GOOSE_ALLOC=0` restoring the native body.
libggml-base rebuilt in the reproducible tree; execution proven in the server:
`GOOSE_ALLOC bulk=3 buffers=3` (native arm: no such line).

Three paired runs, same profile and request (`--moe-cache-mib 0`, 32/8):

| pair | sha | tokens | logprobs | top-5 | native pre/dec | goose pre/dec | VRAM | RAM avail |
|---|---|---|---|---|---|---|---|---|
| 1 | 5a82551375c2 = | = | = | = | 71.04 / 31.74 | 64.90 / 30.36 | 4741 = | 47,121,772 / 47,134,996 |
| 2 | 5a82551375c2 = | = | = | = | 71.00 / 30.04 | 70.73 / 31.63 | 4741 = | 47,121,164 / 47,000,064 |
| 3 | 5a82551375c2 = | = | = | = | 65.64 / 30.28 | 70.54 / 32.25 | 4741 = | 47,017,240 / 47,134,716 |

Medians: prefill 71.00 -> 70.54 (-0.7%), decode 30.28 -> 31.63 (+4.5%), VRAM
identical, RAM identical. Correctness is bit-exact; no functional regression.
The native body stays only as the documented OFF arm (rule 10).
