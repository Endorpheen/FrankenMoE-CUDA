# EXP186: CUDA mul_mat_vec_q for Q6_K in Goose — REJECTED

The transfer is functionally correct and byte-exact, but it costs 3.66% of decode
throughput, so it is **not promoted**. Full evidence, measurement and diagnosis:
see [BLOCKER.md](./BLOCKER.md).

Summary:

- 4 of 4 outputs byte-identical to native across ncols 256/1024/2048(two-channel)/4096
- memcheck 0 errors, model witness present, Apex content sha256 identical (`e68df05b`)
- paired Apex decode 26.89 → 25.91 median, **−3.66%**, prefill flat (−0.06%)
- measured cause: kernel 21.82 us vs 17.41 us per launch (+25%), SASS 536 vs 208
  instructions, dominated by recomputed address arithmetic

The active runtime is unchanged and stays on EXP185. This directory is a diagnostic
checkpoint so the module can be re-measured once the codegen gap is closed.
