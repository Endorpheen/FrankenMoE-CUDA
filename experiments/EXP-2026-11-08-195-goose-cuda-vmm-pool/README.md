# EXP195: VMM GPU memory pool arithmetic in Goose (P3-17)

Transfers the arithmetic of `ggml_cuda_pool_vmm::alloc` and `::free` from `ggml-cuda.cu`
into Goose, and wires it into a real Apex pass. The driver calls stay with the caller;
everything the pool decides is Goose.

## The path was confirmed before porting, not assumed

Two earlier attempts in this session ported code that never executes (EXP188's row
dequantizer, EXP190's Q8_K row quantizer), so this one started with a probe. An LD_PRELOAD
counter over the driver entry points was loaded into a real Apex run:

    FG195_VMM cuMemCreate=105 cuMemAddressReserve=2 cuMemMap=105
               cuMemRelease=105 cuMemUnmap=21 cuMemAddressFree=2

The pool grows, maps and unmaps during inference, so this is live code.

## Scope transferred

Goose owns: the 128-byte alignment rounding of each request, the available-space
computation, the granularity rounding of the growth request, the 32 GB ceiling check, the
bump-pointer advance that yields the returned address, and the reverse-order check on free.

The externs are `cuMemCreate`, `cuMemAddressReserve`, `cuMemMap`, `cuMemRelease`,
`cuMemSetAccess`, `cuMemUnmap`, `cuMemAddressFree` — the hardware boundary, which is not
arithmetic.

`plan_alloc` returns six values: the growth size, the offset of the returned pointer, the
new `pool_size`, the new `pool_used`, the aligned `actual_size`, and the mapping offset. The
last one exists so the caller does not re-derive `cuMemMap`'s target after `pool_size` has
already been updated — the native code maps at the pre-growth end of the pool, and passing
the post-growth value there would map into the new region instead of at its start.

## Gate

| check | result |
|---|---|
| differential vs native algorithm | **5600 checks, 0 failures** |
| request mix | 1 byte, 128, small random, up to 2 MB, and 1 MB+ |
| granularities | 2 MB and 16 MB |
| operations | 400 alloc/free pairs per granularity |
| mapping offset | asserted equal to the pre-growth `pool_size` |

The reference is a line-by-line transcription of the native `alloc`/`free` with the driver
calls removed, not a re-derivation, so the comparison is against the algorithm being
replaced.

Verified the gate can fail: changing the alignment constant from 128 to 64 makes it report
`FAIL actual_size matches native` and exit 1; restoring it returns 5600/0.

## Wiring into the core translation unit

`ggml_cuda_pool_vmm` is a C++ class inside `ggml-cuda.cu`, and its `alloc`/`free` are virtual
methods reached through a vtable, so they cannot be interposed with LD_PRELOAD. The
established mechanism is `shared-cuda.cu` — EXP160's modified copy of `ggml-cuda.cu` that
replaces `ggml-cuda.cu.o` in the link line.

This experiment carries its own copy at `source/shared-cuda.cu` (EXP160's file, unchanged in
that stage, plus the two calls) so the qualified stage stays exactly as it was. `alloc` asks
`fg195v_enabled()`, takes the plan from Goose, and then issues the driver calls; `free` takes
the rewind from Goose. `GGML_GOOSE_CUDA_VMM_POOL=0` restores both native bodies, which the
witness confirms (`alloc=0 free=0`), so the rollback path is real and not just present.

`build_and_test.py` generates the module, builds the fixture, compiles this copy, and links
a library from the active link line with the two objects swapped in. A full chain rebuild
would have to pass EXP160 the same `bridge.o`; the active `runtime/exp185/run` is untouched
and keeps its own library.

## Evidence that Goose executes in a real pass

Server stderr after a full 32K/128 Apex run:

    FG195V_CUDA_VMM_POOL alloc=9526 free=9526 grow=5 native=0

Every allocation and every release in the pass went through the Goose module. Output text
sha256 stayed `e68df05b`, identical to the reference.

## Paired measurement (native pool vs Goose pool, same library)

Alternating runs, witness clean in both arms:

| run | mode | prefill tok/s | decode tok/s | witness |
|---|---|---|---|---|
| n2 | native | 2577.79 | 26.56 | alloc=0 free=0 |
| n3 | native | 2568.90 | 27.07 | alloc=0 free=0 |
| g3 | Goose | 2577.50 | 27.16 | alloc=9526 free=9526 |
| g4 | Goose | 2572.30 | 25.79 | alloc=9526 free=9526 |

| arm | prefill mean | decode mean |
|---|---|---|
| native | 2573.33 | 26.81 |
| Goose | 2574.92 | 26.48 |
| delta | **+0.06%** | **−1.26%** |

Both figures are inside the 2% band. The pool runs 9526 times per pass, so the cost is
measurable at all; it is host-side arithmetic against a driver call that dominates the pair.

## Why the split is where it is

Porting the driver calls themselves into Goose would move no computation — they are opaque
handles and pointers — while making the module impossible to test without a GPU. Splitting at
the arithmetic gives a gate that runs anywhere and matches the native decisions exactly.
