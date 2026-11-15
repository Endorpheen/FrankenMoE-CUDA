# EXP-2026-11-15-720 — every GPU tuning knob measured, none helps

Base: `0be713f3`. Rule 20 applied: 512 tokens, three warmed runs per arm, all four axes.

## 1. The profile first

Before touching anything, GPU utilisation was sampled during a 512-token decode: **96%**. That
settles where decode waits - it waits on the GPU, not the CPU. The CPU masks confirm it from the
other side: widening decode from 4 physical cores (`0f0f`) to 8 (`00ff`) gave +0.3%, and to all 16
(`ffff`) gave -1.3%, both inside the spread.

## 2. The arms

| arm | prefill | decode | range | vs baseline |
|---|---:|---:|---|---:|
| baseline(4352, no extra env) | 2429.9 | **39.86** | 39.83-40.22 | +0.0% |
| GGML_CUDA_GRAPH_OPT=1 | 2412.5 | **39.93** | 39.69-40.07 | +0.2% |
| GGML_CUDA_PDL=1 | 2410.6 | **39.84** | 39.65-40.13 | -0.1% |
| GGML_CUDA_MMQ_PREC=1 | 2408.6 | **40.05** | 39.42-40.10 | +0.5% |
| MMQ_PREC + GRAPH_OPT | 2406.5 | **39.19** | 39.09-39.59 | -1.7% |
| cpu-mask 00ff | 2417.7 | **39.94** | 39.90-40.22 | +0.2% |
| cpu-mask ffff | 2408.2 | **39.34** | 39.32-40.11 | -1.3% |

## 3. Reading it

**No GPU knob produced a gain outside the run-to-run spread.** The baseline's own three runs
already span 39.65-40.25, so a "gain" of 0.3% (mask 00ff) and a "loss" of 1.3% (mask ffff) are the
same measurement noise. The combination of the two individually-best settings was **worse** than
either alone (-1.6%), which is what noise looks like when you keep sampling.

That is a real result, not a null one: it means the knobs these binaries expose
(`GRAPH_OPT`, `PDL`, `MMQ_PREC`, CPU masks) are already at or near their best setting in the
baseline, and decode's remaining 4.5% gap to the reference is not reachable by configuration.

## 4. Where the time actually is

GPU at 96% during decode means the answer is in the kernels and in how many of them run, not in
scheduling. Two candidates remain, and neither is a flag:

1. **The quantised matmul kernels.** The reference ran them at 41.94 on the same GPU; we run at
   40.04. Same hardware, same cache size in the best case, different code generation on the
   llama/ggml side - and that side is the one we cannot swap (`LIBLLAMA-ABI.md`).
2. **Kernel count per token.** `FG203D_MMQ_DISPATCH` decides MMQ versus cuBLAS 51,704 times per
   request; every wrong choice on decode costs a launch. The decision is Goose's, so this is a
   place where Goose code could actually be improved - but only by measuring the decision quality,
   which needs cache hit/miss counters the build does not expose.

## 5. Configuration decision

Nothing changes. 4352 MiB, no extra environment, CPU masks as the reference profile has them.
The pinned configuration in `runtime/goose-stage/BEST-CONFIG.json` stands, and this card is the
evidence that its neighbourhood has been swept.

All 21 rows are in `benchmarks/experiment-history.jsonl`.
