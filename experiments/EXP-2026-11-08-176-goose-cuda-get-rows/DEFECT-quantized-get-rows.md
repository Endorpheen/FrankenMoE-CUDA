# DEFECT: GET_ROWS takes the Goose path for quantized sources

Status: open, reproduced, isolated, not yet fixed.
Found: 2026-11-08, while running `test-backend-ops` against `runtime/exp196/run`.

## What happens

`ggml_backend_cuda_op_get_rows` on a quantized source produces NaN and then a
misaligned-address CUDA error, aborting the process:

    [GET_ROWS] NaN at index 545 (CUDA0=nan CPU=0.572189)
    CUDA error: misaligned address
      in function ggml_backend_cuda_synchronize at shared-cuda.cu:3371

`test-backend-ops test -o GET_ROWS -b CUDA0` reports **69 failures** and then aborts.

## It is our port, not a pre-existing native problem

Isolated with the module's own switch, same binary, one variable:

| run | result |
|---|---|
| `GGML_GOOSE_CUDA_GET_ROWS=0` | 2/2 backends passed, no failures |
| switch unset (Goose on) | 69 failures, abort |
| every `GGML_GOOSE_CUDA_*` switch set to 0 | 2/2 backends passed |

So the failure follows this module and nothing else. It is not inherited from the
baseline.

## Cause

`fg176g`'s dispatch in `experiments/EXP-2026-11-08-176-goose-cuda-get-rows/source/cuda-runtime.cu`
tests only the flag, then hands the case to the Goose module. The Goose body models
`src0_t == dst_t`: the vector path is gated on `t0 == td` (`vec = 0` otherwise), and the
scalar path then reads elements of `esd` bytes from a row whose real element size is the
quantized type's. Reading a `q2_K`/`q8_0` row as `float` gives NaN and, when the row start is
not 4-byte aligned, the misaligned address.

EXP176's scope statement and its gate both cover `F32 -> F32` only. That is exactly what the
Apex profile executes, which is why 38 modules including this one run clean there and this
never surfaced.

## Why it was not caught

The module's own gate compares `F32 -> F32` fixtures, and the Apex profile uses `F32 -> F32`.
No test in the chain exercised a quantized source, so the "byte-exact" claim was true and
narrower than it read.

## Fix direction

Gate the dispatch on the types the module actually implements:

    if (dst->src[0]->type != dst->type) { fg176g_native_ggml_cuda_op_get_rows(ctx, dst); return; }

That restores correctness for every quantized source while leaving the Apex path unchanged.
The alternative — teaching the Goose body the quantized element sizes — is a larger change
and is not needed for this profile.

## Why it is not fixed in this commit

Fixing `cuda-runtime.cu` requires rebuilding the EXP176 object and relinking every stage that
inherits it up to the active runtime. That rebuild is a single coherent change and must be
followed by the full paired Apex protocol; it should not be started in a half-finished state.
The gate that would prove the fix is already known and is the command above.
