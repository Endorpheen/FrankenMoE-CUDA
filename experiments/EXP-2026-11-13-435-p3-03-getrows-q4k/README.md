# EXP435: Q4_K GET_ROWS in Goose

Apex token embeddings are Q4_K (`token_embd.weight`), so the quantized gather is
on the inference path. This experiment ports the Q4_K gather to Goose using the
already-qualified EXP182 Q4_K super-block dequantization arithmetic.

## What was ported

`source/device.goose`:

- `scale_min`: the 12-byte packed 6-bit scale/min unpack;
- `q4k`: the 8x4 thread decomposition, `d`/`dmin` half unpack, affine
  `d1*(q&15)-m1` and `d2*(q>>4)-m2` for one 256-value super-block;
- `getrows`: the `k_get_rows_kq` gather loop -- token index `i10`, source row
  `i01`, destination row, and the super-block stride `ib += gridDim.y`.

Host: block 32; grid `(ne10, min(nsb, 65535), min(ne11*ne12, 65535))`, with
`ne00 % 256 == 0`.

## Gate

The oracle is the native `k_get_rows_kq<float, dequantize_q4_K<float>>` template
from `getrows.cu` / `dequantize.cuh`, compiled into the fixture.

| case | values |
|---|---:|
| 9 shapes: 1/4/8/16/17/32/64 tokens, 1/2/3/4 super-blocks, 1/2/4/6/8/17/32/100 source rows | **61 952 values, 0 failures**, 0 launch failures |

Sanitizers: memcheck, racecheck, initcheck and synccheck all 0. Mutations 5/5:
scale mask 63 -> 31 (12 554 failures), affine sign flipped (28 557), source row
index shifted (61 952), destination column shifted (55 328), thread id halved
(30 976).

## Reproduce

    bash experiments/EXP-2026-11-13-435-p3-03-getrows-q4k/build_fixture.sh
    runtime/exp435/build/gpu-fixture
    python3 experiments/EXP-2026-11-13-435-p3-03-getrows-q4k/mutate.py
