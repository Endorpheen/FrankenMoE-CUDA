# EXP427: dense CUDA dispatch predicates in Goose -- closes P3-10

P3-10 had three parts: dense vector kernels, the cuBLAS/out-prod policy, and
the dense dispatch predicates. The first two were already evidenced by
EXP184/287/288 and EXP203. This experiment ports the two remaining predicates:
`ggml_cuda_should_use_mmf` and `ggml_cuda_should_use_mmvf`.

## What was ported

`source/dispatch.goose`:

- `mmf()`: quantized/type/stride guards, row-block divisibility, CDNA3 BF16
  refusal, the `mul_mat_id` batch bounds, the non-id `ne11 > 16` bound, RDNA3_0
  `> 8`, CDNA1/CDNA2 F16/BF16 refusal, and the F32/F16/BF16 capability return.
- `mmvf()`: the even-ne0 and 2*ts alignment guards, the `src0_small` rule,
  the NVIDIA Ampere/Turing/Ada/FP16-MMA thresholds, the AMD RDNA/FP16-MMA and
  RDNA3/RDNA4 thresholds, and the recursive MMF fallback for F16/BF16.

Device capability flags and type facts arrive as arguments; every comparison
and branch is Goose.

## Gate

The oracle is the **exported native function** in `libggml-cuda.so`, not a
transcription. The fixture links the real library and compares both predicates
on the same inputs.

| check | result |
|---|---|
| cases | **400 000** |
| failures | **0** |
| types | F32, F16, BF16, Q4_K, Q8_0 |
| device classes | NVIDIA 890/900, CDNA1/2/3, RDNA3/3.5/4 |
| shapes | ne0/ne1 random, ne11 1..513, aligned and misaligned nb0..nb3, `mul_mat_id` on/off |

The NVIDIA matrix is restricted to 890/900 because the native capability
helpers consult the build's highest compiled arch (sm_89); older cc values
would compare a different capability set, not a different predicate. That is a
property of the native oracle, not a gap in the port.

Mutations, all caught:

| mutation | failures |
|---|---:|
| `mmf` id threshold 512 -> 256 | 164 |
| `mmf` non-id bound 16 -> 15 | 145 |
| `mmvf` max batch 8 -> 7 | 1 096 |
| F32 Ampere `ne11 <= 3` -> `<= 2` | 108 |
| F16 RDNA4 `ne11 <= 5` -> `<= 4` | 16 |

## Integration and boundary

The predicates are host decision logic; they are gated and ready for the next
unified rebuild. They are not linked into the current shipped DSO, so this is
an OFF_PATH deliverable under the EXP245/246/247 rule.

The actual dense GEMM kernels are tensor-core `mma`/`ldmatrix` code and cuBLAS;
both are the primitive boundary declared in `APEX-FINAL-20261112.md`. The port
covers the owned dispatch policy, which is what P3-10 asks for; it does not
pretend to replace the tensor-core instructions or cuBLAS.

## P3-10 criterion check

| clause | evidence |
|---|---|
| dense vector kernels | EXP184 F32, EXP287 F16/BF16, bit-identical and mutation-gated |
| out-prod and cuBLAS policy | EXP288 pointer kernel and three-branch cuBLAS policy, 10/10 mutations |
| dispatch policy | EXP203 `should_use_mmq` plus this experiment's `should_use_mmf`/`_mmvf` |
| shape/precision coverage | fixture spans F32/F16/BF16/quantized, both predicates, id and non-id |
| external primitives | cuBLAS and `mma`/`ldmatrix` are the declared primitive boundary |

## Reproduce

    bash experiments/EXP-2026-11-13-427-p3-10-dispatch/build_fixture.sh
    runtime/exp427/build/dispatch-fixture
    python3 experiments/EXP-2026-11-13-427-p3-10-dispatch/mutate.py
