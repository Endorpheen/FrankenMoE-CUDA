# EXP-2026-09-02-015: no-OpenMP threadpool

- Status: `REJECTED` (early gate)
- Summary: `benchmarks/exp015-no-openmp-threadpool.json`
- Source patch: none; this experiment changed only the separate build flag

## Hypothesis

`GGML_OPENMP=OFF` might replace the OpenMP graph executor with ggml's persistent native threadpool and remove enough of the spin/wait cost observed in EXP-014 to improve warm decode.

## Result

The immediately paired 12-thread warm requests measured `17.914 tok/s` with OpenMP and `17.544 tok/s` without it (`-2.06%`). CPU occupancy changed from `11.774` to `11.601` cores (`-1.47%`), while RSS, GPU utilization, physical reads, major faults, swap, and output hashes remained effectively identical.

The speed difference is within the paired noise seen in EXP-013, so it is not evidence of an intrinsic regression. It does show that the OFF build has no positive speed or resource signal worth extending to a long A/B.

The OFF build was not perf-profiled. Therefore, no claim is made that the `39.96%` libgomp sample share measured in EXP-014 maps one-for-one to ggml's native spin barrier.

## Decision

Keep `GGML_OPENMP=ON` in the working build and do not change the launcher or baseline. The separate `build/expert-tier-franken-no-openmp` directory remains ignored and can reproduce the build-flag arm.

The next diagnostic should measure per-worker useful dot-product time, chunk count, and barrier wait inside `MUL_MAT_ID`. Changing wait policy alone cannot shorten the slowest worker that gates each barrier.
