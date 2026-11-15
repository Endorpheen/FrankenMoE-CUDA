# How much Goose actually executes in the working Apex path

Measured from the server log of a real 32K/512 request on the pinned configuration
(`runtime/goose-stage/BEST-CONFIG.json`). Not estimated, not counted in modules.

## The numbers

**45 distinct Goose modules executed. 9,427,734 decisions.**

| module | decisions | share |
|---|---:|---:|
| FG398S batch container layout | 3,174,000 | 33.7% |
| FG213P piece copy | 1,063,513 | 11.3% |
| FG401S detok decisions | 1,063,000 | 11.3% |
| FG228E KV cell extent | 793,584 | 8.4% |
| FG209I KV seqpos | 793,584 | 8.4% |
| FG147 backend | 571,748 | 6.1% |
| FG217C unicode code points | 511,100 | 5.4% |
| FG387S sampler chain | 475,600 | 5.0% |

## The CUDA path specifically

`FG203D_MMQ_DISPATCH` executed **51,704 times**. That module decides whether a quantised matmul
takes the MMQ kernel or falls back to cuBLAS / `mul_mat_vec_q`, and the module's own header records
that this decision covers **39.5% of Apex kernel time**. So Goose is not absent from the GPU path -
it makes the dispatch decision that governs the majority of kernel time.

What Goose does not do there is the arithmetic: the kernels themselves are still ggml-cuda.
The decision is ours, the multiply is not.

## What this number does and does not mean

**It means** the integrated runtime is not "llama.cpp with a Goose token glued on". Nearly ten
million decisions per request come from composed Goose code, in batch layout, KV addressing,
detokenisation, sampling and CUDA dispatch.

**It does not mean** the runtime is 100% Goose. The remaining work is arithmetic and transport:
the matmul kernels, the tokenizer engine, the HTTP server, and the full 41-layer execution in the
standalone. Those are the four open items in `tasks.json`.

Reporting decision counts as coverage would be exactly the substitution rule 15 forbids, so the
distinction is stated here rather than left implicit: **decisions are migrated, compute is not.**

## Why record it at all

Because "how much Goose is left" has been argued twice from module counts, which measure the wrong
thing. This is the count of things that actually ran during a real request, which is checkable
against the log and reproducible from the pinned configuration.
