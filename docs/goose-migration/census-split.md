# Where the GPU time actually goes, split by who runs it

Same trace as `census-full.md` (61 kernels, 8.36 s of kernel time), now attributed to the arm
that executes each. A kernel counts as Goose if its name is a Goose body (`fg*`) or if a module
now replaces that family in the active chain.

**52.8% of kernel time is Goose. 47.2% is still native, and 45% of that is three blocked
families.**

| share of total | launches | kernel still native |
|---|---:|---|
| 30.3% | 2448 | `mul_mat_q<` |
| 9.7% | 464 | `mul_mat_q<` |
| 3.4% | 2304 | `quantize_mmq_q8_1<` |
| 1.8% | 624 | `quantize_mmq_q8_1<` |
| 0.6% | 312 | `cutlass::Kernel2<cutlass_80_tensorop_s1688gemm_64x12` |
| 0.5% | 464 | `quantize_mmq_q8_1<` |
| 0.3% | 220 | `fwht_cuda<` |
| 0.3% | 220 | `fwht_cuda<` |
| 0.2% | 480 | `mul_mat_q<` |
| 0.0% | 480 | `mul_mat_q_stream_k_fixup<` |
| 0.0% | 336 | `k_get_rows_float<float, float>` |
| 0.0% | 39 | `mul_mat_f<float,` |

## Reading

- `mul_mat_q` at 30.3% + 9.7% = **40.0%** is the single largest item and is blocked on the
  emitter's address typing, with four measured attempts recorded.
- `quantize_mmq_q8_1` at 3.4% + 1.8% + 0.5% ≈ **5.7%** feeds MMQ and is blocked the same way.
- Everything else native is **1.5% in total** across 40+ kernels — CUTLASS internal gemms, FWHT
  instantiations, stream-k fixups. Individually negligible.
- 19.6% is already Goose attention (`fg175m_kernel`), 8.9% Goose GDN, 3.8% Goose activations.

## Caveat on provenance

This trace was recorded on the exp175 chain, so it predates EXP177/178/179/181. The attribution
above therefore credits those families to Goose by *replacement in the chain*, not by what the
trace itself observed — CPY, q8_1 quantize, MoE weighted reduction and mm_ids appear as native
in the raw numbers. That is the right way to read it for planning, but it is not a measurement of
the current runtime.

The two attempts to capture the current runtime are in `census-status.md`; both failed on the
runner's integrity check rather than on capture.

## What this means for the plan

The remaining work is not "many small kernels" — it is **one blocked family at 40%** plus a 6%
feeder for it. Closing the emitter's address typing would move the larger half; nothing else in
the remaining list is worth a separate experiment.
