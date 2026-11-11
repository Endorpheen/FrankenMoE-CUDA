# EXP273: CUDA count_equal device computation in Goose

The `count_equal` kernel from `ggml-cuda/count-equal.cu`: per-chunk loop, equality
count, 32-lane integer warp reduction and atomic accumulation. This is the
`COUNT_EQUAL` operator from P3-05's remaining reduction list.

## Verdict

**OFF_PATH** — bit-exact against the native kernel, but the fixed 32K/128 Apex
profile does not execute COUNT_EQUAL (it is a test/diagnostic operator). No
performance claim, shipped DSO unchanged.

## Scope transferred

Everything algorithmic is Goose: `i0 = blk*dk`, `i1 = min(i0+dk, k)`, the
WARP_SIZE-strided loop, the equality test, the integer reduction ladder
(`16,8,4,2,1`) and the final atomic add. Extern symbols are a 32-bit load, a
shuffle and an atomic add. The host launch plan (`dne`, `blocks_num`) is shared
launch geometry and remains native.

## Oracle, stated explicitly

The native kernel is defined inside `count-equal.cu`, not in a header, so it
cannot be included as an oracle the way `dequantize.cuh` can. The fixture uses a
**verbatim copy** of the kernel body in `reference/count-equal-kernel.cuh`, with
the source path and `sha256 5f180b30…f2b7f8` recorded in that file. Only the
comment was added. The host plan in the fixture is the native formula
(`dne = pad(ceil(ne/(4*nsm)),128)`, `blocks = min(4*nsm, ceil(ne/128))`), used so
both arms receive the same grid and chunk size.

## Gate

9 sizes (1, 31, 32, 127, 128, 129, 1000, 8192, 100003) × 4 equality patterns
(all equal, none equal, every third equal, half equal) = 36 checks, device
`multiProcessorCount` used for `nsm`.

| check | result |
|---|---|
| Go vs native count | 36 checks / 0 failures |
| launch failures | 0 |

## Mutations

| mutation | failures |
|---|---:|
| loop stride 32 -> 16 | 25 |
| chunk end drops the last element | 17 |
| equality inverted | 32 |
| reduction ladder starts at 8 | 25 |
| atomic adds twice the count | 28 |

## Not integrated

Same rule as EXP245–272: emitted and gated, deliberately outside the candidate
link.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py
