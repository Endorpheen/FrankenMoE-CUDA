# EXP-2026-11-20-1563 — Preserve the routed MMQ input

Base: `d2caa54a337e9e22ca07fb577a3594c86557442e`.
The 384 MiB activation arena publishes every 64 MiB Apex expert
SwiGLU result before its down projection packs the input for MMQ.
The instrumented 4096/1 run has 40 arena-resident SwiGLU
publications and 40 device evictions. Input addresses are resolved
after reserving the matrix result, so the eviction is followed by
an upload of the same span.

`candidate.patch` redirects a complete large matrix result to the
existing inactive output buffer when the selected arena placement
would evict its input. It preserves the original placement on
CUDA out-of-memory and keeps the matrix input resident otherwise.
The patch is a deferred experiment.

## Profiled Apex 32K/512 A/B

RTX 4070, Apex-35B Q4_K_M, context 262,144, batch and ubatch
4096/4096, 384 MiB activation arena and 4,096 MiB host pin budget.
One sequential run per arm with the same executable and
`GOOSE_CUDA_PRESERVE_MATRIX_INPUT=0` for A, `=1` for B.
Both set `GOOSE_MMQ`, `GOOSE_GDN_MMA`, `GOOSE_FLASH_MMA`,
`GOOSE_DENSE_MMA`, `GOOSE_RMS_REFERENCE`,
`GOOSE_MMQ_Q6_REFERENCE`, `GOOSE_MMQ_Q4_REFERENCE`,
`GOOSE_SSM_REFERENCE`, `GOOSE_CUDA_ACTIVATION_REFERENCE`,
`GOOSE_DENSE_MMA_REFERENCE`, `GOOSE_MMVQ`, `GOOSE_FLASH_VEC`
and `GGML_CUDA_REGISTER_HOST` to 1. Request SHA-256:
`484a12de00ec057e4b78d480ebb185153bb10af607eb23192a72995ba3026d0f`.

| Metric | A | B |
|---|---:|---:|
| Prefill, tok/s | 694.731 | 773.585 |
| Prefill, s | 47.166 | 42.359 |
| Decode, steps/s (511 steps) | 16.403 | 16.355 |
| Decode, s | 31.153 | 31.244 |
| H2D, whole trace bytes | 176,385,876,304 | 154,911,039,824 |
| D2H, whole trace bytes | 22,025,992,960 | 551,156,480 |
| 64 MiB copies per direction | 320 | 0 |
| Process RSS high-water, KiB | 23,580,356 | 23,514,636 |
| Sampled peak process VRAM, MiB | 8,524 | 8,748 |
| Sampled peak whole-card VRAM, MiB | 10,786 | 11,011 |
| Process swap, KiB | 0 | 0 |
| System swap-out growth, pages | 0 | 6,286 |

Content, all 512 token IDs, probability rows and stop reason
match between the arms. The existing opening prompt echo and
substantive capacity analysis both persist. All 5,301,139 model
pages remain resident. SLRU totals are 560,324 hits, 77,707 fills
and 74,945 evictions in each arm. The short 4096/1 pair also
removes all 40 copies per direction and moves prefill from
694.653 to 764.795 tok/s. It raises process VRAM from 8,484 to
8,708 MiB, with zero process swap and system swap-out.

The full pair removes 21,474,836,480 bytes per direction and
saves 4.808 s of prefill. The additional 224 MiB peak process
VRAM fails the memory gate. System swap activity was observed
on the full candidate arm and its cause was not isolated.
Revisit when in-arena placement can protect the source without
a second large buffer. Check the same 64 MiB copy group, peak
VRAM and output on the full request.

Raw runs are under
`/home/end0/AI/LlamaServer/franken-goose-matrix-resident.LXe7Kg`.
The full A and B directories are `cuda-full-dense-both.asr7zyan`
and `cuda-full-dense-both.sqnepi58`. The short A and B directories
are `cuda-matrix-dense-both.n4cytrm8` and
`cuda-matrix-dense-both.kmf0s7l5`.
The diagnostic log is
`/home/end0/AI/LlamaServer/franken-goose-swiglu-diagnostic.AnU8ML/cuda-matrix-dense-both.uvle4pw2/nsys.log`.

| Artifact | SHA-256 |
|---|---|
| Executable | `5547b30af240a150189fe0f9f326f2a6a5e5a53b222a5d776693a7ca23b91199` |
| Full A Nsight | `47a198b2200f22c474863fb94b9baaa017df63a5e71d69a61c215368f3247902` |
| Full B Nsight | `2d2c561204a9bf0d764ee5691d96abf7c44ef5951930b7798bf19ff3c21cd3bb` |
| Full A response | `e8cc67447dc5b3686ddfa92db05690e5958a2905942a11a20cee0434fa32610f` |
| Full B response | `be458c131a378681c3aa004c18c74242d4d3194e12d2e6cbab0107e9bfdff07d` |
