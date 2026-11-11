# EXP248: CUDA Q6_K dequantization in Goose

`dequantize_q6_K` (`dequantize.cuh:246-266`), the second dequantisation path after
EXP182 (Q4_K/F32) and EXP188 (Q4_0/F16), and a stated prerequisite for the MMQ
family (P3-08/P3-09).

## Verdict

**OFF_PATH** — bit-exact Q6_K dequantization to F32, but the fixed 32K/128 Apex
profile never calls `ggml_get_to_fp32_cuda(GGML_TYPE_Q6_K)`. The Q6_K tensors in
the Q4_K_M model execute through MMQ, not through the row selector. No performance
claim is made and the shipped DSO is unchanged.

## Scope transferred

`dequantize_q6_K<dst_t=float>` and the row contract in
`dequantize_row_q6_K_cuda`:

- device: the 64-thread decomposition `ip = tid/32`, `il = tid - 32*ip`,
  `is = 8*ip + il/16`; destination base `128*ip + il`; the super-block `d`; the
  low nibble of `ql` combined with the 2-bit high part from `qh`; the signed
  per-16 scale; the affine `d*sc*(q-32)` reconstruction. Four values per thread.
- host: `nb = k/QK_K`, grid `(nb,1,1)`, block 64. A `k` that is not a multiple
  of `QK_K` is rejected instead of silently dropping the tail.

Extern symbols are only raw byte loads, a signed-byte load, the half unpack and a
float store. No original kernel or C++ routine is called.

Layout used: `block_q6_K` is 210 bytes — `uint8_t ql[128]` at 0,
`uint8_t qh[64]` at 128, `int8_t scales[16]` at 192, `ggml_half d` at 208.
`QK_K = 256`.

## Gate

The oracle is the real `dequantize_q6_K` template compiled from the read-only
source tree into the same fixture. Both arms run on the GPU over random raw block
bytes, so every 6-bit pattern and every signed scale is exercised.

| check | result |
|---|---|
| `gpu-fixture` Goose vs native, 4 cases (1/4/64/256 blocks) | 12 checks / 0 failures |
| values compared bit-for-bit | 83 200 |
| every destination element written (canary) | 4 of 4 |
| `host-fixture` plan and rejections | 22 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| `qh` high-bit shift 2 -> 1 | 4 |
| scale index `+2` -> `+1` | 4 |
| affine constant `-32` -> `-31` (first value) | 4 |
| destination `128*ip` -> `127*ip` | 8 |
| scale group `il/16` -> `il/8` | 4 |

The last mutation first reported **0 failures** — and it was a defect in my
mutation script, not in the module: the anchor `il/16` also occurs in the module
comment, so `replace(..., 1)` edited the comment and left the code untouched. The
anchor was changed to the code form `8*ip+il/16`. This is the same class as the
handoff's "fixture generator does not span the trap" rule: when a mutation passes,
suspect the generator first.

## Not integrated

Same rule as EXP245/246/247: implementing and gating an unreachable kernel is a
deliverable; putting it into the shipped DSO is a liability, because a witness that
would prove the route works reads zero in this profile. The module is emitted and
gated, and stays outside the candidate link.

## Reproduce

    python3 emit_sources.py     # pinned compiler from runtime/compiler/current.json
    python3 test_local.py       # GPU differential + host plan gate
    python3 mutate.py           # 5 mutations, all must be caught

## Remaining scope

Q4_K/F16 and Q4_K/BF16 destinations, Q5_K, the IQ family and MXFP4 stay native, as
do the MMQ and MUL_MAT families themselves. MMQ remains blocked on the emitter's
address typing (EXP180: 824 SASS instructions against native's 160, 206 `IMAD`),
which is a compiler-side gap and cannot be closed from here.
