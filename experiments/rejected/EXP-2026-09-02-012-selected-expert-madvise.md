# EXP-2026-09-02-012: selected-expert `madvise`

- Status: `REJECTED`
- Raw results: `benchmarks/exp012-selected-expert-madvise.json`
- Patch: `experiments/rejected/EXP-2026-09-02-012-selected-expert-madvise.patch`

## Hypothesis

After CPU `MUL_MAT_ID` groups rows by selected expert, advising all selected mmap-backed expert slices with `POSIX_MADV_WILLNEED` could start later page-ins while the first selected expert is being computed.

## Implementation

The opt-in `LLAMA_CPU_MOE_PREFETCH=1` path ran on thread zero immediately before the worker barrier. It aligned each selected expert slice to page boundaries and issued `posix_madvise(..., POSIX_MADV_WILLNEED)`. The control arm did not set the environment variable.

## Protocol

Five interleaved control/prefetch pairs used the same Qwen3.8 UD-IQ3_XXS model, 64K context, 16 CPU threads, `EHS=0`, greedy decoding, and identical 256-token requests. Before every server start, all three model shards were evicted with `POSIX_FADV_DONTNEED`. Each server handled one cold request followed by one identical warm request.

## Results

| Metric | Control | Prefetch | Change |
|---|---:|---:|---:|
| Cold generation median | 11.744 tok/s | 10.827 tok/s | -7.81% |
| Cold generation range | 11.586-11.809 | 10.764-10.917 | — |
| Warm generation median | 17.596 tok/s | 15.473 tok/s | -12.07% |
| Warm generation range | 17.407-17.928 | 15.358-15.707 | — |
| Cold physical reads median | 26,052.0 MiB | 24,888.9 MiB | -4.46% |
| Cold major faults median | 266,237 | 182,945 | -31.28% |
| Cold peak RSS median | 29,475.6 MiB | 29,031.2 MiB | -1.51% |
| Process swap peak | 0 MiB | 0 MiB | unchanged |

Every request produced the same output SHA-256: `3be17501f0987ac5881160853f361a274ba40379a14f283b9fa13f61680c11e2`.

## Rejection reason

The kernel did less blocking I/O, but repeated advice in the hot compute path imposed enough syscall, bookkeeping, and competing-readahead overhead to reduce both cold and fully warm decode speed. The warm regression, where physical reads and major faults were already zero, confirms that the implementation overhead itself is material.

The experiment code was removed from the working runtime. Revisit only if expert reads can be collected into a small number of batches and scheduled earlier from router information or a dedicated asynchronous I/O pipeline.
