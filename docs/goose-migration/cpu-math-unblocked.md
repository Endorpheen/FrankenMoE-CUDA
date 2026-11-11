# APEX.CPU_MATH: the vector blocker is gone, with the evidence

I recorded "Goose has no vector types or intrinsics" as the blocker for this item. The compiler side says
otherwise, so I checked instead of insisting.

## Verified on the exact Stage3 they named

| check | result |
|---|---|
| build | `verify-simdreassoc-20261111-v58/stage3`, sha256 `36b744ff…` — matches the published value |
| compile their Q4_K-style probe | ok, once run from the Fried Goose root (`stdlib` is resolved relative to the working directory — the same trap as the `exp` probe) |
| run | prints **495.0**, identical to the binary they shipped |
| capabilities used | `packed_simd::{and8, shr8, maddubs, madd16, add32, splat16}` over `u8[32]`/`i8[32]` |
| compile flags | `clang -std=c11 -O2 -mavx2` |

So the operations the quantized expert dot needs are expressible, and my item-3a note was wrong in the same
way my `exp` note was wrong: the capability exists, the probe was not run the way a real program runs.

## What the transfer needs

The reference is available and exported by the library this candidate loads:

    ggml_vec_dot_q4_K_q8_K          (AVX2)
    ggml_vec_dot_q4_K_q8_K_generic

A full block comparison (not just one 32-element sub-block) needs the real layout: `block_q4_K`
(d, dmin, 12 bytes of packed 6-bit scales/mins, 128 bytes of nibbles) against `block_q8_K` (d, 256 signed
bytes, 16 sums). The example in the handoff only covers one sub-block, so the port must add the per-subblock
scale/min unpack and the `- dmin * Σq8` term.

## Plan for the item

1. implement the full block dot in Goose against those layouts;
2. gate it bit-exactly against `ggml_vec_dot_q4_K_q8_K` on thousands of generated blocks, with mutations;
3. route the expert dot through it with a disable switch, in the Goose `libggml-cpu` this tree now rebuilds;
4. require the CPU_CONTROL witnesses to stay live and the completion to stay `e68df05b`;
5. one checkpoint, then accept.
