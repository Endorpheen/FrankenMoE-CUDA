# EXP279: CUDA cumsum in Goose — shared-memory primitives unblock the class

The `cumsum` fallback kernel from `ggml-cuda/cumsum.cu`. This build defines no
`GGML_CUDA_USE_CUB`, so the fallback is the native path. EXP273's assessment is
now resolved: shared memory is expressed as explicit primitives and the whole
scan stays in Goose.

## Verdict

**OFF_PATH** — bit-exact against the native fallback kernel, but the fixed
32K/128 Apex profile does not execute CUMSUM. No performance claim, shipped DSO
unchanged.

## Scope transferred

All algorithm content is Goose:

- the thread-local 4-element sequential scan (`temp[0..3]` as scalars),
- the warp prefix inclusive sum (shfl_up ladder with the `lane >= offset` guard),
- the shared exchange of per-thread prefixes and per-warp sums,
- warp 0's exclusive scan of the warp sums back into `s_warp_sums`, plus the
  chunk total,
- `final_offset = (s_vals + s_warp_sums + carry) - temp[3]`,
- the per-chunk carry accumulation and the 4*blockDim chunk loop.

Extern primitives: dynamic shared-memory base, shared load/store,
`__syncthreads`, and `shfl_up`. They are memory/synchronization effects, not
algorithm; the addition order is preserved statement by statement, which is what
makes the float results bit-identical.

Host plan: `num_warps = ceil(ne00/warp)`, `block = min(num_warps*warp, 256)`,
`shmem = (block + warps + 2)*4`, grid `(ne01,ne02,ne03)`, plus rejections.

## Gate

| check | result |
|---|---|
| device: 9 cases (1, 31, 32, 33, 127, 128, 129, 1000, and 4096×2 rows) | 9 673 values bit-identical |
| device failures / launch failures | 0 / 0 |
| host: 6 plans (1…8193) + 3 rejections | 36 checks / 0 failures |

## Mutations — and a NaN-masking fixture hole

| mutation | failures |
|---|---:|
| warp scan adds without the lane guard | 9 673 |
| warp scan ladder steps by 1 | 9 564 |
| final offset drops the carry | 6 144 |
| carry replaced instead of accumulated | 4 096 |
| host block cap 256 -> 512 | 6 |

The carry mutation first passed with **0 failures**: the fixture filled inputs
with raw random bit patterns, so most values were NaN or inf, and a NaN
cumulative sum masks any carry difference downstream. Switching to finite values
(`(rng%2001 - 1000) * 0.125f`) exposed it at 4 096 values. Recorded because it is
a new class: the generator spanned the control flow but not the value domain.

## Not integrated

Same rule as EXP245–278: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
