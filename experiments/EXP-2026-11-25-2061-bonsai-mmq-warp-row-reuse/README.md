# EXP-2026-11-25-2061 — Reuse Bonsai MMQ activation fragments across warp rows

**Not promoted.** Change the warp mapping inside the cached K64
64×128 tile. Each warp covers two 16-row groups and 32 tokens,
reusing each loaded activation fragment across the two row groups.
The original mapping covers one 16-row group and 64 tokens.
Per-output MMA order is unchanged.

All four real matrix fixtures match bitwise. Both kernels use 72
registers per thread, 27,648 B shared memory and no spills.

| RTX 4070 diagnostic, including common pack | Control, ms | Candidate, ms |
| --- | ---: | ---: |
| QKV tail / 129 | 0.602 | 0.598 |
| QKV / 1024 | 2.638 | 2.209 |
| FFN up / 1024 | 3.615 | 3.645 |
| FFN down / 1024 | 3.843 | 3.828 |

These are single CUDA trace samples. The test did not reach its
preregistered 5% FFN-down saving. The QKV control is slower than
some historical traces, so keep that observation for a future
targeted probe rather than promoting it as evidence of a stable gain.
The candidate was not run on the EXP-2059 cumulative path or in
the model server. EXP-2049 and other model artifacts are unchanged.

Run `source/build.sh OUTPUT EXP2049/pipeline.cl` and
`source/check.py FIXTURES INVENTORY OUTPUT/pipeline.cubin` with the
saved model fixtures. See [times and exactness](data/component.json).
