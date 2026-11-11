# EXP252: CUDA IQ4_NL dequantization in Goose

`dequantize_iq4_nl` (`dequantize.cuh:407-423`) plus the exact launch wrapper
from `convert.cu`, the first IQ-family path after the K-quants EXP182/188/248/
249/250/251.

## Verdict

**OFF_PATH** — bit-exact IQ4_NL dequantization to F32, but the fixed 32K/128
Apex profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_IQ4_NL)`. No
performance claim, shipped DSO unchanged.

## Scope transferred

- device: 32-thread decomposition `il = tid/8`, `ib = tid%8`; the row wrapper's
  guard index `8*i + tid%8` against `nb32`; source block `i*8 + ib`; destination
  base `i*QK_K + 32*ib + 4*il`; the nibble split; `d * kvalues_iq4nl[q]`.
- host: `nb32 = k/32`, `nb = ceil(k/256)`, grid `(nb,1,1)`, block 32, tail
  rejected when `k % 32 != 0`, `nb32` published for the kernel guard.

The 16-entry `kvalues_iq4nl` table is data, not code. It is reproduced as an
explicit constant function in Goose, so the module needs no native table symbol
and cannot silently drift from it:
`{-127,-104,-83,-65,-49,-35,-22,-10,1,13,25,38,53,69,89,113}`.

Layout used: `block_iq4_nl` is 18 bytes — `ggml_half d` at 0, `uint8_t qs[16]`
at 2. `QK4_NL = 32`, `QK_K = 256`.

## Defect in my own work, found by the gate

The first build wrote odd CTAs (256..511 and 768..1023) nowhere: those ranges
stayed `0xFFFFFFFF` while the even ranges were correct. Cause: the native
wrapper computes `ib = 8*i + threadIdx.x%8` **only for the nb32 guard**;
inside `dequantize_iq4_nl` the block index is `ib = tid%8` and the source is
addressed as `x = base + i*8 blocks`. I used the guard index in the destination
base `32*ib`, so odd CTAs wrote into the next CTA's region. Separating
`ibg` (guard) from `ib = tid%8` (destination/source block) fixed it.

This is the second layout-conflation defect in the dequantisation series
(EXP250's dm offset was the first), and both were caught by the native oracle
rather than by reasoning about the formula.

## Gate

The oracle is the real `dequantize_iq4_nl` template plus the exact wrapper
compiled from the read-only source into the same fixture.

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 5 cases (8/32/512/2048 and 7-block tail) | 15 checks / 0 failures |
| values compared bit-for-bit | 83 424 |
| every destination element written (canary) | 5 of 5 |
| `host-fixture` plan and rejections | 23 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| destination block index uses guard `ibg` instead of `tid%8` (the real defect) | 6 |
| destination `4*il` -> `2*il` | 10 |
| second-half offset `j+16` -> `j+8` | 10 |
| table last entry 113 -> 112 | 5 |
| high nibble reads the low nibble | 5 |

## Not integrated

Same rule as EXP245–251: emitted and gated, deliberately outside the candidate
link, because a witness for this route reads zero in the fixed profile.

## Reproduce

    python3 emit_sources.py
    python3 test_local.py
    python3 mutate.py

## Remaining scope

`iq4_xs`, then `iq1_s`/`iq1_m`, `iq2_xxs`/`iq2_xs`/`iq2_s`,
`iq3_xxs`/`iq3_s` and MXFP4 stay native, as do the non-F32 destinations and the
MMQ/MUL_MAT families. MMQ remains blocked on the emitter address typing recorded
in EXP180.
