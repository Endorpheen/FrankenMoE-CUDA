# Audit of the native remainder that is not blocked by codegen

After splitting the 61-kernel trace, everything still native fell into two groups: the three
blocked families, and a long tail. This checks the long tail item by item against the active
chain, because "small" is not the same as "unported".

| kernel | share | verdict |
|---|---:|---|
| `cutlass::Kernel2<…s1688gemm…>` | 0.65% | **not ours to port** — cuBLAS/CUTLASS internal, reached through the same external primitive the plan already permits |
| `fwht_cuda` (two instantiations) | 0.57% | covered by EXP183, `paths=0x1` |
| `k_get_rows_float<float,float>` | 0.01% | covered — the scalar path of EXP176; the witness `paths=0x3` shows both bits 0 and 1, so vector and scalar both executed |
| `mul_mat_f<float,…>` | 0.00% | covered by EXP184 on these shapes; the residual launches are shapes it declines by design |
| `dot_kernel`, `reduce_1Block_kernel` | 0.00% | cuBLAS internals, as above |
| `concat_cont<unsigned int,…>` | 0.00% | covered — EXP175's witness `paths=0x11` has bits 0 and 4 set, i.e. both the non-contiguous and the contiguous route ran |

## What the audit adds

Two things the earlier notes got wrong by omission:

1. **`concat_cont` is covered, not missing.** EXP175's witness was quoted as `paths=0x11` but the
   bits were never decoded. Bit 0 is the non-contiguous route and bit 4 the contiguous one, so the
   module handles both; the earlier "concat_non_cont is transferred" phrasing understated it.
2. **`k_get_rows_float` is the scalar path of EXP176, also covered.** `paths=0x3` is both bits, so
   the vector and scalar routes both execute as Goose.

With those, the long tail contributes **no unported work** except the CUTLASS/cuBLAS internals,
which the plan classifies as an audited external primitive rather than owned logic.

## Conclusion

The unported surface is exactly the blocked families: `mul_mat_q` 40.0%, `quantize_mmq_q8_1`
about 5.7%, and the declined matvec/matmul shapes. Nothing in the tail is waiting to be ported.
That makes the remaining work a codegen question, not a porting queue.
