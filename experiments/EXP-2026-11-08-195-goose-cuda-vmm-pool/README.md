# EXP195: VMM GPU memory pool arithmetic in Goose (P3-17)

Transfers the arithmetic of `ggml_cuda_pool_vmm::alloc` and `::free` from `ggml-cuda.cu`
into Goose. The driver calls stay with the caller; everything the pool decides is Goose.

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

## Gate

| check | result |
|---|---|
| differential vs native algorithm | **4800 checks, 0 failures** |
| request mix | 1 byte, 128, small random, up to 2 MB, and 1 MB+ |
| granularities | 2 MB and 16 MB |
| operations | 400 alloc/free pairs per granularity |

The reference is a line-by-line transcription of the native `alloc`/`free` with the driver
calls removed, not a re-derivation, so the comparison is against the algorithm being
replaced.

Verified the gate can fail: changing the alignment constant from 128 to 64 makes it report
`FAIL actual_size matches native` and exit 1; restoring it returns 4800/0.

## Remaining step: wiring into the core translation unit

`ggml_cuda_pool_vmm` is a C++ class inside `ggml-cuda.cu`, and its `alloc`/`free` are virtual
methods reached through a vtable, so they cannot be interposed with LD_PRELOAD. The
established mechanism in this project is `shared-cuda.cu` — EXP160's modified copy of
`ggml-cuda.cu` that replaces `ggml-cuda.cu.o` in the link line, verified in the active chain.

Wiring this module in means editing that file so the two methods call
`fg195v_plan_alloc`/`fg195v_plan_free` and then issue the driver calls, and rebuilding the
chain from exp160 up. That is a separate, larger change and is not done here; this stage is
the byte-exact arithmetic it would call.

## Why the split is where it is

Porting the driver calls themselves into Goose would move no computation — they are opaque
handles and pointers — while making the module impossible to test without a GPU. Splitting at
the arithmetic gives a gate that runs anywhere and matches the native decisions exactly,
which is the property that matters when the pool is later wired in.
