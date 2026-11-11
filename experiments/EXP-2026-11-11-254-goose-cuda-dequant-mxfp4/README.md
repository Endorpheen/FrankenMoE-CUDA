# EXP254: CUDA MXFP4 dequantization in Goose

`dequantize_mxfp4` (`dequantize.cuh:436-452`) plus the exact launch wrapper, the
first MX path and the third non-K-family module after EXP252 (IQ4_NL) and
EXP253 (IQ4_XS).

## Verdict

**OFF_PATH** — bit-exact MXFP4 dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_MXFP4)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; source block
  `i*8 + ib` (8 blocks of 32 elements per CTA); destination
  `i*QK_K + 32*ib + 4*il`; the E8M0 exponent conversion; the nibble split; and
  `d * kvalues_mxfp4[q] * 0.5f`.
- host: `nb = ceil(k/256)`, grid `(nb,1,1)`, block 32; a `k` that is not a
  multiple of `QK_K` is rejected because the native function has no tail guard.

`kvalues_mxfp4` is `kvalues_fp4`:
`{0,1,2,3,4,6,8,12,0,-1,-2,-3,-4,-6,-8,-12}`, reproduced as an explicit Goose
constant function. The E8M0 conversion uses the manual bit path
(`bits = e ? e << 23 : 0x00400000`), which is the branch the native code compiles
on this toolchain (CUDART_VERSION < 12080), and is bitcast to float through an
explicit device primitive.

Layout used: `block_mxfp4` is 17 bytes — `uint8_t e` at 0, `uint8_t qs[16]` at 1.
`QK_MXFP4 = 32`, `QK_K = 256`.

## Two findings, neither of them the module

1. **The fixture under-allocated the source.** It allocated one 17-byte block
   per CTA, while `dequantize_mxfp4` addresses `x = base + i*8` blocks: one CTA
   always consumes 8 blocks = 256 elements. The multi-CTA arms read past the
   buffer, which surfaced as a `-0` versus `+0` mismatch. Corrected to
   `nblocks * 8 * 17`; the module itself needed no change. The "one CTA passes,
   several fail" shape is the tell for this class.
2. **The E8M0 bias constant is unobservable in this build.** `0x00400000` is
   2^-127, a denormal. With `-use_fast_math` (FTZ enabled, the production flag
   set), every product `2^-127 * |q| * 0.5` with `|q| <= 12` is still denormal
   and flushes to ±0. A mutation changing the constant to `0x00200000` produced
   zero failures because both arms flush to ±0. The mutation was replaced with
   a block-stride mutation; the exact constant is preserved and matches native.

## Gate

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 20 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| `qs` offset 1 -> 2 | 4 |
| destination `4*il` -> `2*il` | 8 |
| block stride 17 -> 18 | 4 |
| E8M0 shift 23 -> 22 | 4 |
| drop the 0.5 factor on the low nibble | 4 |

## Not integrated

Same rule as EXP245–253: emitted and gated, deliberately outside the candidate
link, because a witness for this route reads zero in the fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq1_s`/`iq1_m`, `iq2_xxs`/`iq2_xs`/`iq2_s` and `iq3_xxs`/`iq3_s` stay native
(they need the shared grid/sign tables), as do the non-F32 destinations and the
MMQ/MUL_MAT families. MMQ remains blocked on the emitter address typing recorded
in EXP180.
